#ifndef _VDT_UTILS_OMV_H_
#define _VDT_UTILS_OMV_H_


/*
 * OMV.h
 *
 * Class to handle the online mean and variance using the Knuth/Welford algorithm
 *
def online_variance(data):
    n = 0
    mean = 0
    M2 = 0
    for x in data:
        n = n + 1
        delta = x - mean
        mean = mean + delta/n
        M2 = M2 + delta*(x - mean)
    variance = M2/(n - 1)
    return variance

This algorithm is much less prone to loss of precision due to massive cancellation,
but might not be as efficient because of the division operation inside the loop.
 * In our case it's interesting to calculate mean and variance online for a most efficient
 * algorithm it's
 *
 *  Created on: Nov 19, 2013
 *      Author: Gabriele Consiglio
 */

namespace vdt
{
	namespace utils
	{

		class OMV {
		private:
			unsigned int n;
			double mean;
			double M2;
			double min;
			bool minCalc;
		public:
			OMV(const bool use_min_offset = false);
			virtual ~OMV();
			void reset();
			void add(double x);
			double getMean();
			double getVariance();
			double getMin();
			unsigned int getSampleNumber();
		};


	}
}


#endif /* _VDT_OMV_H_ */
