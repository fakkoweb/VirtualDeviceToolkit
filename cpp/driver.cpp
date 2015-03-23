
#include "driver.h"


namespace sdrf
{

	///////////////////////////
	//GENERIC DRIVER PROCEDURES
	template <class raw_data_type, class raw_elem_type>
	bool Driver<raw_data_type, raw_elem_type>::ready()
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

	template <class raw_data_type, class raw_elem_type>
	raw_data_type Driver<raw_data_type, raw_elem_type>::request_all()
	{
		unsigned int i = 0;
		raw_elem_type* d = reinterpret_cast<raw_elem_type>(&m);

		std::lock_guard<std::mutex> access(rw);

		//If driver is ready(), performing a new recv_measure() and updating driver state
		if (ready()) state = recv_measure();

		//If state is not SDRF_VALUE_NICE, set all elem_types to SDRF_VALUE_INVALID   
		if (state != SDRF_VALUE_NICE) for (i = 0; i<n_elems; i++) d[i] = SDRF_VALUE_INVALID;

		return m;
	}

	template <class raw_data_type, class raw_elem_type>
	raw_elem_type Driver<raw_data_type, raw_elem_type>::request(const unsigned int type)
	{

		raw_elem_type measure = 0;
		raw_elem_type* d = reinterpret_cast<raw_elem_type>(&m);	//d is a pointer accessing m as an array with offset type

		std::lock_guard<std::mutex> access(rw);

		//If driver is ready(), performing a new recv_measure() and updating driver "state"
		//N.B. ready() DOES NOT SAY MEASURE IS VALID OR NOT!!
		if (ready()) state = recv_measure();

		//ONLY IF "type" exists in raw_data_type "m"...
		if (type <= n_elems && type > 0)
		{
			//...if "state" is not SDRF_VALUE_NICE means last read failed so measure MUST BE SDRF_VALUE_INVALID    
			if (state != SDRF_VALUE_NICE)
			{
				measure = SDRF_VALUE_INVALID;
				cerr << "  D| WARNING: problemi con la periferica, ultima misura non valida!" << endl;
			}
			//...if "state" is SDRF_VALUE_NICE means last read was successful so pick the selected measure from raw_data_type "m"
			else
			{
				measure = d[type - 1];
			}
		}
		//ELSE (when "type" does not exist) return ALWAYS an SDRF_VALUE_INVALID measure
		else
		{
			measure = SDRF_VALUE_INVALID;
			cerr << "  D| ERRORE: TIPO di misura richiesta non supportata dal driver." << endl;
		}


		//N.B. measure is returned ALSO if device is not ready(): this will ALWAYS be LAST the measure retrieved if "state" is SDRF_VALUE_NICE, SDRF_VALUE_INVALID otherwise
		return measure;


	}

	//////////////////////

}


