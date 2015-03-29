#include "VProbe.h"
#include "utils/functions.h"  //for average and variance generic functions
#include <math.h>



namespace vdt
{

	//////////////////////////////
	//GENERIC SENSOR CONSTRUCTORS

	//Default constructor (no zero argument constructor is allowed)
	template <class raw_elem_type, class refined_elem_type>
	VProbe<raw_elem_type, refined_elem_type>::VProbe
		(
		const polling_policy_t selected_polling,
		const processing_policy_t selected_processing,	//if "none" or "custom" following parameters are ignored!
		const unsigned int mean_interval = 1,			//avg_interval = minutes after which online average is periodically reset
														//if not set, default interval is 1 minute
		const bool enable_mean_offset = false			//offset = the minimum value is calculated in the interval and used as offset to add to mean
														//this allows error removal on peripherals that accumulate skew over time (for example dust on a dust sensor!)
		) : polling_mode(selected_polling), processing_mode(selected_processing), MeanGuy(enable_mean_offset)
	{
		//Convert avg_interval in a valid chrono type
		statistic_delay = std::chrono::seconds(mean_interval * 60);
		this.reset();
	}

	//Force sample_rate constructor (dynamic_sample_rate is set to false)
	template <class raw_elem_type, class refined_elem_type>
	VProbe<raw_elem_type, refined_elem_type>::VProbe
		(	
		const polling_policy_t selected_polling,
		const unsigned int sample_rate,					//force a specific value for sample_rate (default = dynamically set to the minimum of attached device)
		const processing_policy_t selected_processing,	//if "none" or "custom" following parameters are ignored!
		const unsigned int mean_interval = 1,			//avg_interval = minutes after which online average is periodically reset
														//if not set, default interval is 1 minute
		const bool enable_mean_offset = false			//offset = the minimum value is calculated in the interval and used as offset to add to mean
														//this allows error removal on peripherals that accumulate skew over time (for example dust on a dust sensor!)
		) : dynamic_sample_rate(false)
	{
		//Call default constructor
		VProbe(selected_polling, selected_processing, mean_interval, enable_mean_offset);

		//Convert sample_rate in a valid chrono type
		if (sample_rate == 0) sample_delay = std::chrono::duration< int, std::milli >::zero();
		else sample_delay = std::chrono::milliseconds(sample_rate);
	}

	//Destructor: only close autosampling thread to close
	template <class raw_elem_type, class refined_elem_type>
	VProbe<raw_elem_type, refined_elem_type>::~VProbe()
	{
		closeSamplingThread();
	}

	//Reset: like calling destructor + constructor without creating a new object, but:
	//	- sample_rate is not reset
	//	- mean_interval is not reset
	//	- policy and processing are not modified
	template <class raw_elem_type, class refined_elem_type>
	void VProbe<raw_elem_type, refined_elem_type>::reset()
	{
		std::unique_lock<std::mutex> access(rw, std::defer_lock);

		std::cout << " | Sto resettando il sensore..." << std::endl;

		//Closes current autosampling thread, if any
		closeSamplingThread();

		//Reset local variables
		board = NULL;
		raw_measure = 0;
		num_total_samples = 0;				//Number of total samples (both VALID and INVALID) picked from VDevice
		close_thread = false;
		r = NULL;

		//Initialization of statistic struct possibly returned to the user
		statistic.average = 0;
		statistic.variance = 0;
		statistic.is_valid = false;		//to be sure it will be set to true!
		statistic.percentage_validity = 0;
		statistic.total_samples = 0;		//num_total_samples will be assigned to it
		statistic.expected_samples = statistic_delay.count() / sample_delay.count(); 	//An ESTIMATION of number of expected samples to be picked from VDevice;
		statistic.valid_samples = 0;		//Number of VALID samples actually considered in statistic calculation

		std::cerr << "  S| Buffer e variabili resettate." << std::endl;
		std::cerr << "  S| Buffer rigenerati." << std::endl;
		std::cout << " | Reset del sensore completato." << std::endl;
	}



	/////////////////////////
	//GENERIC SENSOR MEMBERS


	//Close thread routine (thread can be closed if manually started by user or automatically at destructor)
	template <class raw_elem_type, class refined_elem_type>
	void VProbe<raw_elem_type, refined_elem_type>::closeSamplingThread()
	{
		std::unique_lock<std::mutex> access(rw, std::defer_lock);

		//closes current autosampling thread, if any
		if (r != NULL)
		{
			access.lock();
			close_thread = true;
			access.unlock();
			std::cerr << "  S| Chiusura thread embedded richiesta..." << std::endl;
			r->join();
			std::cerr << "  S| Chiusura thread embedded completata." << std::endl;
		}

	}

	template <class raw_elem_type, class refined_elem_type>
	void VProbe<raw_elem_type, refined_elem_type>::refresh()		//This function is called manually or automatically, in which case all sampling operation must be ATOMICAL 
	{
		std::unique_lock<std::mutex> access(rw, std::defer_lock);
		bool thread_must_exit = false;    	//JUST A COPY of close_thread (for evaluating it outside the lock - in the while condition)

		//Time structures for jitter/delay removal
		std::chrono::steady_clock::time_point refresh_start = std::chrono::system_clock::now();
		std::chrono::steady_clock::time_point refresh_end;
		std::chrono::duration< int, std::micro > wakeup_jitter = std::chrono::duration< int, std::micro >::zero();
		std::chrono::duration< int, std::micro > computed_sleep_delay = std::chrono::duration< int, std::micro >::zero();

		do
		{
			// LOCK
			access.lock();		//ALL sampling operation by thread should be ATOMICAL. So we put locks here (just for autorefreshing thread)  
			//and not put locks on elementary operations on buffers (it would cause ricursive locks!)
			//cout<<"Thread is alive!"<<endl;
	
			// PUBLISH MEAN AND VARIANCE UNTIL NOW
			//If avg_interval minutes (alias "chrono::seconds statistic_delay") have passed since last Mean&Variance request, COMPUTE STATISTIC and RESET (before sampling again!)
			if (std::chrono::system_clock::now() > (last_statistic_request + statistic_delay))
			{
				publish_statistic();

				//FORCE LOGIC TIME SYNC: COMPUTED CLOCK REFERENCE
				//Since real parallelism is not always possible and therefore not guaranteed, it is bad to set time on our own
				//THIS:
				//
				//	last_statistic_request = std::chrono::system_clock::now();
				//
				//IS WRONG since it would set a new time in this instant: now() WON'T BE PRECISELY last_statistic_request + statistic_delay (what user wants), BUT MORE (SO overhead)!!
				//Since small delays sum on time, it is better to state that new last request happened at time last_statistic_request + statistic_delay:
				last_statistic_request = last_statistic_request + statistic_delay;
				//this way, the sensor is FORCED to be ALWAYS ON TIME in respect to start_time set in PLUGGING PHASE, so we have ALWAYS PRECISE CLOCK REFERENCE!!
			}

			// GET AND PUBLISH CURRENT SAMPLE + UPDATE MEAN AND VARIANCE
			//Notice about Raw measure Conversion
			//	VProbe does not store the converted measure since a specific "convert()" pure virtual
			//	method is used on-the-go, whenever real measure is needed:
			//	- when user asks for a converted measure (see "get_measure()")
			//	- when measure is passed to MeanGuy for mean and variance calculation (see below)
			//	EACH SUBCLASS EXTENDING SENSOR MUST IMPLEMENT ITS VERSION OF "convert()"!!
			publish_measure();

			// WHILE EXIT CONDITION: has thread been asked to end?
			thread_must_exit = close_thread;

			// UNLOCK
			access.unlock();

			//JITTER + DELAY CALCULATION AND REMOVAL!
			//-----------------------------------------
			//Thread should wait "refresh_rate" milliseconds if close_thread == FALSE
			if (!thread_must_exit)
			{
				//END: save now() as the time loop ended
				refresh_end = std::chrono::steady_clock::now();
				//cout<< "computation time delay: "<<std::chrono::duration_cast<std::chrono::microseconds>(refresh_end - refresh_start).count()<<endl;			
				//SLEEP for sample_delay, reduced by wakeup_jitter and computation time this loop took
				computed_sleep_delay = sample_delay - wakeup_jitter - std::chrono::duration_cast<std::chrono::microseconds>(refresh_end - refresh_start);
				//cout<< "new nominal wake-up delay: "<<std::chrono::duration_cast<std::chrono::microseconds>(computed_sleep_delay).count()<<endl;
				//cout<<"------"<<endl;
				std::this_thread::sleep_for(computed_sleep_delay);
				//BEGIN: save now() as the time loop begins
				refresh_start = std::chrono::steady_clock::now();
				//compute the jitter as the time it took this thread to wake-up since the time it had to wake up (in ideal world, wake_jitter would be 0...)
				wakeup_jitter = std::chrono::duration_cast<std::chrono::microseconds>(refresh_start - refresh_end) - computed_sleep_delay;
				//cout<<"------"<<endl;
				//cout<< "nominal wake-up delay was: "<<std::chrono::duration_cast<std::chrono::microseconds>(computed_sleep_delay).count()<<endl;
				//cout<< "real wake-up delay: "<<std::chrono::duration_cast<std::chrono::microseconds>(refresh_start - refresh_end).count()<<endl;
				//cout<< "there was a wakeup jitter of : "<<std::chrono::duration_cast<std::chrono::microseconds>(wakeup_jitter).count()<<endl;
			}
			

		} while (!thread_must_exit); 

		return;


	}

	template <class raw_elem_type, class refined_elem_type>
	void VProbe<raw_elem_type, refined_elem_type>::publish_measure()
	{
		//GET NEW SAMPLE & validity check
		std::cerr << " S| Nuova misura di " << stype() << " richiesta al driver (" << (size_t)board << ")" << std::endl;
		raw_measure = sample();					//request new sample from driver
		if (raw_measure != SDRF_VALUE_INVALID)	//ONLY IF MEASURE IS VALID..
		{
			update_statistics();					//update current statistic
			std::cerr << " S| Richiesta misura di " << stype() << " soddisfatta." << std::endl;
		}

		//increment COUNT of TOTAL samples
		num_total_samples++;

		//Notify that a new sample is now available
		measureProvisioning.notifyNewUpdate();
	}

	template <class raw_elem_type, class refined_elem_type>
	void VProbe<raw_elem_type, refined_elem_type>::publish_statistic()
	{
		//Assign to statistic latest Mean and Variance calculated by MeanGuy
		std::cerr << " S| Media pronta e richiesta!" << std::endl;
		statistic.average = MeanGuy.getMean();
		statistic.variance = MeanGuy.getVariance();

		//DEBUG: Assign to statistic number of expected samples (constant for this VProbe and therefore not necessary)
		// statistic.expected_samples = statistic_delay.count() / sample_delay.count();		//ALREADY DONE ONCE in constructor (it is constant value)
		//DEBUG: Assign to statistic also number of total samples, valid and invalid, taken from driver
		statistic.total_samples = num_total_samples;
		//DEBUG: Assign to statistic also number of valid samples taken from driver
		statistic.valid_samples = MeanGuy.getSampleNumber();

		//Estimate STATISTIC VALIDIY:
		// comparing number of EXPECTED samples with numer of VALID samples picked from VDevice will consider clock imprecision AND errors from VDevice
		// WARNING: VALID samples can be MORE than EXPECTED samples due to clock imprecisions, resulting in validity higher than 100%: it's ok, we have more samples!
		if (statistic.total_samples>0) statistic.percentage_validity = (statistic.valid_samples * 100) / statistic.expected_samples;
		else statistic.percentage_validity = 0;
		//Finally declare how to consider this statistic, in respect to a tolerance threshold
		if (statistic.percentage_validity>THRESHOLD)
		{
			statistic.is_valid = true;
		}
		else
		{
			statistic.is_valid = false;
		}

		//Reset MeanGuy and number of total samples
		MeanGuy.reset();
		num_total_samples = 0;

		//Notify all that new statistics are now available
		statisticProvisioning.notifyNewUpdate();
	}

	template <class raw_elem_type, class refined_elem_type>
	void VProbe<raw_elem_type, refined_elem_type>::update_statistic()
	{
		MeanGuy.add(convert(raw_measure));	//..give MeanGuy next converted measure for on-line calculation

		//MeanGuy is responsible of keeping COUNT of VALID samples
	}

	template <class raw_elem_type, class refined_elem_type>
	void VProbe<raw_elem_type, refined_elem_type>::wait_new_sample()
	{
		if (polling_mode == automatic) measureProvisioning.waitForNextUpdate();
	}

	template <class raw_elem_type, class refined_elem_type>
	void VProbe<raw_elem_type, refined_elem_type>::wait_new_statistic()
	{
		statisticProvisioning.waitForNextUpdate();
	}

	template <class raw_elem_type, class refined_elem_type>
	bool VProbe<raw_elem_type, refined_elem_type>::new_sample_is_ready()
	{
		return measureProvisioning.checkForUpdates();
	}

	template <class raw_elem_type, class refined_elem_type>
	bool VProbe<raw_elem_type, refined_elem_type>::new_statistic_is_ready()
	{
		return statisticProvisioning.checkForUpdates();
	}

	template <class raw_elem_type, class refined_elem_type>
	raw_elem_type VProbe<raw_elem_type, refined_elem_type>::get_raw(sync_policy_t sync_mode)
	{
		raw_elem_type measure = SDRF_VALUE_INVALID;
		std::lock_guard<std::mutex> access(rw);
		if (board != NULL && polling_mode == manual)
		{
			switch (sync_mode)
			{
			case sync:		//the caller thread executes refresh() and waits for its end
				refresh();
				break;
			case async:		//the caller starts a new thread that executes refresh() only once, user may wait or check its termination
				close_thread = true;
				std::thread t(&VProbe::refresh, this);
				t.detach();
				measureProvisioning.subscribe(new_only);
				break;
			default:
				break;
			}

			measure = raw_measure;
		}
		else std::cerr << " S| Attenzione: nessuna device allacciata al sensore.\n | Usare il metodo plug_to() per associare." << std::endl;
		return measure;
	}

	template <class raw_elem_type, class refined_elem_type>
	void VProbe<raw_elem_type, refined_elem_type>::plug_to(const VDevice<void, raw_elem_type>& new_board, const std::chrono::system_clock::time_point& start_time)
	{
		std::lock_guard<std::mutex> access(rw);

		//Reset board and also the thread if present
		if (board != NULL) reset();

		//Set new board
		board = const_cast< VDevice<measure_struct, raw_elem_type>* > (&new_board);

		//Get sample_rate from current device and convert it in a valid chrono type
		if (board != nullptr) sample_delay = std::chrono::milliseconds(board->getMinDelay());


		//---------------------------------------------------------------
		//Set new last_statistic_request initialization: is done only when driver is first attached!! So that:
		// - if autorefresh is true, thread will start calculating on-line average and WILL RESET IT as soon as "statistic_delay" minutes have passed since start_time
		// - if autorefresh is false, average is still calculated but it makes sense only if get_measure() or get_raw_measure() are called periodically by user
		last_statistic_request = start_time;


		//Start a new autosampling thread
		if (polling_mode == automatic)
		{
			//CALL OF REFRESH THREAD - Avvia il thread per l'autosampling
			close_thread = false;
			r = new std::thread(&VProbe::refresh, this);		// Per eseguire refresh() è richiesto this quando è un metodo della classe stessa

			//wait_new_sample();     			// Aspettiamo che il thread abbia almeno calcolato il primo sample() per considerare il sensore "collegato"   	
		}


	}



}

