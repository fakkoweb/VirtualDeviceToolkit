#ifndef _SDRF_SENSOR_H_
#define _SDRF_SENSOR_H_
////////////////////


//Include user configuration
#include "config.h"

//Internal dependencies
#include "driver.h"
#include "OMV.h"

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
#endif

namespace sdrf
{

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


	class Sensor					//ABSTRACT CLASS: only sub-classes can be instantiated!
	{
	protected:

		//BUFFERING VARIABLES
		uint16_t raw_measure;
		//OLD:	double format_measure;		//Memorizza una versione convertita di raw_buffer - non più necessaria, ora la misura è convertita su richiesta
		statistic_struct statistic;


		//AVERAGING AND VARIANCE CALCULATION	//asdfg
		OMV MeanGuy;							//Classe per il calcolo della media on-line (Knuth/Welford algorithm) --> vedi lista di inizializzazione del costruttore!

		std::chrono::duration< int, std::milli > statistic_delay;	//GESTIONE STATISTIC INTERVAL: Ogni sample() del sensore viene calcolata una nuova media e varianza,
		//basate sul valore corrente e sulla storia precedente. Ad ogni refresh(), il sensore confronta
		//statistic_delay (ovvero "avg_interval" minuti) con il tempo trascorso da last_statistic_request
		//e quando occorre preleva le statistiche da MeanGuy e lo resetta prima di ulteriori sample().
		std::chrono::steady_clock::time_point last_statistic_request;   //last_statistic_request è l'UNICA ANCORA/RIFERIMENTO TEMPORALE che Sensor ha con l'esterno!!
		//Ad ogni statistic_delay, a last_statistic_request è sommato statistic_delay.
		//HO UN RIFERIMENTO TEMPORALE ASSOULUTO, FISSATO IN PLUGGING PHASE (init. a "start_time")!

		std::chrono::duration< int, std::milli > sample_delay;		//GESTIONE REFRESH RATE: sample_delay (ovvero "sample_rate" ms) è solo il tempo ideale di sampling!
		//IMPOSTATO A SECONDA DEL TIPO DI SENSORE!! Inizializzato da costruttore a "sample_rate"
		//UTILE SOLO SE autorefresh E' TRUE!!
		//La funzione refresh() terrà conto sia del ritardo di computazione che di wake-up dello scheduler, riducendo
		//il tempo di sleep del thread di autorefresh.
		//MINIMIZZO IL JITTER!



		//SAMPLING & CONVERSION
		virtual int mtype() = 0;					//returns the type (its code) of measure sensor requests to driver
		uint16_t sample(){ return board->request(mtype()); };	//Chiamata da get_measure, semplicemente chiama board (la request() col tipo misura richiesto)
		virtual double convert(const uint16_t) = 0;       	//THIS FUNCTIONS MUST BE SPECIALIZED BY INHERITING CLASSES


		//SENSOR POLLING
		Driver<measure_struct, uint16_t>* board;	//Puntatore all'oggetto Driver da cui chiamare la funzione request() per chiedere il campione                   
		bool autorefresh;                       //TRUE: pooling attivo, FALSE: campionamento solo su richiesta (get_measure)        
		//Se autorefresh è TRUE: ogni "sample_rate" ms viene richiesta una nuova misura al driver (sample)
		//Se autorefresh è FALSE, "sample_rate" è ignorato.
		void refresh();                         //Questa funzione chiama sample() e convert() e aggiorna il buffer raw_measure (e statistic solo se avg_interval è trascorso)
		//Se autorefresh è TRUE viene chiamata da un thread ogni "sample_rate" ms oppure manualmente da get_raw()
		//Se autorefresh è FALSE solo get_raw() può chiamarla
		void reset();				//Resetta i valori del sensore a default (chiamata quando ad es. si vuole scollegare il sensore senza deallocarlo!)


		//THREADING STRUCTURES
		std::mutex rw;				//Guarantees ONLY mutual access between autorefresh thread and external requesting threads
		std::condition_variable new_sample;
		std::condition_variable new_statistic;
		std::thread* r;
		bool close_thread;


	public:
		//"safe" si intende nel contesto in cui UN SOLO thread usi la classe e sia attivo il thread per l'autosampling


		//COSTRUTTORE & DISTRUTTORE
		Sensor() = delete;                         		//disabling zero-argument constructor completely
		explicit Sensor(const int sample_rate,			//sample_rate = millisecondi per l'autocampionamento (se attivato)
			const int avg_interval,			//avg_interval = minuti ogni quanto viene resettata la media (che è calcolata on-line)
			const bool enable_autorefresh = true,	//indica se rendere ATTIVO (autopolling) o PASSIVO (misure su richiesta) il sensore
			const bool enable_mean_offset = false);	//indica a MeanGuy di considerare, per ogni media, un offset pari al minimo tra i campioni dell'intervallo su cui è calcolata
		//N.B.	Questa funzione è UTILE SOLO PER MISURE che:
		//	- lavorano su "spike" ovvero danno misure significative solo su alcuni campioni
		//	- necessitano di una frequenza di campionamento elevata
		//	- soffrono di deviazioni FISICHE nel tempo dovute ad usura od ostrusioni.                                                                                            			
		//La più alta misura in un intervallo non viene riportata in maniera assoluta, ma relativa alla più bassa misura di quell'intervallo!

		~Sensor();	//safe



		//METODI DI ACCESSO PRIMARI (gestiscono i lock)
		uint16_t get_raw();	//safe                          	//Restituisce l'ultima misura. Se autorefresh è FALSE ed è trascorso min_sample_rate
		//dall'ultima chiamata, richiede anche una nuova misura (sample), altrimenti da l'ULTIMA effettuata
		//( in futuro: IMPLEMENTARE una versione che dia il measure_code della misura restituita )

		/**double get_average(){ lock_guard<mutex> access(rw); return average; };
		double get_variance(){ lock_guard<mutex> access(rw); return variance; };**/
		statistic_struct get_statistic(){ std::lock_guard<std::mutex> access(rw); return statistic; };

		void wait_new_sample(); //safe                                  //Se chiamata, ritorna solo quando il sensore effettua la prossima misura
		//- HA EFFETTO SOLO SE AUTOREFRESH E' ATTIVO (altrimenti non ha senso perchè la richiesta la farebbe get_measure)
		//- E' UTILE SE SUBITO DOPO VIENE CHIAMATA get_measure
		void wait_new_statistic(); //safe                               //Stessa cosa di wait_new_sample() ma per media e varianza        


		//METODI SECONDARI (sfruttano i metodi primari)
		double get_measure(){ return convert(get_raw()); };		//Fa la stessa cosa di get_raw(), ma la converte prima di restituirla (ON-THE-GO CONVERSION)
		virtual std::string stype() = 0;	//returns a string explaining the type of sensor
		virtual std::string sunits() = 0;	//returns a string explaining the units of measure used
		void display_measure(){ std::cout << stype() << ": " << get_measure() << " " << sunits() << std::endl; };
		void display_statistic()
		{
			std::lock_guard<std::mutex> access(rw);
			std::cout << stype() << ":" << std::endl << "Media: " << statistic.average << " " << sunits() << std::endl << "Varianza: " << statistic.variance << " " << sunits() << std::endl;
		};


		// PLUGGING / ATTACHING PHASE: lega il Sensor ad un Driver e ne abilita il processo di lettura. VA CHIAMATO PER AVVIARE IL SENSORE!

		// -- WEAK TIME SYNC version --
		//This allows to the sample() of the Sensor to call request() of the attached Driver
		void plug_to(const Driver<measure_struct, uint16_t>& new_board)
		{
			plug_to(new_board, std::chrono::system_clock::now());	//The logic time instant where sampling begins is set HERE as now()
		};
		// -- STRONG TIME SYNC version --
		//This constructor allows you to set a "start_time" manually: it can be before or after the PLUG_TO() PHASE!
		//Extremely useful if you want MORE Sensors TO BE IN SYNC with each other, and to AVOID JITTER!!
		//EXPLANATION:
		//It is impossible to start execution of a Sensor at a precise time (due to SO Scheduling, non-parallelism, etc.)
		//BUT we can fix a COMMON REFERENCE TIME POINT BETWEEN MORE Sensors.
		//Knowing also the INTERVAL, Sensors won't wake up all at the same instant or at the specified time...
		//but they will all keep up with themselves in the specified time window(s)!!
		//ALERT:
		//It is also possible to set up a manual "start_time" at plug_to() call. If you do, doing it here is useless.
		//By default, if no "start_time" is ever set, a now() will be called ALWAYS at plug_to phase!
		void plug_to(const Driver<measure_struct, uint16_t>& new_board, const std::chrono::system_clock::time_point& start_time); //As above, but you set the start_point manually

	};

}


/////////////////
#endif

