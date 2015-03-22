#ifndef _CONFIG_H_
#define _CONFIG_H_


//N.B.: Global variables are parameters loaded and assigned with param_load()


///////////////////////////////
//Parametri di configurazione// --> quelli commentati sono specificali in "parameters.json"

//Device identification
//#define MY_VID 0x04D8
//#define MY_PID 0x0013

//Local_feed_id per identificare i sensori al server
//#define EXT_TEMP_lfid 0
//#define EXT_HUMID_lfid  0
//#define EXT_DUST_lfid   0
//#define INT_TEMP_lfid   0
//#define INT_HUMID_lfid  0

//Costanti per esprimere lo status delle funzioni
#define RUNNING	1
#define IDLE	0

//Costanti per esprimere l'esito delle funzioni
#define ERROR	1
#define NICE	0
#define ABORTED	-1

//Costanti per distinguere i tipi di misure
#define TEMPERATURE    1
#define HUMIDITY   2
#define DUST    3

//Costante per esprimere una misura non valida o non disponibile
#define NA	-1	//ATTENZIONE: deve essere negativo (perché le misure raw da sensore variano in 0-255)
#define INVALID 0xFFFF

#define THRESHOLD 60
//Costanti per il prelievo misure  --> specificali in "parameters.json"
//#define REPORT_INTERVAL 5         //in minuti - ogni quanto il programma manda un report al server
                                    //(ovvero l'intervallo utile di tempo su cui calcolare la media e varianza)
//#define SENSOR_BUFFER   30        //in numero - ogni quante misure il sensore calcola la media
//#define SENSOR_REFRESH_RATE 5     //in secondi - ogni quanto il sensore fa una nuova misura (richieste al driver su sec)
//#define TEMP_BUFFER   0;          //Deprecato: si ricava da (REPORT_INTERVAL*60)/TEMP_REFRESH_RATE
//#define TEMP_REFRESH_RATE = 0;
//#define HUMID_BUFFER = 0;         //Deprecato: si ricava da (REPORT_INTERVAL*60)/HUMID_REFRESH_RATE
//#define HUMID_REFRESH_RATE = 0;
//#define DUST_BUFFER = 0;          //Deprecato: si ricava da (REPORT_INTERVAL*60)/DUST_REFRESH_RATE
//#define DUST_REFRESH_RATE = 0;
#define HARDWARE_DELAY  10          //in millisecondi - intervallo minimo di richieste soddisfatte dal driver
                                    //(alle richieste sotto questo intervallo sarà data l'ultima misura effettuata)
#define T_H_DIV		16382		//divisions used for temp and humid conversion: 2^14-2


//Altre define di codice                                
#define HOST_THREADS		        //Commenta per disabilitare il supporto thread (c++11)



///////////////////////////////

#endif
