
#include "functions.h"
#include <iomanip>
#include <algorithm>


namespace sdrf
{

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




	/////////////////////////////
	// GENERIC UTILITY FUNCTIONS


	///////////////////
	// NOT ZERO CHECK
	//When check_not_zero finds <0, makes true the static variable:
	static bool zero_found = false;
	//Utility function: checks if at least one of a few values is 0 or less
	//Returns the same value.
	int check_not_zero(int value)
	{
		if (value <= 0) zero_found = true;
		return value;
	}


	///////////////////
	// GET TIMESTAMP OF NOW
	std::string getTimeStamp(){

		time_t now;
		time(&now);
		char buf[sizeof "2011-10-08T07:07:09Z"];
		strftime(buf, sizeof buf, "%FT%TZ", gmtime(&now));
		std::stringstream ss;
		std::string s;
		ss << buf;
		ss >> s;
		return s;
	}


	///////////////////
	// SWAP commas (,) WITH dots (.)
	std::string dotnot(std::string input)
	{
		std::replace(input.begin(), input.end(), ',', '.');
		return input;
	}

}
















