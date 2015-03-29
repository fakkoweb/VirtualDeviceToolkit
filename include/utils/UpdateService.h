#ifndef _SDRF_UTILS_UPDATESERVICE_H_
#define _SDRF_UTILS_UPDATESERVICE_H_
////////////////////

#include <map>
#include <list>
#include <thread>
#include <future>
#include <mutex>
#include <condition_variable>

namespace vdt
{
	namespace utils
	{


		enum freshness_policy_t
		{
			all,
			new_only
		};

		class UpdateService
		{

		private:

			//Declaring sync structures
			std::mutex rw;
			std::condition_variable newUpdateCondition;
			std::future<void> newUpdate;

			// Declaring "subscription" type
			typedef struct {
				bool new_update_available;
				unsigned short int subscription_age;
			} subscription;

			// Declaring SUBSCRIBERS LIST = thread_id <-> subscription
			std::map <std::thread::id, subscription>			subscribersList; 		//Lista dei thread che aspettano aggiornamenti
			std::map <std::thread::id, subscription>::iterator	subscribersList_p;		//Iteratore generico per accedere alla lista
			typedef std::pair <std::thread::id, subscription>	subscriberRow;			//Tipo riga per inserimento nelle map

		protected:

			//Functions for async mechanism, NOT THREAD SAFE calls
			bool consumeUpdate(std::map <std::thread::id, subscription>::iterator &subscriber);			//Returns and resets notification for a subscriber, if any
			void startUpdateDaemon();		//Start thread waiting for next update
			void broadcastNewUpdate();		//Spread new update notification among all subscribers

			void unsubscribe(std::map <std::thread::id, subscription>::iterator &too_old_subscriber);	//A subscribes is automatically deleted when subscription_age == 0
			void unsubscribeAll();			//Flush all subscribers now. Warning: new checkForUpdates() will register frequent subscribers again!

		public:

			UpdateService();
			~UpdateService();

			//NOTIFYING UPDATES
			void notifyNewUpdate();			//When caller releases a new update:
			//	- all synchronous users are awakened from "waitForNextUpdate()"
			//	- all asynchronous (subscribed) users will get TRUE from next "checkForUpdates()" call

			//WAITING FOR UPDATES (SYNCHRONOUS)
			void waitForNextUpdate();		//Makes the caller sleep until a new update is released (no subscription needed!)

			//CHECKING FOR UPDATES (ASYNCHRONOUS)
			bool checkForUpdates();			//If caller is subscribed to "subscribersList" returns:
			//	- TRUE if NEW updates were released since last time I called "checkForUpdates()"
			//	- FALSE if NO new updates were released since last time I called "checkForUpdates()"
			//If caller is not subscribed, it is implicitly subscribed with "all" policy, so TRUE is returned
			//Subscription and unsubscription are automatically manages in "checkForUpdates"...
			//...good manners are welcome though!
			void subscribe(freshness_policy_t freshness = all);		//manually adds caller to subscribers list
			//it is possible to specify whether the update prior to subscription must be considered ("all") or not ("new_only")
			void unsubscribe();				//removes caller from subscribers list (one less to notify new updates)

		};


	}
}

#endif