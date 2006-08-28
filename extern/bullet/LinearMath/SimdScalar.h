/*
Copyright (c) 2003-2006 Gino van den Bergen / Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/



#ifndef SIMD___SCALAR_H
#define SIMD___SCALAR_H

#include <math.h>
#undef max



#include <cstdlib>
#include <cfloat>
#include <float.h>

#ifdef WIN32

		#if defined(__MINGW32__) || defined(__CYGWIN__)
			#define SIMD_FORCE_INLINE inline
		#else
			#pragma warning(disable:4530)
			#pragma warning(disable:4996)
			#define SIMD_FORCE_INLINE __forceinline
		#endif //__MINGW32__
	
		//#define ATTRIBUTE_ALIGNED16(a) __declspec(align(16)) a
		#define ATTRIBUTE_ALIGNED16(a) a
		#include <assert.h>
		#define ASSERT assert
#else
	
	//non-windows systems

		#define SIMD_FORCE_INLINE inline
		#define ATTRIBUTE_ALIGNED16(a) a
		#ifndef assert
		#include <assert.h>
		#endif
		#define ASSERT assert
#endif



typedef float    SimdScalar;

#if defined (__sun) || defined (__sun__) || defined (__sparc) || defined (__APPLE__)
//use double float precision operation on those platforms for Blender
		
SIMD_FORCE_INLINE SimdScalar SimdSqrt(SimdScalar x) { return sqrt(x); }
SIMD_FORCE_INLINE SimdScalar SimdFabs(SimdScalar x) { return fabs(x); }
SIMD_FORCE_INLINE SimdScalar SimdCos(SimdScalar x) { return cos(x); }
SIMD_FORCE_INLINE SimdScalar SimdSin(SimdScalar x) { return sin(x); }
SIMD_FORCE_INLINE SimdScalar SimdTan(SimdScalar x) { return tan(x); }
SIMD_FORCE_INLINE SimdScalar SimdAcos(SimdScalar x) { return acos(x); }
SIMD_FORCE_INLINE SimdScalar SimdAsin(SimdScalar x) { return asin(x); }
SIMD_FORCE_INLINE SimdScalar SimdAtan(SimdScalar x) { return atan(x); }
SIMD_FORCE_INLINE SimdScalar SimdAtan2(SimdScalar x, SimdScalar y) { return atan2(x, y); }
SIMD_FORCE_INLINE SimdScalar SimdExp(SimdScalar x) { return exp(x); }
SIMD_FORCE_INLINE SimdScalar SimdLog(SimdScalar x) { return log(x); }
SIMD_FORCE_INLINE SimdScalar SimdPow(SimdScalar x,SimdScalar y) { return pow(x,y); }

#else
		
SIMD_FORCE_INLINE SimdScalar SimdSqrt(SimdScalar x) { return sqrtf(x); }
SIMD_FORCE_INLINE SimdScalar SimdFabs(SimdScalar x) { return fabsf(x); }
SIMD_FORCE_INLINE SimdScalar SimdCos(SimdScalar x) { return cosf(x); }
SIMD_FORCE_INLINE SimdScalar SimdSin(SimdScalar x) { return sinf(x); }
SIMD_FORCE_INLINE SimdScalar SimdTan(SimdScalar x) { return tanf(x); }
SIMD_FORCE_INLINE SimdScalar SimdAcos(SimdScalar x) { return acosf(x); }
SIMD_FORCE_INLINE SimdScalar SimdAsin(SimdScalar x) { return asinf(x); }
SIMD_FORCE_INLINE SimdScalar SimdAtan(SimdScalar x) { return atanf(x); }
SIMD_FORCE_INLINE SimdScalar SimdAtan2(SimdScalar x, SimdScalar y) { return atan2f(x, y); }
SIMD_FORCE_INLINE SimdScalar SimdExp(SimdScalar x) { return expf(x); }
SIMD_FORCE_INLINE SimdScalar SimdLog(SimdScalar x) { return logf(x); }
SIMD_FORCE_INLINE SimdScalar SimdPow(SimdScalar x,SimdScalar y) { return powf(x,y); }
	
#endif


const SimdScalar  SIMD_2_PI         = 6.283185307179586232f;
const SimdScalar  SIMD_PI           = SIMD_2_PI * SimdScalar(0.5f);
const SimdScalar  SIMD_HALF_PI		 = SIMD_2_PI * SimdScalar(0.25f);
const SimdScalar  SIMD_RADS_PER_DEG = SIMD_2_PI / SimdScalar(360.0f);
const SimdScalar  SIMD_DEGS_PER_RAD = SimdScalar(360.0f) / SIMD_2_PI;
const SimdScalar  SIMD_EPSILON      = FLT_EPSILON;
const SimdScalar  SIMD_INFINITY     = FLT_MAX;

SIMD_FORCE_INLINE bool      SimdFuzzyZero(SimdScalar x) { return SimdFabs(x) < SIMD_EPSILON; }

SIMD_FORCE_INLINE bool	SimdEqual(SimdScalar a, SimdScalar eps) {
	return (((a) <= eps) && !((a) < -eps));
}
SIMD_FORCE_INLINE bool	SimdGreaterEqual (SimdScalar a, SimdScalar eps) {
	return (!((a) <= eps));
}

/*SIMD_FORCE_INLINE SimdScalar SimdCos(SimdScalar x) { return cosf(x); }
SIMD_FORCE_INLINE SimdScalar SimdSin(SimdScalar x) { return sinf(x); }
SIMD_FORCE_INLINE SimdScalar SimdTan(SimdScalar x) { return tanf(x); }
SIMD_FORCE_INLINE SimdScalar SimdAcos(SimdScalar x) { return acosf(x); }
SIMD_FORCE_INLINE SimdScalar SimdAsin(SimdScalar x) { return asinf(x); }
SIMD_FORCE_INLINE SimdScalar SimdAtan(SimdScalar x) { return atanf(x); }
SIMD_FORCE_INLINE SimdScalar SimdAtan2(SimdScalar x, SimdScalar y) { return atan2f(x, y); }
*/

SIMD_FORCE_INLINE int       SimdSign(SimdScalar x) {
    return x < 0.0f ? -1 : x > 0.0f ? 1 : 0;
}

SIMD_FORCE_INLINE SimdScalar SimdRadians(SimdScalar x) { return x * SIMD_RADS_PER_DEG; }
SIMD_FORCE_INLINE SimdScalar SimdDegrees(SimdScalar x) { return x * SIMD_DEGS_PER_RAD; }



#endif //SIMD___SCALAR_H
