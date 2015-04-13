#ifndef _VDT_IMAGEPROBE_H_
#define _VDT_IMAGEPROBE_H_
////////////////////
#include <opencv2/core/core.hpp>
#include "IProbe.hpp"

namespace vdt
{

	class ImageProbe : public IProbe<cv::Mat,cv::Mat>							//ABSTRACT CLASS: only sub-classes can be instantiated!
	{
	protected:
		virtual int mtype(){ return 1; }	//FIRST MAT GIVEN BY DRIVER!
		virtual cv::Mat convert(const cv::Mat& to_convert){ return to_convert; };
	public:
		ImageProbe
			(
			const polling_policy_t selected_polling = automatic,
			const processing_policy_t selected_processing = none	//if "none" or "custom" following parameters are ignored!
			) : IProbe<cv::Mat, cv::Mat>(selected_polling, selected_processing)
		{}

		ImageProbe
			(
			const unsigned int sample_rate,
			const polling_policy_t selected_polling = automatic,
			const processing_policy_t selected_processing = none	//if "none" or "custom" following parameters are ignored!
			) : IProbe<cv::Mat, cv::Mat>(sample_rate, selected_polling, selected_processing)
		{}


		virtual std::string stype(){ return "cv::Mat"; }
		virtual std::string sunits(){ return "pixels"; }


	};


}


/////////////////
#endif

