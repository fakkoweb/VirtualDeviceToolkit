#ifndef _VDT_IPROBE_H_
#define _VDT_IPROBE_H_
////////////////////


//Include user configuration
#include "config.h"

//Internal dependencies
#include "IInputDevice.hpp"
#include "utils/OMV.h"
#include "utils/UpdateService.h"
#include "utils/functions.h"  //for average and variance generic functions
#include <math.h>

//Standard included libraries
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <sstream>
#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/ipc.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <sys/shm.h>
    #include <unistd.h>
    #include <string.h>
#endif
#ifdef HOST_THREADS
	#include <mutex>
	#include <thread>
	#include <condition_variable>
	#include <future>

#endif

namespace vdt
{

	enum polling_policy_t
	{
		manual,
		automatic
	};

	enum processing_policy_t
	{
		none,
		custom,
		online_mean_var
	};

	enum sync_policy_t
	{
		sync,
		async
	};

	typedef struct _STATISTIC_STRUCT
	{
		double average;
		double variance;
		bool is_valid;
		double percentage_validity;
		unsigned int expected_samples;
		unsigned int total_samples;
		unsigned int valid_samples;
	} statistic_struct;

	template <class raw_elem_type, class refined_elem_type>
	class IProbe							//ABSTRACT CLASS: only sub-classes can be instantiated!
	{

	protected:

		//BUFFERING VARIABLES
		raw_elem_type raw_measure;
		raw_elem_type refined_measure;
		//OLD:	double format_measure;		//Memorizza una versione convertita di raw_buffer - non più necessaria, ora la misura è convertita su richiesta
		statistic_struct statistic;
		unsigned int num_total_samples;
		bool dynamic_sample_rate = true;
		polling_policy_t polling_mode;
		//sync_policy_t sync_mode;
		processing_policy_t processing_mode;


		//AVERAGING AND VARIANCE CALCULATION		//asdfg
//		utils::OMV MeanGuy;							//Classe per il calcolo della media on-line (Knuth/Welford algorithm) --> vedi lista di inizializzazione del costruttore!

		std::chrono::duration< int, std::milli > statistic_delay;	//GESTIONE STATISTIC INTERVAL: Ogni sample() del sensore viene calcolata una nuova media e varianza,
		//basate sul valore corrente e sulla storia precedente. Ad ogni refresh(), il sensore confronta
		//statistic_delay (ovvero "avg_interval" minuti) con il tempo trascorso da last_statistic_request
		//e quando occorre preleva le statistiche da MeanGuy e lo resetta prima di ulteriori sample().
		std::chrono::steady_clock::time_point last_statistic_request;   //last_statistic_request è l'UNICA ANCORA/RIFERIMENTO TEMPORALE che IProbe ha con l'esterno!!
		//Ad ogni statistic_delay, a last_statistic_request è sommato statistic_delay.
		//HO UN RIFERIMENTO TEMPORALE ASSOULUTO, FISSATO IN PLUGGING PHASE (init. a "start_time")!

		std::chrono::duration< int, std::milli > sample_delay;		//GESTIONE REFRESH RATE: sample_delay (ovvero "sample_rate" ms) è solo il tempo ideale di sampling!
		//IMPOSTATO A SECONDA DEL TIPO DI SENSORE!! Inizializzato da costruttore a "sample_rate"
		//UTILE SOLO SE autorefresh E' TRUE!!
		//La funzione refresh() terrà conto sia del ritardo di computazione che di wake-up dello scheduler, riducendo
		//il tempo di sleep del thread di autorefresh.
		//MINIMIZZO IL JITTER!



		//SAMPLING & CONVERSION & MEAN CALCULATION
		virtual int mtype() = 0;					//returns the type (its code) of measure sensor requests to driver
		raw_elem_type sample(){ return board->request(mtype()); };	//Chiamata da get_measure, semplicemente chiama board (la request() col tipo misura richiesto)
		virtual refined_elem_type convert(const raw_elem_type&) = 0;       	//THIS FUNCTIONS MUST BE SPECIALIZED BY INHERITING CLASSES
		
		/*virtual void update_statistic()
		{
			MeanGuy.add(convert(raw_measure));	//..give MeanGuy next converted measure for on-line calculation

			//MeanGuy is responsible of keeping COUNT of VALID samples
		}*/
		virtual void publish_measure()
		{
			//GET NEW SAMPLE & validity check
			std::cerr << " S| Nuova misura di " << stype() << " richiesta al driver (" << (size_t)board << ")" << std::endl;
			raw_measure = sample();					//request new sample from driver
			if (true || board->isValid(raw_measure))		//ONLY IF MEASURE IS VALID..
			{
				std::cerr << " S| Richiesta misura di " << stype() << " soddisfatta." << std::endl;
				switch (processing_mode)
				{
					case none:
						break;
					case online_mean_var:
//						update_statistics();					//update current statistic
						break;
					case custom:
						refined_measure = convert(raw_measure);
						break;
					default:
						break;
				}
			
			}
			else
			{
				std::cerr << " S| Richiesta misura di " << stype() << " non soddisfatta." << std::endl;
				refined_measure = raw_measure;
			}

			//increment COUNT of TOTAL samples
			num_total_samples++;

			//Notify that a new sample is now available
			measureProvisioning.notifyNewUpdate();
		}
		virtual void publish_statistic()
		{
			//Assign to statistic latest Mean and Variance calculated by MeanGuy
			std::cerr << " S| Media pronta e richiesta!" << std::endl;
//			statistic.average = MeanGuy.getMean();
//			statistic.variance = MeanGuy.getVariance();

			//DEBUG: Assign to statistic number of expected samples (constant for this IProbe and therefore not necessary)
			// statistic.expected_samples = statistic_delay.count() / sample_delay.count();		//ALREADY DONE ONCE in constructor (it is constant value)
			//DEBUG: Assign to statistic also number of total samples, valid and invalid, taken from driver
			statistic.total_samples = num_total_samples;
			//DEBUG: Assign to statistic also number of valid samples taken from driver
//			statistic.valid_samples = MeanGuy.getSampleNumber();

			//Estimate STATISTIC VALIDIY:
			// comparing number of EXPECTED samples with numer of VALID samples picked from IInputDevice will consider clock imprecision AND errors from IInputDevice
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
//			MeanGuy.reset();
			num_total_samples = 0;

			//Notify all that new statistics are now available
			statisticProvisioning.notifyNewUpdate();
		}


		//SENSOR POLLING
		IPluggable<raw_elem_type>* board;			//Puntatore all'oggetto IInputDevice da cui chiamare la funzione request() per chiedere il campione  *** (CASTING IS A PATCH)                 
		bool autorefresh;							//TRUE: pooling attivo, FALSE: campionamento solo su richiesta (get_measure)        
		//Se autorefresh è TRUE: ogni "sample_rate" ms viene richiesta una nuova misura al driver (sample)
		//Se autorefresh è FALSE, "sample_rate" è ignorato.
		void refresh()								//Questa funzione chiama sample() e convert() e aggiorna il buffer raw_measure (e statistic solo se avg_interval è trascorso)
		{
			std::unique_lock<std::recursive_mutex> access(rw, std::defer_lock);
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
					//publish_statistic();

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
				//	IProbe does not store the converted measure since a specific "convert()" pure virtual
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
		//Se autorefresh è TRUE viene chiamata da un thread ogni "sample_rate" ms oppure manualmente da get_raw()
		//Se autorefresh è FALSE solo get_raw() può chiamarla
		void reset()				//Resetta i valori del sensore a default (chiamata quando ad es. si vuole scollegare il sensore senza deallocarlo!)
		{
			std::unique_lock<std::recursive_mutex> access(rw, std::defer_lock);

			std::cout << " | Sto resettando il sensore..." << std::endl;

			//Closes current autosampling thread, if any
			closeSamplingThread();

			//Reset local variables
			board = nullptr;
			num_total_samples = 0;				//Number of total samples (both VALID and INVALID) picked from IInputDevice
			close_thread = false;
			r = nullptr;

			//Initialization of statistic struct possibly returned to the user
			statistic.average = 0;
			statistic.variance = 0;
			statistic.is_valid = false;		//to be sure it will be set to true!
			statistic.percentage_validity = 0;
			statistic.total_samples = 0;		//num_total_samples will be assigned to it
			statistic.expected_samples = 0;		//statistic_delay.count() / sample_delay.count(); 	//An ESTIMATION of number of expected samples to be picked from IInputDevice;
			statistic.valid_samples = 0;		//Number of VALID samples actually considered in statistic calculation

			std::cerr << "  S| Buffer e variabili resettate." << std::endl;
			std::cerr << "  S| Buffer rigenerati." << std::endl;
			std::cout << " | Reset del sensore completato." << std::endl;
		}


		//THREADING STRUCTURES
		std::recursive_mutex rw;				//Guarantees ONLY mutual access between autorefresh thread and external requesting threads
		std::promise<refined_elem_type> new_sample_p;
		std::promise<statistic_struct> new_statistic_p;
		std::condition_variable new_sample;
		std::condition_variable new_statistic;
		std::thread* r = nullptr;
		bool close_thread = false;
		void closeSamplingThread()
		{
			std::unique_lock<std::recursive_mutex> access(rw, std::defer_lock);

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

		utils::UpdateService measureProvisioning;
		utils::UpdateService statisticProvisioning;


	public:
		//"safe" si intende nel contesto in cui UN SOLO thread usi la classe e sia attivo il thread per l'autosampling


		//COSTRUTTORE & DISTRUTTORE
		IProbe
			(
			const polling_policy_t selected_polling = automatic,
			const processing_policy_t selected_processing = none,	//if "none" or "custom" following parameters are ignored!
			const unsigned int mean_interval = 1,					//avg_interval = minutes after which online average is periodically reset
																	//if not set, default interval is 1 minute
			const bool enable_mean_offset = false					//offset = the minimum value is calculated in the interval and used as offset to add to mean
																	//this allows error removal on peripherals that accumulate skew over time (for example dust on a dust sensor!)
			) : polling_mode(selected_polling), processing_mode(selected_processing)//, MeanGuy(enable_mean_offset)
		{
			//Convert avg_interval in a valid chrono type
			statistic_delay = std::chrono::seconds(mean_interval * 60);
			reset();
		}

		explicit IProbe
			(
			
			const unsigned int sample_rate,					//force a specific value for sample_rate (default = dynamically adapt to the minimum of attached device)
															//ALERT: if you put this too low, IProbe will supersample from device (many requests to device returning same result!)
			const polling_policy_t selected_polling = automatic,
			const processing_policy_t selected_processing = none,
			const unsigned int mean_interval = 1,			//avg_interval = minutes after which online average is periodically reset
															//if not set, default interval is 1 minute
			const bool enable_mean_offset = false			//offset = the minimum value is calculated in the interval and used as offset to add to mean
															//this allows error removal on peripherals that accumulate skew over time (for example dust on a dust sensor!)
			) : IProbe(selected_polling, selected_processing)
		{	
			dynamic_sample_rate = false;

			//Convert sample_rate in a valid chrono type
			if (sample_rate == 0) sample_delay = std::chrono::duration< int, std::milli >::zero();
			else sample_delay = std::chrono::milliseconds(sample_rate);
		}


		//N.B.	Questa funzione è UTILE SOLO PER MISURE che:
		//	- lavorano su "spike" ovvero danno misure significative solo su alcuni campioni
		//	- necessitano di una frequenza di campionamento elevata
		//	- soffrono di deviazioni FISICHE nel tempo dovute ad usura od ostrusioni.                                                                                            			
		//La più alta misura in un intervallo non viene riportata in maniera assoluta, ma relativa alla più bassa misura di quell'intervallo!

		~IProbe()
		{
			closeSamplingThread();
		}	//safe



		//METODI DI ACCESSO PRIMARI (gestiscono i lock)
		raw_elem_type get_raw(sync_policy_t sync_mode = sync)	//safe                          	//Restituisce l'ultima misura. Se autorefresh è FALSE ed è trascorso min_sample_rate
		{
			raw_elem_type returned_measure;
			std::lock_guard<std::recursive_mutex> access(rw);
			if (board != NULL)
			{
				if(polling_mode == manual)
				{
					close_thread = true;
					switch (sync_mode)
					{
					case sync:		//the caller thread executes refresh() and waits for its end
						refresh();
						break;
					case async:		//the caller starts a new thread that executes refresh() only once, user may wait or check its termination
						std::thread t(&IProbe::refresh, this);
						t.detach();
						measureProvisioning.subscribe(utils::new_only);
						break;
					}
				}
			}
			else std::cerr << " S| Attenzione: nessuna device allacciata al sensore.\n | Usare il metodo plug_to() per associare." << std::endl;

			returned_measure = raw_measure;

			return returned_measure;
		}
		//dall'ultima chiamata, richiede anche una nuova misura (sample), altrimenti da l'ULTIMA effettuata
		//( in futuro: IMPLEMENTARE una versione che dia il measure_code della misura restituita )

		/**double get_average(){ lock_guard<mutex> access(rw); return average; };
		double get_variance(){ lock_guard<mutex> access(rw); return variance; };**/
		statistic_struct get_statistic(){ std::lock_guard<std::recursive_mutex> access(rw); return statistic; };

		void wait_new_sample() //safe                                  //Se chiamata, ritorna solo quando il sensore effettua la prossima misura
		{
			if (polling_mode == automatic) measureProvisioning.waitForNextUpdate();
		}
		//- HA EFFETTO SOLO SE AUTOREFRESH E' ATTIVO (altrimenti non ha senso perchè la richiesta la farebbe get_measure)
		//- E' UTILE SE SUBITO DOPO VIENE CHIAMATA get_measure
		void wait_new_statistic() //safe                               //Stessa cosa di wait_new_sample() ma per media e varianza        
		{
			statisticProvisioning.waitForNextUpdate();
		}

		bool new_sample_is_ready()
		{
			return measureProvisioning.checkForUpdates();
		}

		bool new_statistic_is_ready()
		{
			return statisticProvisioning.checkForUpdates();
		}

		//METODI SECONDARI (sfruttano i metodi primari)
		refined_elem_type get_measure()
		{
			return convert(get_raw());
		}									//Fa la stessa cosa di get_raw(), ma la converte prima di restituirla (ON-THE-GO CONVERSION)
		virtual std::string stype() = 0;	//returns a string explaining the type of sensor
		virtual std::string sunits() = 0;	//returns a string explaining the units of measure used
		void display_measure()
		{
			std::cout << stype() << ": " << get_measure() << " " << sunits() << std::endl;
		}
		void display_statistic()
		{
			std::lock_guard<std::recursive_mutex> access(rw);
			std::cout << stype() << ":" << std::endl << "Media: " << statistic.average << " " << sunits() << std::endl << "Varianza: " << statistic.variance << " " << sunits() << std::endl;
		}


		// PLUGGING / ATTACHING PHASE: lega il IProbe ad un IInputDevice e ne abilita il processo di lettura. VA CHIAMATO PER AVVIARE IL SENSORE!

		// -- WEAK TIME SYNC version --
		//This allows to the sample() of the IProbe to call request() of the attached IInputDevice
		void plug_to(const IPluggable<raw_elem_type>& new_board)
		{
			plug_to(new_board, std::chrono::system_clock::now());	//The logic time instant where sampling begins is set HERE as now()
		};
		// -- STRONG TIME SYNC version --
		//This constructor allows you to set a "start_time" manually: it can be before or after the PLUG_TO() PHASE!
		//Extremely useful if you want MORE Sensors TO BE IN SYNC with each other, and to AVOID JITTER!!
		//EXPLANATION:
		//It is impossible to start execution of a IProbe at a precise time (due to SO Scheduling, non-parallelism, etc.)
		//BUT we can fix a COMMON REFERENCE TIME POINT BETWEEN MORE Sensors.
		//Knowing also the INTERVAL, Sensors won't wake up all at the same instant or at the specified time...
		//but they will all keep up with themselves in the specified time window(s)!!
		//ALERT:
		//By default, if no "start_time" is ever set, a now() will be called ALWAYS at plug_to phase!
		void plug_to(const IPluggable<raw_elem_type>& new_board, const std::chrono::system_clock::time_point& start_time) //As above, but you set the start_point manually
		{
			std::lock_guard<std::recursive_mutex> access(rw);

			//Reset board and also the thread if present
			if (board != NULL) reset();

			//Set new board
			board = const_cast< IPluggable<raw_elem_type>* > (&new_board);

			//Get sample_rate from current device and convert it in a valid chrono type
			//Also, initialize buffering variables
			if (board != nullptr)
			{
				if (dynamic_sample_rate) sample_delay = std::chrono::milliseconds(board->getMinDelay());
				raw_measure = board->invalidValue();
				refined_measure = raw_measure;
			}

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
				r = new std::thread(&IProbe::refresh, this);		// Per eseguire refresh() è richiesto this quando è un metodo della classe stessa

				//wait_new_sample();     			// Aspettiamo che il thread abbia almeno calcolato il primo sample() per considerare il sensore "collegato"   	
			}


		}

	};

	//Include separate implementation
	//#include "IProbe.tpp"


}


/////////////////
#endif

