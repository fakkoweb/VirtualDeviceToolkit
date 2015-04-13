//Implementation for template of ICamera.hpp
//Compiler does not allow to separate declaration from implementation explicitly.
//This file is then included in the .hpp file, so that is still conceptually separated but compiler can still read this


template <class raw_data_type, class raw_elem_type>
IInputDevice<raw_data_type, raw_elem_type>::IInputDevice(const unsigned int min_delay) : n_elems(sizeof(raw_data_type) / sizeof(raw_elem_type)){
	if (min_delay <= 0) request_delay = std::chrono::duration< int, std::milli >::zero();
	else request_delay = std::chrono::milliseconds(min_delay);
	last_request = std::chrono::steady_clock::now() - request_delay; //This way first recv_measure is always performed regardless of timer
	state = VDT_VALUE_ERROR;	//for programming safety, initialize state as VDT_VALUE_ERROR (even if unlogic)
}


///////////////////////////
//GENERIC DRIVER PROCEDURES
template <class raw_data_type, class raw_elem_type>
bool IInputDevice<raw_data_type, raw_elem_type>::ready()
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
raw_data_type IInputDevice<raw_data_type, raw_elem_type>::request_all()
{
	unsigned int i = 0;
	raw_elem_type* d = reinterpret_cast<raw_elem_type>(&m);

	std::lock_guard<std::mutex> access(rw);

	//If driver is ready(), performing a new recv_measure() and updating driver state
	if (ready()) state = recv_measure();

	//If state is not VDT_VALUE_NICE, set all elem_types to VDT_VALUE_INVALID   
	if (state != VDT_VALUE_NICE) for (i = 0; i<n_elems; i++) d[i] = VDT_VALUE_INVALID;

	return m;
}

template <class raw_data_type, class raw_elem_type>
raw_elem_type IInputDevice<raw_data_type, raw_elem_type>::request(const unsigned int type)
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
		//...if "state" is not VDT_VALUE_NICE means last read failed so measure MUST BE VDT_VALUE_INVALID    
		if (state != VDT_VALUE_NICE)
		{
			measure = VDT_VALUE_INVALID;
			cerr << "  D| WARNING: problemi con la periferica, ultima misura non valida!" << endl;
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
		measure = VDT_VALUE_INVALID;
		cerr << "  D| ERRORE: TIPO di misura richiesta non supportata dal driver." << endl;
	}


	//N.B. measure is returned ALSO if device is not ready(): this will ALWAYS be LAST the measure retrieved if "state" is VDT_VALUE_NICE, VDT_VALUE_INVALID otherwise
	return measure;


}

template <class raw_data_type, class raw_elem_type>
unsigned int IInputDevice<raw_data_type, raw_elem_type>::getMinDelay()
{
	return request_delay.count();
}



