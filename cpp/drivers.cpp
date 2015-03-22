
#include "drivers.h"


///////////////////////////
//GENERIC DRIVER PROCEDURES
template <class data_type, class elem_type>
bool Driver<data_type,elem_type>::ready()
{
	bool delay_elapsed;
	if (std::chrono::steady_clock::now() <= (last_request+request_delay) ) delay_elapsed = false;
	else
	{
		delay_elapsed = true;
		last_request = std::chrono::steady_clock::now();
	}
	return delay_elapsed;

}

template <class data_type, class elem_type>
data_type Driver<data_type,elem_type>::request_all()
{
	unsigned int i=0;
   	elem_type* d = (elem_type*)&m;

	lock_guard<mutex> access(rw);
	
	//If driver is ready(), performing a new recv_measure() and updating driver state
	if(ready()) state=recv_measure();
	
	//If state is not NICE, set all elem_types to INVALID   
    	if(state!=NICE) for (i=0;i<n_elems;i++) d[i]=INVALID;

	return m;
}

template <class data_type, class elem_type>
elem_type Driver<data_type,elem_type>::request(const unsigned int type)
{

   	uint16_t measure=0;
    	elem_type* d = (elem_type*)&m;	//d is a pointer accessing m as an array with offset type

    	lock_guard<mutex> access(rw);
    
    	//If driver is ready(), performing a new recv_measure() and updating driver "state"
    	//N.B. ready() DOES NOT SAY MEASURE IS VALID OR NOT!!
	if(ready()) state=recv_measure();
	
	//ONLY IF "type" exists in data_type "m"...
	if( type <= n_elems && type > 0)
	{	
		//...if "state" is not NICE means last read failed so measure MUST BE INVALID    
	    	if(state!=NICE)
	    	{
	    		measure=INVALID;
	    		cerr<<"  D| WARNING: problemi con la periferica, ultima misura non valida!"<<endl;
	    	}
	    	//...if "state" is NICE means last read was successful so pick the selected measure from data_type "m"
	    	else
	    	{
	    		measure=d[type-1];
	    	}
	}
	//ELSE (when "type" does not exist) return ALWAYS an INVALID measure
	else
	{
		measure=INVALID;
		cerr<<"  D| ERRORE: TIPO di misura richiesta non supportata dal driver."<<endl;
	}

	
	//N.B. measure is returned ALSO if device is not ready(): this will ALWAYS be LAST the measure retrieved if "state" is NICE, INVALID otherwise
	return measure;


}




///////////////////////
//USB DRIVER PROCEDURES

bool Usb::ready()
{

	bool device_ready=false;

	//(1) Check base class/driver constraint
	bool base_ready = Driver<measure_struct,uint16_t>::ready();	//Verifico le condizioni del driver di base.

	if(base_ready)							//Se sono verificate, procedo con quelle specifiche
	{
		//(2) Check if device is physically plugged in
		if(d==NULL)                                                //Se la handle attuale non è valida...
		{
			cerr<<"  D| Nessuna periferica. Avvio scansione..."<<endl;
			if ( Usb::scan(vid,pid) == NICE )		            //Chiama scan()
			{
				cerr<<"  D| Periferica individuata. Avvio connessione..."<<endl;
				d = hid_open(vid, pid, NULL);		            //Se device trovata, connettila.
				if(d!=NULL)
				{
					cerr<<"  D| Periferica connessa!"<<endl;		//Se connessa, DEVICE READY
					device_ready=true;
				}
				else cerr<<"  D| ERRORE: hid_open ha restituito NULL con periferica collegata.\n  | Controllare i permessi."<<endl;
			}
			else cerr<<"  D| ERRORE: Periferica non trovata! Assicurarsi che il cavo sia inserito."<<endl;

			//if(!device_ready) cerr<<"  D| WARNING: le misure non saranno aggiornate."<<endl;
			last_request = std::chrono::steady_clock::now();	//Resetto il timer, così che ci sia un tempo minimo
										//anche tra una scansione e un'altra.
		}
		else device_ready=true;				//Se la handle è valida, DEVICE READY

	}

	return device_ready;				//Solo se (1) e (2) vere allora ready() ritorna TRUE!
}


int Usb::scan(const int vid, const int pid)
{
	struct hid_device_info* devices;	//Lista linkata di descrittori di device (puntatore al primo elemento)
	struct hid_device_info* curr_dev;	//Descrittore di device selezionato (puntatore per scorrere la lista sopra)

	//Scansione della nostra device
	int i, esito_funzione=ERROR;		
	bool trovata=false;
	cerr<<"   D! Scansione devices in corso..."<<endl;
	//while (!trovata)
	//{
	//Scansiona tutte periferiche e le alloca in devices
	devices = hid_enumerate(0x0, 0x0);
	curr_dev=devices;
	i=0;
	while(curr_dev) 		//finchè non raggiungo la fine della lista (cioè curr_dev==NULL)
	{
		if(curr_dev->vendor_id == vid && curr_dev->product_id == pid)
		{


			//cerr<<"Device "<<++i<<" trovata!"<<endl;
			//cerr<<"  Manufacturer: "<<curr_dev->manufacturer_string<<"\nProduct: "<<curr_dev->product_string<<endl;

			cerr<<"   x Device "<<++i<<" trovata:"<<endl;
			cerr<<"     |  VID: "<<hex<<curr_dev->vendor_id<<" PID: "<<curr_dev->product_id<<dec<<endl;
			cerr<<"     |  Path: "<<curr_dev->path<<"\n     |  serial_number: "<<curr_dev->serial_number<<endl;
			cerr<<"     |  Manufacturer: "<<curr_dev->manufacturer_string<<endl;
			cerr<<"     |  Product:      "<<curr_dev->product_string<<endl;
			cerr<<"     |  Release:      "<<curr_dev->release_number<<endl;
			cerr<<"     |  Interface:    "<<curr_dev->interface_number<<endl;
			trovata=true;
			esito_funzione=NICE;
		}
		else
		{
			cerr<<"   o Device "<<++i<<"-> VID: "<<curr_dev->vendor_id<<" PID: "<<curr_dev->product_id<<"  --  NO MATCH"<<endl;
		}
		curr_dev=curr_dev->next;
	}

	//Dealloca la lista di devices
	hid_free_enumeration(devices);


	//FLAG DI CONTROLLO
	/*
	 *x* Forza il thread a terminare la ricerca se STOP è alto
		if(get_stop())
		{
			trovata=true;		*x* Forza trovata=true per uscire dal ciclo
			cout<<".....scan aborted by user....."<<endl;
			esito_funzione=ABORTED;
			//set_stop(false);	//Resetta il flag
		}
	 */

	//Se STOP è falso e trovata è falso aspetta 3 secondi prima di effettuare una nuova scansione
	if(!trovata)
	{
		cerr<<"   D! Nessuna corrispondenza con VID e PID cercati."<<endl;
		//p_sleep(5000);
	}



	//}

	return esito_funzione;	


}


/*
int usb::read_show(const unsigned int times, const unsigned int delay)		//uses recv_measure() and displays/uses its result
{
	unsigned int i=0;
	int status=ERROR;
	measure_struct misura;

	cout<<"Inizio apertura device..."<<endl;
	hid_device* handle = hid_open(MY_VID, MY_PID, NULL);		//Device in uso (puntatore alla handle del kernel)

	if (handle == NULL)
	{
		cout<<"Errore nell'apertura della device!"<<endl;
		status=ERROR;
	}
	else
	{
		cout<<"Periferica aperta!"<<endl;

		for(i=0;i<times && status!=ERROR && status!=ABORTED;i++)
		{
			cout<<"| Lettura da "<<handle<<" in corso..."<<endl;
			status=usb::recv_measure(handle,misura);
			if(status==ERROR) cout<<"| Errore: lettura abortita. Codice: "<<status<<endl;
			else if(status==ABORTED) cout<<"| Lettura abortita dall'utente. Codice: "<<status<<endl;
			else
			{
				cout<<"| Lettura effettuata.\n|   Polvere: "<<misura.dust<<"\n|   Temperatura: "<<misura.temp<<"\n|   Umidità: "<<misura.humid<<endl;	
				p_sleep(delay);
			}
		}

		hid_close(handle);
		cout<<"Device chiusa."<<endl;
	}

	return status;	

}
 */

int Usb::recv_measure()	//copies device format data into the embedded measure_struct data type of the driver instance
{	
	int bytes_read=0, last_bytes_read=0, bytes_to_read=sizeof(measure_struct), status=ERROR;
	int i, num_retry = 5;
	unsigned char buf[6] = { 0 , 0 , 0 , 0 , 0 , 0 } ;
	std::chrono::milliseconds retry_interval( 10 ) ;			//sleep amount in case of soft fail
	


	if(d!=NULL)
	{
		cerr<<"  D| Procedura di lettura iniziata."<<endl;
		hid_set_nonblocking(d,1);					//Default - read bloccante (settare 1 per NON bloccante)
		while( last_bytes_read != bytes_to_read && bytes_read!=-1 && num_retry>0)	//Questo ciclo si ripete fino a 5 letture errate E SE NON ho un errore critico (-1) -- OLD: !get_stop() &&
		{
			num_retry--;
		
			//Re-init
			for(i=0;i<6;i++) buf[i] = 0;
			i=0;
			
			cerr<<"   D! Tentativo "<<++i<<endl;
			do
			{

				last_bytes_read=bytes_read;						//Memorizza il numero di byte letti dal report precedente
				bytes_read = hid_read(d,buf,bytes_to_read);				//Leggi il report successivo:
				//cerr<<bytes_read<<endl;						//	- se bytes_read>0 allora esiste un report più recente nel buffer -> scaricalo
				//	- se bytes_read=0, non ci sono più report -> l'ultimo scaricato è quello buono
				//	- se bytes_read=-1 c'é un errore critico con la device -> interrompi tutto
				//La read si blocca AL PIU' per 5 secondi, dopodiché restituisce errore critico (-1)
			}while(bytes_read>0);								//Cicla finché il buffer non è stato svuotato (0) oppure c'è stato errore critico (-1)

			//HARD FAIL
			if (bytes_read == -1)								//Se c'è stato errore...
			{
				cerr<<"   D! Lettura fallita."<<endl;
				cerr<<"   D! ERRORE: Periferica non pronta o scollegata prematuramente.\n  D| Disconnessione in corso..."<<endl;
				hid_close(d);									//chiudi la handle. 	-> ESCE dal ciclo.
				cerr<<"  D| Device disconnessa."<<endl;
				d=NULL;	//Resets handle pointer for safety
			}
			else										//..altrimenti analizziamo per bene l'ultimo report scaricato...
			{
				//SOFT FAIL
				if (last_bytes_read < bytes_to_read)						//se i byte letti dell'ultima misura nel buffer (la più recente) non sono del numero giusto
				{
					cerr<<"   D! Lettura fallita."<<endl;						//-> la lettura è fallita, bisogna riprovare. 	-> NON ESCE dal ciclo.
					std::this_thread::sleep_for( retry_interval ) ;
				}
				//GOOD
				else										//altrimenti -> stampa a video il risultato e memorizzalo in "m" -> ESCE dal ciclo.
				{
					//Debug dump visualization
					cerr<<"   D! "<<last_bytes_read<<" bytes letti: ";
					cerr<<(int)buf[0]<<" "<<(int)buf[1]<<" "<<(int)buf[2]<<" "<<(int)buf[3]<<" "<<(int)buf[4]<<" "<<(int)buf[5]<<" "<<endl;
					
					
					/* SWAP
					for(i=0;i<6;i=i+2)
					{
						temp=buf[i];
						buf[i]=buf[i+1];
						buf[i+1]=temp;
					}
					*/
					
					//Old approach
					//memcpy( (void*) &m, (void*) buf, bytes_to_read);
					
					
					//Lo shift dei registri del PIC è stato spostato lato driver
					m.temp = (( buf[4] << 8 ) + buf[5])>>2 ;
					m.humid =  ((buf[2]<< 8 ) + buf[3]) & 0x3fff;
					m.dust = ( buf[0] << 8 ) + buf[1] ;
					
					if ( m.humid == 0xFFFF || m.temp == 0xFFFF )
					{
						std::cerr<<"   D! WARNING: La device ha ritornato valori di Temperatura e Umidita' non validi." <<std::endl;
					}
					else status=NICE;								//In conclusione, la funzione ritorna NICE solo se ha letto esattamente 6byte validi!

				}
			}

		}

		cerr<<"  D| Procedura di lettura conclusa."<<endl;
	}
	else cerr<<"  D| ERRORE: il driver non è connesso ad alcuna periferica! (device=NULL)"<<endl;


	//Do a simple stat of recv_measure
	//if(status==NICE) num_succeded_reads++;
	//else num_failed_reads++;


	return status;				//Qui ERROR è ritornato di default a meno che non vada tutto OK.

}


/*
uint16_t Usb::request(const int type)
{

    lock_guard<mutex> access(rw);

    if(ready())
    {
    	state=recv_measure();
    	if( state == ERROR ) cerr<<"  D| WARNING: riconnettere la periferica, misure non aggiornate!"<<endl;
    }
    
    
    uint16_t measure=0;
    
    switch ( type ) {
    case TEMPERATURE:
      measure=m.temp;
      break;
    case HUMIDITY:
      measure=m.humid;
      break;
    case DUST:
      measure=m.dust;
      break;
    default:
      measure=INVALID;
      cout<<"  D| ERRORE: TIPO di misura richiesta non supportata dal driver."<<endl;
      break;
    }
        
    return measure;
    

}
*/


/////////////////////////////
//RASPBERRY DRIVER PROCEDURES


bool Raspberry::ready()
{

	bool device_ready=false;

	//(1) Check base class/driver constraint
	bool base_ready = Driver<measure_struct,uint16_t>::ready();	//Verifico le condizioni del driver di base.

	if(base_ready)							//Se sono verificate, procedo con quelle specifiche
	{
		//(2) Check if handle is already open
		if(i2cHandle==0)                                                //Se la handle attuale non è valida...
		{
			cerr<<"  D| Nessuna handle aperta. Avvio connessione..."<<endl;
			i2cHandle = open("/dev/i2c-1",O_RDWR);				//Create a file descriptor for i2c bus
			if ( ioctl( i2cHandle , I2C_SLAVE , 0x27) == 0 )		//Tell the I2C peripheral the device address
			{
				cerr<<"  D| Connessione riuscita. Handle pronta."<<endl;
				device_ready=true;
			}
			else cerr<<"  D| ERRORE: Connessione non riuscita. (?)"<<endl;

			if(!device_ready) cerr<<"  D| WARNING: le misure non saranno aggiornate."<<endl;
		}
		else device_ready=true;						//Se la handle è valida, DEVICE READY

	}

	return device_ready;				//Solo se (1) e (2) vere allora ready() ritorna TRUE!
}



int Raspberry::recv_measure()
{
	/* i2c funziona in modo diverso da usb:
		- io dico l'indirizzo dove andare a leggere (in questo caso è 0x4e che sarebbe 0x27 shiftato come richiede il protocollo)
		- dopo un'attesa sufficiente, provvedo a leggere i dati
	*/
	uint8_t Data[4] = {0x4E,0,0,0};
	int status = ERROR;
	
	if(i2cHandle!=0)
	{
	
		cerr<<"  D! Abilitazione lettura..."<<endl;
		if ( write(i2cHandle,Data,1) !=1)
		{
			cerr<<"  D! ERRORE: abilitazione lettura fallita!"<<endl;
		}
		else
		{	
			cerr<<"  D! Abilitazione lettura riuscita."<<endl;
			p_sleep(40);	//Sleep for 40000 microsecond it's equals to sleep for 40 ms.
					//It is necessary to let sensor reach nirvana

			if(( read( i2cHandle ,Data , 4 )) == 4 )
			{
				std::cerr<<"   D! leggo 1 "<<std::endl;
				//shifting..
				m.temp = ((Data[2] << 8)+Data[3])>>2;
				m.humid = ((Data[0] << 8) +Data[1]);
				status=NICE;
			}
			else cerr<<"  D! ERRORE: Problema sulla lettura (?)"<<endl;
		}
				
	}
	
	return status;
}


/*
uint16_t Raspberry::request(const int type)
{

    	lock_guard<mutex> access(rw);
    	
	if(ready())
	{
		if( recv_measure() == ERROR )	//IF ready condition is satisfied, call recv_measure();
		cerr<<"  D| WARNING: problemi su seriale, misure non aggiornate!"<<endl;
	}

	uint16_t measure=0;

	switch ( type ) {
	case TEMPERATURE:
		measure=m.temp;
		break;
	case HUMIDITY:
		measure=m.humid;
		break;
	default:
		measure=ERROR;
		cerr<<"  D| ERRORE: TIPO di misura richiesta non supportata dal driver."<<endl;
		break;
	}

	return measure;


}
*/
