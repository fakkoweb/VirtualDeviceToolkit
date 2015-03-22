#ifndef _FUNCTIONS_H_
#define _FUNCTIONS_H_
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
	#include <future>
#endif

//External libs
#include "p_sleep.h"
#include <json.h>	//for accessing Json::Value

//Internal libs
#include "sensors.h"	//for map of sensors and statistic_struct type support
#include "http_manager.h"


using namespace std;






//////////////////////////
// GENERIC FUNCTIONS
float faverage(float* array, int dim);
float fvariance(float* array, int dim);
string getTimeStamp();


////////////////////////////////////////
// PARAMETER LOADING FROM A json file
// Input: file.json -- Output: Json Value
Json::Value load_params(const string jsonfile);
extern const Json::Value params;	// GLOBAL CONSTANT VARIABLE FOR SPEED PARAMETER ACCESS

/////////////////////////////////////////////////////////
// THREADS/PROCEDURES
bool registering(const string device_mac, const Json::Value& sensors);							//thread called once to announce device/sensors to server (called again when network fails)
bool reporting(const string filename, const string device_mac, const map<int, Sensor*>& sa);				//thread called periodically to save and post to server statistics (if any in the local file)
template<class return_type> bool is_ready(std::future<return_type>& f){ return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready; } //tells if <future> is ready (in our case: registering thread)


//////////////////////
// ATOMIC OPERATIONS - ALL RETURN ERROR,ABORTED,NICE
int register_device(const string device_mac);                      	//Checks if MAC_ADDR is registered. If not, registers. RETURNS ERROR,ABORT,NICE
int register_sensors(const string device_mac, const Json::Value& sd);  	//Checks if Sensor is registered. If not, registers. RETURNS ERROR,ABORT,NICE
									//The Json::Value is needed for registering further data like "tags"
int register_sensor(const string device_mac, const Json::Value& node, const string rs);			//The recursive call from register_sensors (to check all json nodes recursively)									
int save_report(const string to_filename, const map<int, Sensor*>& sa);
int post_report(const string from_filename, const string device_mac, const map<int, Sensor*>& sa);   	//Gets statistics from Sensors (without waiting!!) in sa and posts it to server





///////////////////////
#endif










