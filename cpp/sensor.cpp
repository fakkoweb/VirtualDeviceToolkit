#include "sensor.h"
#include "functions.h"  //for average and variance generic functions
#include <math.h>



namespace sdrf
{

	////////////////////////////
	//GENERIC SENSOR PROCEDURES

	Sensor::Sensor(const int sample_rate, const int avg_interval, const bool enable_autorefresh, const bool enable_mean_offset) : MeanGuy(enable_mean_offset)
	{

		//Convert avg_interval in a valid chrono type
		if (avg_interval <= 0) statistic_delay = std::chrono::duration< int, std::milli >::zero();
		else statistic_delay = std::chrono::seconds(avg_interval * 60);

		//Convert sample_rate in a valid chrono type
		if (sample_rate <= 0) sample_delay = std::chrono::duration< int, std::milli >::zero();
		else sample_delay = std::chrono::milliseconds(sample_rate);

		board = NULL;
		raw_measure = 0;

		statistic.average = 0;
		statistic.variance = 0;
		statistic.is_valid = false;		//to be sure it will be set to true!
		statistic.percentage_validity = 0;
		statistic.total_samples = 0;
		statistic.expected_samples = 0;
		statistic.valid_samples = 0;

		autorefresh = enable_autorefresh;
		close_thread = false;
		r = NULL;

	}


	Sensor::~Sensor()
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


	//Like calling destructor + constructor without creating a new object
	void Sensor::reset()
	{
		std::unique_lock<std::mutex> access(rw, std::defer_lock);

		std::cout << " | Sto resettando il sensore..." << std::endl;

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

		board = NULL;
		raw_measure = 0;

		statistic.average = 0;
		statistic.variance = 0;
		statistic.is_valid = false;		//to be sure it will be set to true!
		statistic.percentage_validity = 0;
		statistic.total_samples = 0;
		statistic.expected_samples = 0;
		statistic.valid_samples = 0;

		close_thread = false;
		r = NULL;


		std::cerr << "  S| Buffer e variabili resettate." << std::endl;

		std::cerr << "  S| Buffer rigenerati." << std::endl;

		std::cout << " | Reset del sensore completato." << std::endl;
	}

	void Sensor::refresh()		//This function is called manually or automatically, in which case all sampling operation must be ATOMICAL 
	{
		std::unique_lock<std::mutex> access(rw, std::defer_lock);
		bool thread_must_exit = false;    	//JUST A COPY of close_thread (for evaluating it outside the lock)

		int num_expected_samples = statistic_delay.count() / sample_delay.count(); 	//An ESTIMATION of number of expected samples to be picked from Driver
		int num_total_samples = 0;						//Number of total samples (both VALID and INVALID) picked from Driver
		int num_considered_samples = 0;					//Number of VALID samples considered by statistic

		//Time structures for jitter/delay removal
		std::chrono::steady_clock::time_point refresh_start = std::chrono::system_clock::now();
		std::chrono::steady_clock::time_point refresh_end;
		std::chrono::duration< int, std::micro > wakeup_jitter = std::chrono::duration< int, std::micro >::zero();
		std::chrono::duration< int, std::micro > computed_sleep_delay = std::chrono::duration< int, std::micro >::zero();

		do
		{

			if (autorefresh)
			{
				access.lock();		//ALL sampling operation by thread should be ATOMICAL. So we put locks here (just for autorefreshing thread)  
				//and not put locks on elementary operations on buffers (it would cause ricursive locks!)
				//cout<<"Thread is alive!"<<endl;

				//Is thread been asked to be closed?
				thread_must_exit = close_thread;
			}

			if (!thread_must_exit)
			{


				//MEAN AND VARIANCE
				//If avg_interval minutes (alias "chrono::seconds statistic_delay") have passed since last Mean&Variance request, COMPUTE STATISTIC and RESET (before sampling again!)
				if (std::chrono::system_clock::now() > (last_statistic_request + statistic_delay))
				{
					//Assign to statistic latest Mean and Variance calculated by MeanGuy
					std::cerr << " S| Media pronta e richiesta!" << std::endl;
					statistic.average = MeanGuy.getMean();
					statistic.variance = MeanGuy.getVariance();

					//Get number of valid samples considered in statistic calculation
					num_considered_samples = MeanGuy.getSampleNumber();

					//DEBUG: Assign to statistic number of expected samples (constant for this Sensor and therefore not necessary)
					statistic.expected_samples = num_expected_samples;
					//DEBUG: Assign to statistic also number of total samples, valid and invalid, taken from driver
					statistic.total_samples = num_total_samples;
					//DEBUG: Assign to statistic also number of valid samples taken from driver
					statistic.valid_samples = num_considered_samples;

					//Estimate STATISTIC VALIDIY:
					// comparing number of EXPECTED samples with numer of VALID samples picked from Driver will consider clock imprecision AND errors from Driver
					// WARNING: VALID samples can be MORE than EXPECTED samples due to clock imprecisions, resulting in validity higher than 100%: it's ok, we have more samples!
					if (statistic.total_samples>0) statistic.percentage_validity = (num_considered_samples * 100) / num_expected_samples;
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
					num_considered_samples = 0;

					//Notify that new statistics are now available
					new_statistic.notify_all();

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



				//Notice about Raw measure Conversion
				//	Sensor does not store the converted measure since a specific "convert()" pure virtual
				//	method is used on-the-go, whenever real measure is needed:
				//	- when user asks for a converted measure (see "get_measure()")
				//	- when measure is passed to MeanGuy for mean and variance calculation (see below)
				//	EACH SUBCLASS EXTENDING SENSOR MUST IMPLEMENT ITS VERSION OF "convert()"!!


				//GET NEW SAMPLE & validity check
				std::cerr << " S| Nuova misura di " << stype() << " richiesta al driver (" << (size_t)board << ")" << std::endl;
				raw_measure = sample();	//request new sample from driver
				if (raw_measure != SDRF_VALUE_INVALID)	//ONLY IF MEASURE IS VALID..
				{
					MeanGuy.add(convert(raw_measure));	//..give MeanGuy next converted measure for on-line calculation
					//MeanGuy is responsible of taking COUNT of VALID samples
					std::cerr << " S| Richiesta misura di " << stype() << " soddisfatta." << std::endl;
				}
				num_total_samples++;	//then increment COUNT of TOTAL samples


				//Notify that a new sample is now available
				new_sample.notify_all();



			}



			if (autorefresh)
			{
				access.unlock();

				//Thread should wait "refresh_rate" milliseconds if is not closing
				if (!thread_must_exit)
				{
					//JITTER AND DELAY CALCULATION AND REMOVAL!
					//-----------------------------------------
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
			}

		} while (autorefresh && !thread_must_exit);   //If there is no thread autorefreshing, there must be no loop

		return;


	}

	void Sensor::wait_new_sample()
	{
		std::unique_lock<std::mutex> access(rw);
		if (autorefresh) new_sample.wait(access);
	}

	void Sensor::wait_new_statistic()
	{
		std::unique_lock<std::mutex> access(rw);
		if (autorefresh) new_statistic.wait(access);
	}



	uint16_t Sensor::get_raw()
	{
		uint16_t measure = 0;
		std::lock_guard<std::mutex> access(rw);
		if (board != NULL)
		{
			if (!autorefresh)
			{
				refresh();         //on demand refresh if autorefresh is FALSE
			}
			measure = raw_measure;
		}
		else std::cerr << " S| Attenzione: nessuna device allacciata al sensore.\n | Usare il metodo plug_to() per associare." << std::endl;
		return measure;
	}

	void Sensor::plug_to(const Driver<measure_struct, uint16_t>& new_board, const std::chrono::system_clock::time_point& start_time)
	{

		//Reset board and also the thread if present
		if (board != NULL) reset();

		//Set new board
		board = const_cast< Driver<measure_struct, uint16_t>* > (&new_board);


		//---------------------------------------------------------------
		//Set new last_statistic_request initialization: is done only when driver is first attached!! So that:
		// - if autorefresh is true, thread will start calculating on-line average and WILL RESET IT as soon as "statistic_delay" minutes have passed since start_time
		// - if autorefresh is false, average is still calculated but it makes sense only if get_measure() or get_raw_measure() are called periodically by user
		last_statistic_request = start_time;


		//Start a new autosampling thread
		if (autorefresh == true)
		{
			//CALL OF REFRESH THREAD - Avvia il thread per l'autosampling
			r = new std::thread(&Sensor::refresh, this);		// Per eseguire refresh() è richiesto this quando è un metodo della classe stessa
			//wait_new_sample();     			// Aspettiamo che il thread abbia almeno calcolato il primo sample() per considerare il sensore "collegato"   	
		}


	}



}

