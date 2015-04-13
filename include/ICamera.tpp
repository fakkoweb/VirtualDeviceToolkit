//Implementation for template of ICamera.hpp
//Compiler does not allow to separate declaration from implementation explicitly.
//This file is then included in the .hpp file, so that is still conceptually separated but compiler can still read this


template <class raw_data_type>
ICamera<raw_data_type>::ICamera(const unsigned int min_delay) : vdt::IInputDevice<raw_data_type, cv::Mat>(min_delay)
{
	
}

template <class raw_data_type>
ICamera<raw_data_type>::~ICamera()
{
	if (active) deactivate();
	//active = false;
}

template <class raw_data_type>
bool ICamera<raw_data_type>::ready()
{

	bool device_ready = false;

	//(1) Check base class/driver constraint
	bool base_ready = vdt::IInputDevice<raw_data_type, cv::Mat>::ready();	//Verifico le condizioni del driver di base.

	if (base_ready)							//Se sono verificate, procedo con quelle specifiche
	{
		//(2) Check if device is physically plugged in
		if (!active)								//Se la device non è attualmente attiva...
		{
			cerr << "  D| Camera device not active. Activating..." << endl;
			activate();
			active = true;
			cerr << "  D| Camera device is now active" << endl;
		}

		device_ready = active;

	}
	else device_ready = true;				//Se la handle è valida, DEVICE READY

	return device_ready;				//Solo se (1) e (2) vere allora ready() ritorna TRUE!
}

template <class raw_data_type>
void ICamera<raw_data_type>::init()
{
	std::lock_guard<std::mutex> access(rw);
	activate();
	active = true;
}

template <class raw_data_type>
void ICamera<raw_data_type>::close()
{
	std::lock_guard<std::mutex> access(rw);
	deactivate();
	active = false;
}

