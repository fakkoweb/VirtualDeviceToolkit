
#include "sensors.h"



//specified in...
#include "functions.h"
/////////////////////////////
// GENERIC MATH FUNCTIONS

double faverage(double* array, int dim)
{
    return 3;
}
double fvariance(double* array, int dim)
{
    return 3;
}




//////////////////////////////////
// SPECIFIC SENSOR MATH FUNCTIONS

double TempSensor::convert(uint16_t raw)
{

	double real_T = ((raw * 165)/T_H_DIV)-40;

    	return real_T;
    
}

double HumidSensor::convert(uint16_t raw)
{

	double real_H = (raw *100)/T_H_DIV;
    
    	return real_H;
}


double DustSensor::convert(uint16_t raw)
{
//	const double minDust = -0.08;
	double voltDust = raw*3.3/1000;
	double realDust = voltDust*0.17-0.08;//-minDust;
	
	return realDust;  
}




