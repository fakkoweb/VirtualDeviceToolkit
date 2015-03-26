#include "utils/UpdateService.h"
#include "utils/functions.h"


namespace sdrf
{
	namespace utils
	{

		UpdateService::UpdateService()
		{
		}


		UpdateService::~UpdateService()
		{
		}

		/*
		void UpdateService::setJob( void (*input_job)() )
		{
		job = input_job;
		}

		void UpdateService::resetJob()
		{
		job = nullptr;
		}
		*/

		void UpdateService::subscribe(freshness_policy_t freshness)
		{
			//If this is first subscriber, start daemon for new updates
			if (subscribersList.empty())
			{
				startUpdateDaemon();
			}

			//Then subscribe myself, with the preferred policy
			subscription newSubscription;
			newSubscription.subscription_age = 100;
			switch (freshness)
			{
			case all:
				newSubscription.new_update_available = true;
				break;
			case new_only:
				newSubscription.new_update_available = false;
				break;
			default:
				newSubscription.new_update_available = true;
				break;
			}

			subscribersList.insert(subscriberRow(std::this_thread::get_id(), newSubscription));
		}

		void UpdateService::unsubscribe()
		{
			subscribersList.erase(std::this_thread::get_id());
		}

		void UpdateService::unsubscribe(std::map <std::thread::id, subscription>::iterator &too_old_subscriber)
		{
			subscribersList.erase(too_old_subscriber);
		}

		void UpdateService::unsubscribeAll()
		{
			subscribersList.clear();
		}






		bool UpdateService::consumeUpdate(std::map <std::thread::id, subscription>::iterator &subscriber)
		{
			bool new_update_available = subscriber->second.new_update_available;	//Save "new_update_available" flag
			subscriber->second.new_update_available = false;						//Reset "new_update_available" flag
			subscriber->second.subscription_age = 120;								//Restore/refill my subscription_age
			return new_update_available;											//Return saved "new_update_available" flag
		}

		void UpdateService::startUpdateDaemon()
		{
			newUpdate = async(std::launch::async,
				[this]() -> void
			{
				std::unique_lock<std::mutex> access(rw);
				newUpdateCondition.wait(access);
				return;
			}
			);
		}

		void UpdateService::broadcastNewUpdate()
		{
			std::map <std::thread::id, subscription>::iterator	s;
			for (size_t n = 0; n < subscribersList.size(); n++)
			{
				//signal update to a subscriber
				s->second.new_update_available = true;

				//subscribers garbage collection (decrement subscription_age: when it reaches 0, subscriber is removed from subscribersList)
				if (--(s->second.subscription_age) <= 0)
					unsubscribe(s);
			}
		}






		void UpdateService::notifyNewUpdate()
		{
			std::lock_guard<std::mutex> access(rw);
			newUpdateCondition.notify_all();
		}

		bool UpdateService::checkForUpdates()
		{
			bool newUpdateFound = false;
			std::map <std::thread::id, subscription>::iterator subscriberFound;

			std::lock_guard<std::mutex> access(rw);
			subscriberFound = subscribersList.find(std::this_thread::get_id());

			//Am I already subscribed?
			if (subscriberFound != subscribersList.end())
			{
				//YES: I am already subscribed, check if a new update has been published
				if (is_ready(newUpdate))
				{
					//yes: release update to everyone and consume it, then launch async request for next update 
					broadcastNewUpdate();										//new_update_available flag is set to TRUE for everyone!
					startUpdateDaemon();
				}
			}
			else
			{
				//NO: not subscribed yet. Subscribe and consume latest update (when subscribing is implicit, first update is always TRUE)
				subscribe();													//new_update_available flag is initialized to TRUE for me!
			}

			//Then consume update for me (if any)
			newUpdateFound = consumeUpdate(subscriberFound);

			return newUpdateFound;
		}


	}
}