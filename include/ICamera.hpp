#ifndef _VDT_ICAMERA_H_
#define _VDT_ICAMERA_H_
////////////////////
#include <opencv2/core/core.hpp>
#include "IInputDevice.hpp"


namespace vdt
{

	template <class raw_data_type>
	class ICamera : public vdt::IInputDevice < raw_data_type, cv::Mat >
	{

	private:
		bool active = false;

	protected:

		bool isActive()
		{
			return active;
		}

		virtual bool ready()				//SPECIALIZED: implements algorithms to counter camera hardware limits:
											//	(base class) Minimum delay between requests
											//	(extension)  Device IS active (if not is activated)
											//CAN BE FURTHER EXTENDED BY DERIVED CLASSES!!
		{
			bool device_ready = false;

			//(1) Check base class/driver constraint
			bool base_ready = vdt::IInputDevice<raw_data_type, cv::Mat>::ready();	//Verifico le condizioni del driver di base.

			if (base_ready)							//Se sono verificate, procedo con quelle specifiche
			{
				//(2) Check if device is physically plugged in
				if (!active)								//Se la device non è attualmente attiva...
				{
					std::cerr << "  D| Camera device not active. Activating..." << std::endl;
					active = activate();
					if(active) std::cerr << "  D| Camera device is now active" << std::endl;
					else std::cerr << "  D| Failed to activate camera" << std::endl;
				}

				device_ready = active;

			}

			return device_ready;				//Solo se (1) e (2) vere allora ready() ritorna TRUE!
		}

		virtual bool activate() = 0;		//MUST IMPLEMENT: procedure to open device, return result status
		virtual bool deactivate() = 0;		//MUST IMPLEMENT: procedure to close device, return result status

		virtual int recv_measure() = 0;     //MUST IMPLEMENT: should return ERROR if the read failed and measure is not valid, NICE otherwise.
		//To mark a measure as not valid, a specific "INVALID" value has been defined in config.h

	public:

		ICamera(const unsigned int min_delay = HARDWARE_DELAY) :IInputDevice<raw_data_type, cv::Mat>(min_delay){}

		~ICamera()
		{
			if (active) close();
		}

		void init()
		{
			std::lock_guard<std::mutex> access(rw);
			active = activate();
		}

		void close()
		{
			std::lock_guard<std::mutex> access(rw);
			active = deactivate();
		}

		virtual cv::Mat invalidValue()
		{
			cv::Mat emptyMat;
			return emptyMat;
		}

		virtual bool isValid(cv::Mat& elem)
		{
			return (elem.data != nullptr);
		}

	};

	//Include separate implementation
	//#include "ICamera.tpp"

}


////////////////
#endif