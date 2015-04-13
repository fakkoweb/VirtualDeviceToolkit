#ifndef _VDT_OPENCVCAMERA_H_
#define _VDT_OPENCVCAMERA_H_
////////////////////

#include "ICamera.hpp"
#include <opencv2/highgui/highgui.hpp>


// This is the data structure that vdt gives to the user.
// More contextual variables to the frame can be added freely, but they should be relevant/set for each new frame

namespace vdt
{

	typedef struct
	{
		cv::Mat lastFrame;
	} OpenCVCameraData;


	class OpenCVCamera : public ICamera < OpenCVCameraData >
	{

	protected:
		uint8_t cameraIndex;
		uint16_t videoWidth;
		uint16_t videoHeight;
		float widthToDistanceRatio;
		float heightToDistanceRatio;
		cv::VideoCapture camera;

		virtual bool ready();				//SPECIALIZED: implements algorithms to counter camera hardware limits:
											//	(base class) IInputDevice + ICamera checks (time delay + activity checks)
											//	(extension)  OpenCVCamera (lastFrame = cv::Mat::zero when device not ready)

		virtual bool activate();			//IMPLEMENTED: procedure to open device, return result status (CAN BE OVERRIDDEN)
		virtual bool deactivate();			//IMPLEMENTED: procedure to close device, return result status (CAN BE OVERRIDDEN)

		virtual int recv_measure();			//IMPLEMENTED: writes a new cv::Mat in m.lastFrame from device
		//Returns ERROR if the read failed and measure is not valid, NICE otherwise.
		//To mark a measure as not valid, a specific "INVALID" value has been defined in config.h
		//(CAN BE OVERRIDDEN)

	public:

		explicit OpenCVCamera(const unsigned int _cameraIndex = 0, const unsigned int min_delay = HARDWARE_DELAY);

		//see init() + close() from ICamera
		//see request() + request_all() from IInputDevice
	};

}



////////////////
#endif


