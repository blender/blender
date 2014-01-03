/**
 */

/*

*
* Template Numerical Toolkit (TNT): Linear Algebra Module
*
* Mathematical and Computational Sciences Division
* National Institute of Technology,
* Gaithersburg, MD USA
*
*
* This software was developed at the National Institute of Standards and
* Technology (NIST) by employees of the Federal Government in the course
* of their official duties. Pursuant to title 17 Section 105 of the
* United States Code, this software is not subject to copyright protection
* and is in the public domain.  The Template Numerical Toolkit (TNT) is
* an experimental system.  NIST assumes no responsibility whatsoever for
* its use by other parties, and makes no guarantees, expressed or implied,
* about its quality, reliability, or any other characteristic.
*
* BETA VERSION INCOMPLETE AND SUBJECT TO CHANGE
* see http://math.nist.gov/tnt for latest updates.
*
*/



// Header file for scalar math functions

#ifndef TNTMATH_H
#define TNTMATH_H

// conventional functions required by several matrix algorithms

#if defined(_MSC_VER) && (_MSC_VER < 1800)
#define hypot _hypot
#endif

namespace TNT 
{

struct TNTException {
        int i;
};


inline double abs(double t)
{
    return ( t > 0 ? t : -t);
}

inline double min(double a, double b)
{
    return (a < b ? a : b);
}

inline double max(double a, double b)
{
    return (a > b ? a : b);
}

inline float abs(float t)
{
    return ( t > 0 ? t : -t);
}

inline float min(float a, float b)
{
    return (a < b ? a : b);
}

inline int min(int a, int b)
{
    return (a < b ? a : b);
}

inline int max(int a, int b)
{
    return (a > b ? a : b);
}

inline float max(float a, float b)
{
    return (a > b ? a : b);
}

inline double sign(double a)
{
    return (a > 0 ? 1.0 : -1.0);
}

inline double sign(double a,double b) {
	return (b >= 0.0 ? TNT::abs(a) : -TNT::abs(a));
}

inline float sign(float a,float b) {
	return (b >= 0.0f ? TNT::abs(a) : -TNT::abs(a));
}

inline float sign(float a)
{
    return (a > 0.0 ? 1.0f : -1.0f);
}

inline float pythag(float a, float b)
{
	float absa,absb;
	absa = abs(a);
	absb = abs(b);

	if (absa > absb) {
		float sqr = absb/absa;
		sqr *= sqr;
		return absa * float(sqrt(1 + sqr));
	} else {
		if (absb > float(0)) {
			float sqr = absa/absb;
			sqr *= sqr;
			return absb * float(sqrt(1 + sqr));
		} else {
			return float(0);
		}
	}
}

inline double pythag(double a, double b)
{
	double absa,absb;
	absa = abs(a);
	absb = abs(b);

	if (absa > absb) {
		double sqr = absb/absa;
		sqr *= sqr;
		return absa * double(sqrt(1 + sqr));
	} else {

		if (absb > double(0)) {	
			double sqr = absa/absb;
			sqr *= sqr;
			return absb * double(sqrt(1 + sqr));
		} else {
			return double(0);
		}
	}
}


} /* namespace TNT */

#endif /* TNTMATH_H */

