#ifndef _SDRF_FUNCTIONS_H_
#define _SDRF_FUNCTIONS_H_
////////////////////


//Include user configuration
#include "config.h"

//Standard included libraries
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <map>
#include <string>
#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/ipc.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <sys/shm.h>
    #include <unistd.h>
#endif
#ifdef HOST_THREADS
	#include <mutex>
	#include <thread>
	#include <future>
#endif

//Internal libs
#include "sensors.h"	//for map of sensors and statistic_struct type support


namespace sdrf
{

	//////////////////////////
	// GENERIC FUNCTIONS
	float faverage(float* array, int dim);
	float fvariance(float* array, int dim);
	std::string getTimeStamp();

}


///////////////////////
#endif










