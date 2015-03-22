
#include "functions.h"
#include <iomanip>
#include <algorithm>



/// UTILITY FUNCTIONS


///////////////////
// NOT ZERO CHECK
//When check_not_zero finds <0, makes true the static variable:
static bool zero_found=false;
//Utility function: checks if at least one of a few values is 0 or less
//Returns the same value.
int check_not_zero(int value)
{
    if(value<=0) zero_found=true;
    return value;
}


///////////////////
// GET TIMESTAMP OF NOW
string getTimeStamp(){

	time_t now;
    	time(&now);
    	char buf[sizeof "2011-10-08T07:07:09Z"];
    	strftime(buf, sizeof buf, "%FT%TZ", gmtime(&now));
 	stringstream ss;
 	string s;
 	ss<<buf;
 	ss>>s;
 	return s;
}


///////////////////
// SWAP commas (,) WITH dots (.)
string dotnot(string input)
{
	replace( input.begin(), input.end(), ',', '.');
	return input;
}




/// PROGRAM FUNCTIONS


Json::Value load_params(const string jsonfile)
{
    //Caricamento parametri utente (da parameters.json)
    Json::Reader reader;  // Oggetto per il parsing: trasforma file.json ---> json::value
    Json::Value loaded_params;
    std::ifstream param_input(jsonfile, std::ifstream::binary); //Nuovo oggetto stream in ingresso (associato al file "filename.json")
    if ( !reader.parse( param_input, loaded_params, true ) )
    {
        // report to the user the failure and their locations in the document.
        std::cout  << "Errore di lettura dal file di configurazione:\n"
                   << reader.getFormattedErrorMessages();
        loaded_params.clear();
    }
    else
    {
    
	    //Visualizza i parametri appena caricati (e verifica che gli interi non siano nulli o minori di 0!)
	    cout<<"Parametri caricati"<<endl;
	    cout<<"-- ID data --"<<endl;
	    cout<<"MY_VID: 0x"<<hex<< check_not_zero( loaded_params["device"].get("MY_VID",0).asInt() ) <<endl;
	    cout<<"MY_PID: 0x"<<hex<< check_not_zero( loaded_params["device"].get("MY_PID",0).asInt() ) <<endl;
	    cout<<"MY_MAC: "<<loaded_params["device"].get("MY_MAC",0).asString()<<endl;
	    cout<<"Outdoor Temp Local feed id: "<<dec<< check_not_zero( loaded_params["sensors"]["temp"]["ext"].get("lfid",0).asInt() ) <<endl;
	    cout<<"Outdoor Humid Local feed id: "<< check_not_zero( loaded_params["sensors"]["humid"]["ext"].get("lfid",0).asInt() ) <<endl;
	    cout<<"Outdoor Dust Local feed id: "<< check_not_zero( loaded_params["sensors"]["dust"]["ext"].get("lfid",0).asInt() ) <<endl;
	    cout<<"Indoor Temp Local feed id: "<< check_not_zero( loaded_params["sensors"]["temp"]["int"].get("lfid",0).asInt() ) <<endl;
	    cout<<"Indoor Humid Local feed id: "<< check_not_zero( loaded_params["sensors"]["humid"]["int"].get("lfid",0).asInt() ) <<endl;
	    cout<<"-- Precision data --"<<endl;
	    cout<<"Temperature sample rate (sec): "<< check_not_zero( loaded_params["sensors"]["temp"].get("REFRESH_RATE",0).asInt() ) <<endl;
	    cout<<"Humidity sample rate (sec): "<< check_not_zero( loaded_params["sensors"]["humid"].get("REFRESH_RATE",0).asInt() ) <<endl;
	    cout<<"Dust sample rate (sec): "<< check_not_zero( loaded_params["sensors"]["dust"].get("REFRESH_RATE",0).asInt() ) <<endl;
	    cout<<"Server report interval (min): "<< check_not_zero( loaded_params["report"].get("INTERVAL",0).asInt() ) <<endl;
	    cout<<"\tEach sensor will work on num.samples = ("<< loaded_params["report"].get("INTERVAL",0).asInt() <<"*60s)/sample_rate"<<endl;
    	    cout<<"Acceptable report validity (%): "<< check_not_zero( loaded_params["report"].get("VALIDITY_PERCENTAGE_THRESHOLD",0).asInt() ) <<"%"<<endl;

	    if(zero_found)
	    {
	    	cout<<"WARNING: Some values from configuration are not valid (0 or less). Params cleared!"<<endl;
	    	loaded_params.clear();
	    }
	    zero_found=false;	//reset check_not_zero() flag
	    
    }
    
    return loaded_params;	//will be empty if loading has failed
    
    
}





bool registering(const string device_mac, const Json::Value& sensors)
{
	int dev_reg_status=ERROR;
	int sens_reg_status=ERROR;
	//Registrazione device - ritorna NICE o ABORTED se ha successo, altrimenti riproverà al prossimo giro
	if(dev_reg_status==ERROR)
	{
		cout<<"NET: Avvio routine di annuncio device al server..."<<endl;
		dev_reg_status = register_device(device_mac);
		if(dev_reg_status==NICE) cout<<"NET NICE: Device registrata."<<endl;
		else if(dev_reg_status==ABORTED) cout<<"NET ABORT: Device gia' registrata."<<endl;
		else cout<<"NET ERROR: Problemi di comunicazione con il server! Operazione rimandata."<<endl;
	}
	//Registrazione sensori (solo se la device è registrata)
	if(dev_reg_status!=ERROR && sens_reg_status==ERROR)
	{
		cout<<"NET: Avvio routine di annuncio sensori al server..."<<endl;
		//Registrazione sensori - NON ritorna gli unique_feed_id registrati sul server
		//(1) ATTENZIONE se si cambiano i valori lfid dei sensori (ma non la loro posizione) da parameters.json il sistema sarà diverso!!
		//(2) ATTENZIONE scambiare gli lfid tra loro mischierebbe le misure al server!
		sens_reg_status = register_sensors(device_mac, sensors);
		if(sens_reg_status==NICE) cout<<"NET NICE: Uno/più nuovi sensori registrati."<<endl;
		else if(sens_reg_status==ABORTED) cout<<"NET ABORT: Tutti i sensori sono gia' registrati."<<endl;
		else cout<<"NET ERROR: Problemi di comunicazione con il server! Operazione rimandata."<<endl;
	}
	
	//Pronti a mandare misure sul server? Solo se device e sensori sono tutti registrati!
	return (dev_reg_status!=ERROR && sens_reg_status!=ERROR);

}




int register_device( const string device_mac )
{
    int esito=ERROR;
    string server_response_s;

    //Check if the device already exists:
    esito=http_get_auth( params["report"].get("API_URL","").asString() + "devices/" +device_mac, server_response_s);
    
    //check_registration= check_registration.substr(0,16);
    size_t found = server_response_s.find("mac="+device_mac);
    //Device already registered?
    if(esito!=ERROR)	//check if there were errors in connection BEFORE register
    {
	    if (found!=std::string::npos)		//NO -- server returned a mac=x:x:x:x:x:x string because it is missing
	    {
	    	cerr<<"Device con MAC cercato non trovato sul server. Il device sara' registrato."<<endl; 
	    	Json::Value reg_device;
	       	reg_device["id"]=0;
	      	reg_device["username"]="gruppo19";
	      	reg_device["raspb_wifi_mac"]=device_mac;
	       	cout<<reg_device<<endl;
	      	esito=http_post_auth( params["report"].get("API_URL","").asString() + "devices/" , reg_device.toStyledString(), server_response_s);
	       	//cerr<<"\nRISPOSTA SERVER SU REGISTER DEVICE:\n"<<server_response_s<<endl;
	     }
	     else					//YES -- server returned a full json describing our device
	     {
	     	cerr<<"Il device e' già registrato!"<<endl;
	     	esito = ABORTED;
	     }
     }
     
     return esito;
     
}





//Chiede al server la lista dei sensori attualmente registrati. Provvede a registrare quelli nuovi se necessario.
//La funzione ritorna:
//- ABORTED se tutti i nuovi sensori erano già registrati,
//- NICE se almeno un nuovo sensore è stato registrato perché non presente,
//- ERROR se almeno un sensore DOVEVA essere registrato ma la registrazione è fallita.
int register_sensors( const string device_mac, const Json::Value& sd)
{
	int esito=ERROR;
	string registered_sensors_s;

	// check the list of registered sensors:
   	http_get_auth( params["report"].get("API_URL","").asString() + "devices/" +device_mac+"/feeds", registered_sensors_s);
   	//cout<<registered_sensors_s<<endl;
	
	esito = register_sensor(device_mac,sd,registered_sensors_s);
	return esito;
}


//Controlla l'esistenza di lfid (uno solo!) nel nodo json "sd" passato e in quelli in esso contenuti. Se non presenti in "rs" li registra usando "device_mac".
//La funzione ritorna:
//- ABORTED se tutti i nuovi sensori erano già registrati,
//- NICE se almeno un nuovo sensore è stato registrato perché non presente,
//- ERROR se almeno un sensore DOVEVA essere registrato ma la registrazione è fallita.
int register_sensor( const string device_mac, const Json::Value& node, const string rs )
{
	int esito=ABORTED, inner_esito=ERROR;
    	string my_sensor_s;
    	string server_response_s;

	//if you find an lfid code in the node..
    	if(node.isMember("lfid"))
    	{
		//check if the sensor already exists:
		my_sensor_s="\"local_feed_id\":"+node.get("lfid",0).asString();
		if (rs.find(my_sensor_s) == std::string::npos)		//NO
    		{
			Json::Value new_sensor;
      			new_sensor["feed_id"]=0;
      			new_sensor["tags"]=node.get("tags",0).asString();
      			new_sensor["local_feed_id"]=node.get("lfid",0).asInt();
			new_sensor["raspb_wifi_mac"]=device_mac;
			esito=http_post_auth( params["report"].get("API_URL","").asString() + "devices/" +device_mac+"/feeds", new_sensor.toStyledString(), server_response_s);
    			//cerr<<"\nRISPOSTA SERVER SU REGISTER SENSOR "<<node.get("lfid",0).asString()<<":\n"<<server_response_s<<endl;
		}
		else							//YES
		{
			cerr<<"This sensor was already registered... ";
			esito=ABORTED;
		}
    	}
    	//otherwise maybe the attributes may contain lfids..
    	else
    	{
	    	for(Json::Value::iterator i = node.begin(); i !=node.end(); ++i)
		{
			Json::Value element = (*i);
			//END CONDITION: if the attribute contains other attributes, recursively call myself
			if(element.size()!=0)
			{
				inner_esito=register_sensor(device_mac, element, rs);
				//PROPAGAZIONE STATO DI ERRORE
				//esito deve riportare l'errore più grave avvenuto in una delle sottochiamate (inner_esito)
				if(inner_esito!=esito && esito!=ERROR)			//Verifica solo se ci sono stati cambiamenti (se esito era ERROR non ci importa più)
				{
					if(inner_esito != ABORTED) esito = inner_esito;		//se esito era ABORTED, inner_esito avrà riportato un cambiamento: reg. effettuata (NICE) o fallita (ERROR)
												//se esito era NICE, l'algoritmo va comunque bene.
				}
			}
		
		}
	}
	
	//Esito riporterà:
	//- ABORTED se i sensori erano già tutti registrati,
	//- NICE se almeno una registrazione è avvenuta (TUTTE con successo),
	//- ERROR se almeno una registrazione era necessaria MA è fallita.
	return esito;	
	
}


//SERVE SOLO PER TESTARE AUTENTICAZIONE
/*int check_status()
{
	string check_authentication;
	Json::Value authentication;
	Json::Reader reader;
	http_get_auth("http://crowdsensing.ismb.it/SC/rest/apis/auth/gruppo19", check_authentication);
	cout<<check_authentication;
	reader.parse(check_authentication,authentication,false);
	cout<< authentication.toStyledString();
	
}
*/






bool reporting(const string filename, const string device_mac, const map<int, Sensor*>& sa)
{

	
	int post_rep_status=ERROR;
	/*	OLD: save_report() was called just before post_report() but this is not optimal
	int times=3,
    	bool retry=true;
    	while(retry && times>0)
    	{
		esito = save_report(filename, sa);
		if(esito!=NICE)
		{
			times--;
			std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		}
		else retry=false;
	}
	*/
	post_rep_status=post_report(filename, device_mac, sa);
	if(post_rep_status==ERROR)
	{
		cout<<"NET ERROR: Invio statistiche fallito. Problema di rete. Riprovero' a connettermi più tardi."<<endl;
		cerr<<getTimeStamp()<<" - Invio statistiche fallito. Problema di rete. Riprovero' a connettermi più tardi."<<endl;
	}
	else if(post_rep_status==ABORTED)
	{
		cout<<"NET ABORT: Invio annullato: nessuna statistica valida da inviare."<<endl;
		cerr<<getTimeStamp()<<" - Invio annullato: nessuna statistica valida da inviare."<<endl;
	}
	else
	{
		cout<<"NET NICE: Le statistiche sono ora sincronizzate con il server."<<endl;
		cerr<<getTimeStamp()<<" - Le statistiche sono ora sincronizzate con il server."<<endl;
	}
	
	
	return (post_rep_status!=ERROR);
	
}





int save_report(const string to_filename, const map<int, Sensor*>& sa)
{
    	int esito=ERROR;
    	
    	//Output management variables
    	Json::FastWriter writer;
    	Json::Value report;	//Only "sensor_values" field will be saved locally
    	fstream report_file;		//file in which append new json report
    	stringstream textconverter;	//BUFFER to convert any variable to STRING
    	
    	/* OLD CONVERSION
	stringstream textconverter;	//BUFFER to convert any variable to STRING
    					//JSON ONLY WORKS ON TEXT!! TYPES are then distinguished automatically!
	string report_line; 		//json line to append to file
    	*/
    	

	//Open file
    	cout<<"||/ Apro file di buffer..."<<endl;
    	report_file.open (to_filename, ios::out | ios::app);

	//Check file opening
    	std::map<int, Sensor*>::const_iterator row;
    	
    		if(report_file.is_open())
    		{
		    	cout<<"||/ File aperto!"<<endl;
		    	
		    	//Write line to file
			for(row=sa.begin(); row!=sa.end(); row++)
			{
				/* OLD CONVERSION
				textconverter<<row->first;
				string local_feed_id=textconverter.str();
				textconverter.str("");

				//Conversion of FloatingType into String:
				textconverter<<std::setprecision(numeric_limits<double>::digits10+10);
				textconverter <<row->second->get_statistic().average;
		 		string average_value=textconverter.str();
				textconverter.str("");
				textconverter<<row->second->get_statistic().variance;
				string variance=textconverter.str();
				textconverter.str("");
				
				report_line=
					"value_timestamp="+getTimeStamp()+
					"##average_value="+to_string(row->second->get_statistic().average)+
					"##local_feed_id="+to_string(row->first)+
					"##variance="+to_string(row->second->get_statistic().variance)+
					"##units_of_measurement="+row->second->sunits()+"##";

				report_file<<(report_line+"\n");
				*/
			
				if( row->second->get_statistic().is_valid )
				{
					textconverter.str("");
					textconverter<<std::setprecision(numeric_limits<double>::digits10+10);

					//Assembling the "sensor_values" part of Json
					report["value_timestamp"]=getTimeStamp();
					textconverter << row->second->get_statistic().average;
					report["average_value"]=textconverter.str();
					textconverter.str("");
					report["local_feed_id"]=to_string(row->first);
					textconverter << row->second->get_statistic().variance;
					report["variance"]=textconverter.str();
					report["units_of_measurement"]=row->second->sunits();
			
					//Save it to file
					report_file << writer.write(report);
					cout<<"||/ REPORT DI lfid="<< row->first <<" SCRITTO SU FILE!"<<endl;
				}
				else
				{
					cout<<"||/ REPORT DI lfid="<< row->first <<" NON VALIDO E NON SCRITTO!"<<endl;
				}
				
			}

			//Close file
			report_file.close();
			cout<<"||/ File chiuso!"<<endl;
			esito=NICE;
			
			
		}
		else
		{
    			cout<<"||/ CRITICAL ERROR: Could not open buffer file!!"<<endl;
    			esito=ERROR;
    		}


    	return esito;
	
}



int post_report(const string from_filename, const string device_mac, const map<int, Sensor*>& sa)
{
    	int esito=ERROR;
	string server_response_s;
	server_response_s.assign("null");
	
	Json::Reader reader;
	ifstream in_report_file;	//input file containing past reports (not yet dispatched to server)
	ofstream out_report_file;	//output file containing the remaining reports after this post (max 50 are sent)
	int num_extracted_lines=0;	//num of lines extracted from file: if it is 0 then no full report must be sent!
	string report_line; 		//line extracted from file (it will contain one unparsed report)
	Json::Value report;						//a single report extracted from file	
	Json::Value report_array = Json::Value(Json::arrayValue);	//saved reports array (parsed from file)


	//Open File 
	in_report_file.open(from_filename, ios::in );
	if(!in_report_file.is_open())
	{
		cout<<"||/ CRITICAL ERROR: Could not find source buffer file!!"<<endl;
		return esito;
	}
	else
	{
		//Open destination file for reports not being sent now (if there are too many or with problems)
		out_report_file.open("new_"+from_filename, ios::out);
		if(!out_report_file.is_open())
		{
			cout<<"||/ CRITICAL ERROR: Could not create destination buffer file!!"<<endl;
			return esito;
		}
	}

	//Parsing saved reports from input report file
	while( getline(in_report_file,report_line) )
	{
		/*
		//Parsing lines and creating elements of JsonArray Sensor_values:
		string fields[]={"value_timestamp","average_value","local_feed_id","variance","units_of_measurement"};
		string delimiter = "##";
		string token;	
		int i=0;
		size_t pos = 0;
		
		while ((pos = report_line.find(delimiter)) != std::string::npos) 
		{
			token = report_line.substr(0, pos);
			int pos1= token.find_first_of("=");
			report[fields[i]]=token.substr(pos1+1,token.size());
	     		i++;
	    		report_line.erase(0, pos + delimiter.length());
		}
		*/
		
		
		//Extract only a maximum number of measure lines per report sent!
		if( num_extracted_lines < 1000 )//IF current number of lines extracted is less than N
		{
			if( reader.parse(report_line,report) )	//PARSE current line from input_file and CHECK for error
			{
				report_array.append(report);		//NO ERROR: append the parsed line (json format) to report ready to be sent
			}
			else
			{
				out_report_file<<report_line<<"\n";	//ERROR: store the line AS IS to output_file (next report I will try again)
			}
		}
		else				//ELSE (we have already parsed at least N lines)
		{
			out_report_file<<report_line<<"\n";	//ALWAYS store the extracted line to output_file (next report I will fetch other N lines)
		}
		
		num_extracted_lines++;		//Increment number of lines computed

	}	
	in_report_file.close();
	out_report_file.close();
	
	
	if(num_extracted_lines > 0)	//check if input_file was not empty..
	{
		//Declaring COMPLETE Json
		Json::Value json_post;
	
		//Assembling the "position" part: geolocalization data
		Json::Value position;
			//Getting milliseconds since Epoch and converting it to string...
			stringstream textconverter;
			textconverter << std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now().time_since_epoch() ).count();
		position["kind"]="latitude#location";
		position["timestampMs"]= textconverter.str();
		position["latitude"]="45.067616";
		position["longitude"]="7.622660";
		position["accuracy"]="5000";
		position["height_meters"]="240";
	
		//Assembling COMPLETE Json
		json_post["position"]=position;
		json_post["sensor_values"]=report_array;	//attaching the array of reports fetched from file
		json_post["send_timestamp"]=getTimeStamp();
		json_post["raspb_wifi_mac"]=device_mac;

		//Just one HTTP POST call to the Server for all report selected
		esito=http_post_auth( params["report"].get("API_URL","").asString() + "device/" +device_mac+"/posts", json_post.toStyledString(), server_response_s);
		//cerr<<"\nRISPOSTA SERVER SU POST REPORT DI "<<getTimeStamp()<<":\n"<<server_response_s<<endl;
		//cout<<json_post<<endl;

		//.. and If http_post is ok --> delete the old file and rename the new as old
		if (esito==NICE)
		{
			remove(from_filename.c_str());
			rename( ("new_"+from_filename).c_str() , from_filename.c_str());

			/* OLD DELETE
			//Open New file as output
			ofstream ofs;
			ofs.open(from_filename, std::ofstream::out | std::ofstream::trunc);
			ofs.close();
			*/
		}
		else	//just delete the new file: the old file will be kept for next trial
		{
			remove( ("new_"+from_filename).c_str() );
		}
	}
	else
	{
		esito=ABORTED;	
	}
    

	return esito;

}















