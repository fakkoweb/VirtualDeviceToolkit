#include "OpenCVCamera.h"


namespace vdt
{

	OpenCVCamera::OpenCVCamera(const unsigned int _cameraIndex, const unsigned int min_delay) : ICamera<OpenCVCameraData>(min_delay)
	{
		m.lastFrame = invalidValue();
		cameraIndex = _cameraIndex;
	}

	bool OpenCVCamera::ready()
	{
		bool device_ready = false;

		//(1) Check base class/driver constraint
		bool base_ready = vdt::ICamera<OpenCVCameraData>::ready();	//Verifico le condizioni del driver di base

		if (base_ready)
		{
			//m.lastFrame. = cv::Mat::zeros;
		}

		device_ready = base_ready;

		return device_ready;

	}

	// Try to activate camera: returns activation status
	bool OpenCVCamera::activate()
	{
		bool activated = false;
		if (!isActive())
		{
			cv::VideoCapture _camera(cameraIndex);
			camera = _camera;
			activated = camera.isOpened();
			if (activated) {
				camera.set(CV_CAP_PROP_FRAME_WIDTH, 1280);
				camera.set(CV_CAP_PROP_FRAME_HEIGHT, 720);
			}
		}
		else activated = true;
		
		return activated;
	}

	// Try to DEactivate camera: returns activation status
	bool OpenCVCamera::deactivate()
	{
		camera.release();
		m.lastFrame = invalidValue();
		return camera.isOpened();
	}

	int OpenCVCamera::recv_measure()
	{
		static int i = 0;
		camera >> m.lastFrame; // get a new frame from camera
		//m.lastFrame = cv::Mat::zeros(1280, 720, CV_8UC3);
		std::cout << "\nchiamata:" << ++i << std::endl;
		return VDT_VALUE_NICE;
	}

}