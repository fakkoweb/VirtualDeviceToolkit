#ifndef _VDT_IINPUTDEVICE_H_
#define _VDT_IINPUTDEVICE_H_
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


namespace vdt
{

	//DEPRECATA: IInputDevice call usata quando i driver erano statici -> puntatore a funzione
	//typedef uint16_t (*driver_call)();

	//PURE INTERFACE: methods a class should implement to be "pluggable" by an IProbe
	//This interface is used by IProbe to access homogeneously to ANY class type providing data (es. IDevice)
	template <class raw_elem_type>
	class IPluggable
	{
	public:
		virtual raw_elem_type request(const unsigned int type) = 0;
		virtual raw_elem_type invalidValue() = 0;
		virtual bool isValid(raw_elem_type& elem) = 0;
		virtual unsigned int getMinDelay(){ return 0; }			//Not all plugged devices are real, default delay is 0
	};


	//CLASSE NON ISTANZIABILE -- Template che rappresenta una device generica che su richiesta restituisce una struct "raw_data_type" contenente N diversi "raw_elem_type"
	template <class raw_data_type, class raw_elem_type>
	class IInputDevice : public IPluggable<raw_elem_type>
	{
	protected:
		raw_data_type m;					//Contains the last raw data extracted by recv_measure
		const unsigned int n_elems;			//Number of elements contained into raw_data_type - AUTOMATICALLY INIZIALIZED BY CONSTRUCTOR
		std::mutex rw;						//Guarantees mutual access from multiple sensors
		int state;							//Keeps track of last state returned by recv_measure() each call - if ERROR, m is set to VDT_VALUE_INVALID (see config.h)
		std::chrono::duration< int, std::milli > request_delay;		//GESTIONE DELAY HARDWARE: Se più sensori/processi/thread fanno richiesta al driver di seguito,
		std::chrono::steady_clock::time_point last_request;			//limito le richieste effettive inoltrate all'hardware fisico (tengo conto dei ritardi intrinseci e
																	//li nascondo al programmatore) SOPRATTUTTO le richieste INUTILI (ad esempio, in caso di errore,
																	//mi basta che sia la prima richiesta a segnalarlo per le richieste subito successive).

		virtual bool ready()					//TELLS IF DEVICE IS READY (NOT BUSY!) TO SATISFY A NEW RECEIVE ( recv_measure() )
												//IT DOES NOT TELL IF THE MEASURE IS VALID OR NOT! (this ONLY depends on recv_measure() returning VDT_VALUE_ERROR or VDT_VALUE_NICE!!)
												//N.B.: this condition should be tested RIGHT BEFORE calling recv_measure
												//It implements generic driver algorithms to counter hardware limits:
												//	- Minimum delay between requests
												//CAN BE EXTENDED BY DERIVED CLASSES!!
		{
			bool delay_elapsed;
			if (std::chrono::steady_clock::now() <= (last_request + request_delay)) delay_elapsed = false;
			else
			{
				delay_elapsed = true;
				last_request = std::chrono::steady_clock::now();
			}
			return delay_elapsed;

		}

		virtual int recv_measure() = 0;			//Takes a new raw_data_type from d via HID protocol from physical device
		//RETURNS VDT_VALUE_ERROR if the read failed and measure is not valid, VDT_VALUE_NICE otherwise.
		//To mark a measure as not valid, a specific "VDT_VALUE_INVALID" value has been defined in config.h

	public:
		//IInputDevice() = delete;				//Disable default constructor
		explicit IInputDevice(const unsigned int min_delay = HARDWARE_DELAY) : n_elems(sizeof(raw_data_type) / sizeof(raw_elem_type))
		{
			if (min_delay <= 0) request_delay = std::chrono::duration< int, std::milli >::zero();
			else request_delay = std::chrono::milliseconds(min_delay);
			last_request = std::chrono::steady_clock::now() - request_delay; //This way first recv_measure is always performed regardless of timer
			state = VDT_VALUE_ERROR;	//for programming safety, initialize state as VDT_VALUE_ERROR (even if unlogic)
		}

		raw_data_type request_all()   				//A utility method that returns raw_data_type AS A WHOLE
													//It has the same rules of request() -- see below
		{
			unsigned int i = 0;
			raw_elem_type* d = reinterpret_cast<raw_elem_type>(&m);

			std::lock_guard<std::mutex> access(rw);

			//If driver is ready(), performing a new recv_measure() and updating driver state
			if (ready()) state = recv_measure();

			//If state is not VDT_VALUE_NICE, set all elem_types to VDT_VALUE_INVALID   
			if (state != VDT_VALUE_NICE) for (i = 0; i<n_elems; i++) d[i] = invalidValue();

			return m;
		}

		virtual raw_elem_type request(const unsigned int type)				//RETURNS the raw_elem_type from raw_data_type at position "type".
																			//Argument "type" must be non-zero and raw_elem_type returned will depend on how you defined raw_data_type!
																			//This method:
																			// - PROTECTS recv_measure() with the "rw" mutex provided
																			// - Tests YOUR ready() condition, which MUST extend the base one
																			// - Issues the recv_measure whenever possible (ONLY when YOUR ready() returns true)
																			// - Returns "VDT_VALUE_INVALID" (see config.h) when YOUR LAST recv_measure returned VDT_VALUE_ERROR
		{

			raw_elem_type measure = invalidValue();
			raw_elem_type* d = reinterpret_cast<raw_elem_type*>(&m);		//d is a pointer accessing m as an array with offset type

			std::lock_guard<std::mutex> access(rw);

			//If driver is ready(), performing a new recv_measure() and updating driver "state"
			//N.B. ready() DOES NOT SAY MEASURE IS VALID OR NOT!!
			if (ready()) state = recv_measure();

			//ONLY IF "type" exists in raw_data_type "m"...
			if (type <= n_elems && type > 0)
			{
				//...if "state" is not VDT_VALUE_NICE means last read failed so measure MUST BE VDT_VALUE_INVALID    
				if (state != VDT_VALUE_NICE)
				{
					//measure = invalidValue();
					std::cerr << "  D| WARNING: problemi con la periferica, ultima misura non valida!" << std::endl;
				}
				//...if "state" is VDT_VALUE_NICE means last read was successful so pick the selected measure from raw_data_type "m"
				else
				{
					measure = d[type - 1];
				}
			}
			//ELSE (when "type" does not exist) return ALWAYS an VDT_VALUE_INVALID measure
			else
			{
				//measure = invalidValue();
				std::cerr << "  D| ERRORE: TIPO di misura richiesta non supportata dal driver." << std::endl;
			}


			//N.B. measure is returned ALSO if device is not ready(): this will ALWAYS be the LAST measure retrieved if "state" is VDT_VALUE_NICE, VDT_VALUE_INVALID otherwise
			return measure;


		}

		//Get device delay in milliseconds (because making too frequent requests is useless)
		unsigned int getMinDelay()
		{
			return request_delay.count();
		}

		virtual raw_elem_type invalidValue() = 0;
		virtual bool isValid(raw_elem_type& elem) = 0;

		//virtual ~IInputDevice();
	};

	//Include separate implementation
	//#include "IInputDevice.tpp"

}


/////////////////
#endif

