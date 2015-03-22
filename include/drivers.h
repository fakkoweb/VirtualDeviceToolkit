#ifndef _DRIVERS_H_
#define _DRIVERS_H_
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

//External libs
#include "hidapi.h"
#include "p_sleep.h"
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <fcntl.h>


using namespace std;


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
    	mutex rw;			    //Guarantees mutual access from multiple sensors
    	int state;			    //Keeps track of last state returned by recv_measure() each call - if ERROR, m is set to INVALID (see config.h)
        std::chrono::duration< int, std::milli > request_delay;		//GESTIONE DELAY HARDWARE: Se più sensori/processi/thread fanno richiesta al driver di seguito,
        std::chrono::steady_clock::time_point last_request;		//limito le richieste effettive inoltrate all'hardware fisico (tengo conto dei ritardi intrinseci e
        								//li nascondo al programmatore) SOPRATTUTTO le richieste INUTILI (ad esempio, in caso di errore,
        								//mi basta che sia la prima richiesta a segnalarlo per le richieste subito successive).
        
        virtual bool ready();               //TELLS IF DEVICE IS READY (NOT BUSY!) TO SATISFY A NEW RECEIVE ( recv_measure() )
        				    //IT DOES NOT TELL IF THE MEASURE IS VALID OR NOT! (this ONLY depends on recv_measure() returning ERROR or NICE!!)
        				    //N.B.: this condition should be tested RIGHT BEFORE calling recv_measure
        				    //It implements generic driver algorithms to counter hardware limits:
        				    //	- Minimum delay between requests
        				    //CAN BE EXTENDED BY DERIVED CLASSES!!
        
        virtual int recv_measure()=0;       //Takes a new data_type from d via HID protocol from physical device
                                            //RETURNS ERROR if the read failed and measure is not valid, NICE otherwise.
                                            //To mark a measure as not valid, a specific "INVALID" value has been defined in config.h

    public:
        Driver(const int min_delay = HARDWARE_DELAY) : n_elems( sizeof(data_type)/sizeof(elem_type) ){
            if(min_delay<=0) request_delay = std::chrono::duration< int, std::milli >::zero();
            else request_delay = std::chrono::milliseconds(min_delay);
            last_request = std::chrono::steady_clock::now() - request_delay; //This way first recv_measure is always performed regardless of timer
            state=ERROR;	//for programming safety, initialize state as ERROR (even if unlogic)
        };
        data_type request_all();   				//A utility method that returns data_type AS A WHOLE
        							//It has the same rules of request() -- see below
                                                                
        virtual elem_type request(const unsigned int type);	//RETURNS the elem_type from data_type at position "type".
        							//Argument "type" must be non-zero and elem_type returned will depend on how you defined data_type!
        							//This method:
        							// - PROTECTS recv_measure() with the "rw" mutex provided
        							// - Tests YOUR ready() condition, which MUST extend the base one
        							// - Issues the recv_measure whenever possible (ONLY when YOUR ready() returns true)
        							// - Returns "INVALID" (see config.h) when YOUR LAST recv_measure returned ERROR

};



//CLASSE ISTANZIABILE -- Rappresenta una device USB via HID
//Contiene tutte le funzioni relative a USB
class Usb : public Driver<measure_struct,uint16_t>
{
    private:
    	//unsigned long long int num_failed_reads;	//A simple stat for recv_measure
    	//unsigned long long int num_succeded_reads;

    protected:
        hid_device* d;                      //Selected hardware device via HID protocol. IF IT IS NULL, NO DEVICE IS CONNECTED SO CHECK FIRST!!
                                            //Device is "opened" at first call of recv_measure()
        int vid;
        int pid;
        
        virtual bool ready();               //TELLS IF DEVICE IS READY (NOT BUSY!) TO SATISFY A NEW RECEIVE ( recv_measure() )
               				    //IT DOES NOT TELL IF THE MEASURE IS VALID OR NOT! (this ONLY depends on recv_measure() returning ERROR or NICE!!)
        				    //N.B.: this condition should be tested RIGHT BEFORE calling recv_measure
        				    //Implements usb driver algorithms to counter hardware limits:
        				    //	(base) Minimum delay between requests
        				    //	(extension) Device IS plugged in (issue a scan and try plug if not)
        				    //CAN BE EXTENDED BY DERIVED CLASSES!!
        				    	
        virtual int recv_measure();         //SPECIALIZED: Takes a new "measure_struct" from d via HID protocol from physical device.
                                            //RETURNS ERROR if the read failed and measure is not valid, NICE otherwise.
                                            //To mark a measure as not valid, a specific "INVALID" value has been defined in config.h
        
    public:
        Usb() = delete; 
        Usb(const int vid_in, const int pid_in, const int min_delay = HARDWARE_DELAY) : Driver(min_delay){
    	    //num_failed_reads=0;
    	    //num_succeded_reads=0;
            d=NULL;
            m.temp=0;
            m.humid=0;
            m.dust=0;
            vid=vid_in;
            pid=pid_in;
            hid_init();		//init hidapi for safety
        };
        ~Usb()
        {
            if(d!=NULL) hid_close(d);
            cerr<<"  D| Device chiusa."<<endl;
            hid_exit();		//free hidapi data
        };
        
    
    	//Funzioni generiche (static) usb
	static int scan(const int vid, const int pid);
		
		
		//VECCHIE FUNZ.
		//static int recv_measure(hid_device* d, measure_struct &m);	//riceve una singola misura da device usb
		
		//Debug
		//static int read_show(const unsigned int times, const unsigned int delay);

    
};



//CLASSE INSTANZIABILE -- Rappresenta l'interfaccia seriale del Raspberry
//Contiene tutte le funzioni relative a RASP
class Raspberry : public Driver<measure_struct,uint16_t>
{
    protected:
    	int i2cHandle;
    	
        virtual bool ready();               //TELLS IF DEVICE IS READY (NOT BUSY!) TO SATISFY A NEW RECEIVE ( recv_measure() )
        				    //IT DOES NOT TELL IF THE MEASURE IS VALID OR NOT! (this ONLY depends on recv_measure() returning ERROR or NICE!!)        
        				    //N.B.: this condition should be tested RIGHT BEFORE calling recv_measure
        				    //Implements serial raspberry driver algorithms to counter hardware limits:
        				    //	(base) Minimum delay between requests
        				    //	(extension) Serial handle IS open (try open it if not)
        				    //CAN BE EXTENDED BY DERIVED CLASSES!!
        				    
        virtual int recv_measure();         //SPECIALIZED: Takes a new measure_struct from physical device
                                            //RETURNS ERROR if the read failed and measure is not valid, NICE otherwise.
                                            //To mark a measure as not valid, a specific "INVALID" value has been defined in config.h
        
    public:
        Raspberry(const int min_delay = HARDWARE_DELAY) : Driver(min_delay){
            i2cHandle=0;
            m.temp=0;
            m.humid=0;
        };
        ~Raspberry(){
            cerr<<"  D| Handle seriale chiusa."<<endl;
            if(i2cHandle!=0) close(i2cHandle);
        }

        
        //Funzioni generiche raspberry
        
        
        //VECCHIE FUNZ.
    	//static int recv_measure(hid_device* d, measure_struct &m);	//riceve una singola misura da device usb

};


/////////////////
#endif

