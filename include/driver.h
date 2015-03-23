#ifndef _SDRF_DRIVER_H_
#define _SDRF_DRIVER_H_
////////////////////


//Include user configuration
#include "config.h"

//Standard included libraries
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
//#include <inttypes.h>
#include <chrono>
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
#endif


namespace sdrf
{

	//DEPRECATA: Driver call usata quando i driver erano statici -> puntatore a funzione
	//typedef uint16_t (*driver_call)();

	typedef struct _MEASURE_STRUCT
	{
		uint16_t temp;
		uint16_t humid;
		uint16_t dust;
	} measure_struct;


	//CLASSE NON ISTANZIABILE -- Template che rappresenta una device generica che su richiesta restituisce una struct "data_type" contenente N diversi "elem_type"
	template <class data_type, class elem_type>
	class Driver
	{
	protected:
		data_type m;                        //Contains the last raw data extracted by recv_measure
		const unsigned int n_elems;	    //Number of elements contained into data_type - AUTOMATICALLY INIZIALIZED BY CONSTRUCTOR
		std::mutex rw;			    //Guarantees mutual access from multiple sensors
		int state;			    //Keeps track of last state returned by recv_measure() each call - if ERROR, m is set to SDRF_VALUE_INVALID (see config.h)
		std::chrono::duration< int, std::milli > request_delay;		//GESTIONE DELAY HARDWARE: Se più sensori/processi/thread fanno richiesta al driver di seguito,
		std::chrono::steady_clock::time_point last_request;		//limito le richieste effettive inoltrate all'hardware fisico (tengo conto dei ritardi intrinseci e
		//li nascondo al programmatore) SOPRATTUTTO le richieste INUTILI (ad esempio, in caso di errore,
		//mi basta che sia la prima richiesta a segnalarlo per le richieste subito successive).

		virtual bool ready();               //TELLS IF DEVICE IS READY (NOT BUSY!) TO SATISFY A NEW RECEIVE ( recv_measure() )
		//IT DOES NOT TELL IF THE MEASURE IS VALID OR NOT! (this ONLY depends on recv_measure() returning SDRF_VALUE_ERROR or SDRF_VALUE_NICE!!)
		//N.B.: this condition should be tested RIGHT BEFORE calling recv_measure
		//It implements generic driver algorithms to counter hardware limits:
		//	- Minimum delay between requests
		//CAN BE EXTENDED BY DERIVED CLASSES!!

		virtual int recv_measure() = 0;       //Takes a new data_type from d via HID protocol from physical device
		//RETURNS SDRF_VALUE_ERROR if the read failed and measure is not valid, SDRF_VALUE_NICE otherwise.
		//To mark a measure as not valid, a specific "SDRF_VALUE_INVALID" value has been defined in config.h

	public:
		Driver(const int min_delay = HARDWARE_DELAY) : n_elems(sizeof(data_type) / sizeof(elem_type)){
			if (min_delay <= 0) request_delay = std::chrono::duration< int, std::milli >::zero();
			else request_delay = std::chrono::milliseconds(min_delay);
			last_request = std::chrono::steady_clock::now() - request_delay; //This way first recv_measure is always performed regardless of timer
			state = SDRF_VALUE_ERROR;	//for programming safety, initialize state as SDRF_VALUE_ERROR (even if unlogic)
		};
		data_type request_all();   				//A utility method that returns data_type AS A WHOLE
		//It has the same rules of request() -- see below

		virtual elem_type request(const unsigned int type);	//RETURNS the elem_type from data_type at position "type".
		//Argument "type" must be non-zero and elem_type returned will depend on how you defined data_type!
		//This method:
		// - PROTECTS recv_measure() with the "rw" mutex provided
		// - Tests YOUR ready() condition, which MUST extend the base one
		// - Issues the recv_measure whenever possible (ONLY when YOUR ready() returns true)
		// - Returns "SDRF_VALUE_INVALID" (see config.h) when YOUR LAST recv_measure returned SDRF_VALUE_ERROR

	};

}


/////////////////
#endif
