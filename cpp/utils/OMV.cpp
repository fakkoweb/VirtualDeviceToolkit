/*
 * OMV.cpp
 *
 *  Created on: Nov 19, 2013
 *      Author: gabriele
 */
/*class OMV {
private:
	int n;
	double mean;
	double M2;
	double min;
public:
	OMV();
	virtual ~OMV();
	void reset();
	void add(double x_i,bool min);
	double getMean();
	double getVariance();
	double getMin();
	double setMin();
};*/
#include "utils/OMV.h"
#include <limits>


namespace vdt
{
	namespace utils
	{


		/*
		*  OMV::OMV(bool aMin=false)
		*	Il costruttore prende come variabile opzionale aMin.
		*	se aMin è settato a true la classe si occupa anche di calcora il minimo
		*	fino al prossimo reset;
		*
		*/
		OMV::OMV(const bool use_min_offset) {
			minCalc = use_min_offset;
			reset();
		}

		OMV::~OMV() {
			// TODO Auto-generated destructor stub
		}

		/*
		* void OMV::reset(double min=0)
		* il reset ha un argomento opzionale che permette di settare
		* al reset un minimo personalizzato
		*/

		void OMV::reset() {
			n = 0;
			mean = 0;
			M2 = 0;
			min = std::numeric_limits<double>::max(); //the highest possible number, so first value will be always stored as min
		}

		/*
		*	void OMV::add(double x)
		*	la funzione aggiunge un double al calcolo della media online
		*/

		void OMV::add(double x) {
			n++;
			double delta = x - mean;
			mean = mean + delta / n;
			M2 = M2 + delta*(x - mean);

			if (this->min > x) this->min = x;
		}

		/*
		* 		double OMV::getMean()
		* 	Ritorna la media attuale;
		*
		*/

		double OMV::getMean() {
			if (minCalc && mean != 0)
			{
				return mean - min;
			}
			else
			{
				return mean;
			}
		}

		/*
		*		double OMV::getVariance()
		*	ritorna la varianza attuale
		*
		*/

		double OMV::getVariance() {
			if (M2 > 0) return M2 / (n - 1);
			else return 0;
		}

		/*
		* 		double OMV::getMin()
		* 	ritorna il minimo
		*
		*/
		double OMV::getMin(){
			return min;
		}

		/*
		* double OMV::getSampleNumber()
		*/
		unsigned int OMV::getSampleNumber(){
			return n;
		}


	}
}

