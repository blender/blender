/*
 * SOLID - Software Library for Interference Detection
 * 
 * Copyright (C) 2001-2003  Dtecta.  All rights reserved.
 *
 * This library may be distributed under the terms of the Q Public License
 * (QPL) as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This library may be distributed and/or modified under the terms of the
 * GNU General Public License (GPL) version 2 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 *
 * This library is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Commercial use or any other use of this library not covered by either 
 * the QPL or the GPL requires an additional license from Dtecta. 
 * Please contact info@dtecta.com for enquiries about the terms of commercial
 * use of this library.
 */

#ifndef MT_SCALAR_H
#define MT_SCALAR_H

#if defined (__sun__) || defined ( __sun ) || defined (__sparc) || defined (__sparc__) || defined (__sgi)
#include <math.h>
#include <float.h>
#else
#include <cmath>
#include <cstdlib>
#include <cfloat>
#endif

#undef max

#include "SOLID_types.h"

#include "GEN_MinMax.h"
#include "GEN_random.h"

template <typename Scalar>
struct Scalar_traits {};

template<>
struct Scalar_traits<float> {
	static float TwoTimesPi() { return 6.283185307179586232f; }
	static float epsilon() { return FLT_EPSILON; }
	static float max() { return FLT_MAX; }
	
	static float random() { return float(GEN_rand()) / float(GEN_RAND_MAX); }
#if defined (__sun) || defined (__sun__) || defined (__sparc) || defined (__APPLE__)
	static float sqrt(float x) { return ::sqrt(x); } 
	static float abs(float x) { return ::fabs(x); } 

	static float cos(float x) { return ::cos(x); } 
	static float sin(float x) { return ::sin(x); } 
	static float tan(float x) { return ::tan(x); } 

	static float acos(float x) { return ::acos(x); } 
	static float asin(float x) { return ::asin(x); } 
	static float atan(float x) { return ::atan(x); } 
	static float atan2(float x, float y) { return ::atan2(x, y); } 

	static float exp(float x) { return ::exp(x); } 
	static float log(float x) { return ::log(x); } 
	static float pow(float x, float y) { return ::pow(x, y); } 

#else
	static float sqrt(float x) { return ::sqrtf(x); } 
	static float abs(float x) { return ::fabsf(x); } 

	static float cos(float x) { return ::cosf(x); } 
	static float sin(float x) { return ::sinf(x); } 
	static float tan(float x) { return ::tanf(x); } 

	static float acos(float x) { return ::acosf(x); } 
	static float asin(float x) { return ::asinf(x); } 
	static float atan(float x) { return ::atanf(x); } 
	static float atan2(float x, float y) { return ::atan2f(x, y); } 

	static float exp(float x) { return ::expf(x); } 
	static float log(float x) { return ::logf(x); } 
	static float pow(float x, float y) { return ::powf(x, y); } 
#endif
};

template<>
struct Scalar_traits<double> {
	static double TwoTimesPi() { return 6.283185307179586232; }
	static double epsilon() { return DBL_EPSILON; }
	static double max() { return DBL_MAX; }
	
	static double random() { return double(GEN_rand()) / double(GEN_RAND_MAX); }
	static double sqrt(double x) { return ::sqrt(x); } 
	static double abs(double x) { return ::fabs(x); } 

	static double cos(double x) { return ::cos(x); } 
	static double sin(double x) { return ::sin(x); } 
	static double tan(double x) { return ::tan(x); } 

	static double acos(double x) { return ::acos(x); } 
	static double asin(double x) { return ::asin(x); } 
	static double atan(double x) { return ::atan(x); } 
	static double atan2(double x, double y) { return ::atan2(x, y); } 

	static double exp(double x) { return ::exp(x); } 
	static double log(double x) { return ::log(x); } 
	static double pow(double x, double y) { return ::pow(x, y); } 
};

#ifdef USE_TRACER
#include "MT_ScalarTracer.h"

#ifdef USE_DOUBLES
typedef MT_ScalarTracer<double>   MT_Scalar;
#else
typedef MT_ScalarTracer<float>    MT_Scalar;
#endif

#else

#ifdef USE_DOUBLES
typedef double   MT_Scalar;
#else
typedef float    MT_Scalar;
#endif

#endif


const MT_Scalar  MT_2_PI         = Scalar_traits<MT_Scalar>::TwoTimesPi();
const MT_Scalar  MT_PI           = MT_2_PI * MT_Scalar(0.5);
const MT_Scalar  MT_HALF_PI		 = MT_2_PI * MT_Scalar(0.25);
const MT_Scalar  MT_RADS_PER_DEG = MT_2_PI / MT_Scalar(360.0);
const MT_Scalar  MT_DEGS_PER_RAD = MT_Scalar(360.0) / MT_2_PI;

const MT_Scalar  MT_EPSILON      = Scalar_traits<MT_Scalar>::epsilon();
const MT_Scalar  MT_INFINITY     = Scalar_traits<MT_Scalar>::max();

inline MT_Scalar MT_random() { return  Scalar_traits<MT_Scalar>::random(); }
inline MT_Scalar MT_abs(MT_Scalar x) { return Scalar_traits<MT_Scalar>::abs(x); }
inline MT_Scalar MT_sqrt(MT_Scalar x) { return Scalar_traits<MT_Scalar>::sqrt(x); }

inline MT_Scalar MT_cos(MT_Scalar x) { return Scalar_traits<MT_Scalar>::cos(x); }
inline MT_Scalar MT_sin(MT_Scalar x) { return Scalar_traits<MT_Scalar>::sin(x); }
inline MT_Scalar MT_tan(MT_Scalar x) { return Scalar_traits<MT_Scalar>::tan(x); }

inline MT_Scalar MT_acos(MT_Scalar x) { return Scalar_traits<MT_Scalar>::acos(x); }
inline MT_Scalar MT_asin(MT_Scalar x) { return Scalar_traits<MT_Scalar>::asin(x); }
inline MT_Scalar MT_atan(MT_Scalar x) { return Scalar_traits<MT_Scalar>::atan(x); }
inline MT_Scalar MT_atan2(MT_Scalar x, MT_Scalar y) { return Scalar_traits<MT_Scalar>::atan2(x, y); }

inline MT_Scalar MT_radians(MT_Scalar x) { return x * MT_RADS_PER_DEG; }
inline MT_Scalar MT_degrees(MT_Scalar x) { return x * MT_DEGS_PER_RAD; }

#endif
