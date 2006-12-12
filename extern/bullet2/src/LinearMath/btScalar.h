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

#include <cstdlib>
#include <cfloat>
#include <float.h>

#ifdef WIN32

		#if defined(__MINGW32__) || defined(__CYGWIN__)
			#define SIMD_FORCE_INLINE inline
			#define ATTRIBUTE_ALIGNED16(a) a
		#else
			#pragma warning(disable:4530)
			#pragma warning(disable:4996)
			#pragma warning(disable:4786)
			#define SIMD_FORCE_INLINE __forceinline
			#define ATTRIBUTE_ALIGNED16(a) __declspec(align(16)) a
		#endif //__MINGW32__

		#include <assert.h>
		#define btAssert assert
#else
	
	//non-windows systems

		#define SIMD_FORCE_INLINE inline
		#define ATTRIBUTE_ALIGNED16(a) a
		#ifndef assert
		#include <assert.h>
		#endif
		#define btAssert assert
#endif



typedef float    btScalar;

///older compilers (gcc 3.x) and Sun needs double versions of srqt etc.
///exclude Apple Intel (it's assumed to be a Macbook or newer Intel Dual Core processor)
#if defined (__sun) || defined (__sun__) || defined (__sparc) || (defined (__APPLE__) && ! defined (__i386__))
//use slow double float precision operation on those platforms
		
SIMD_FORCE_INLINE btScalar btSqrt(btScalar x) { return sqrt(x); }
SIMD_FORCE_INLINE btScalar btFabs(btScalar x) { return fabs(x); }
SIMD_FORCE_INLINE btScalar btCos(btScalar x) { return cos(x); }
SIMD_FORCE_INLINE btScalar btSin(btScalar x) { return sin(x); }
SIMD_FORCE_INLINE btScalar btTan(btScalar x) { return tan(x); }
SIMD_FORCE_INLINE btScalar btAcos(btScalar x) { return acos(x); }
SIMD_FORCE_INLINE btScalar btAsin(btScalar x) { return asin(x); }
SIMD_FORCE_INLINE btScalar btAtan(btScalar x) { return atan(x); }
SIMD_FORCE_INLINE btScalar btAtan2(btScalar x, btScalar y) { return atan2(x, y); }
SIMD_FORCE_INLINE btScalar btExp(btScalar x) { return exp(x); }
SIMD_FORCE_INLINE btScalar btLog(btScalar x) { return log(x); }
SIMD_FORCE_INLINE btScalar btPow(btScalar x,btScalar y) { return pow(x,y); }

#else
		
SIMD_FORCE_INLINE btScalar btSqrt(btScalar x) { return sqrtf(x); }
SIMD_FORCE_INLINE btScalar btFabs(btScalar x) { return fabsf(x); }
SIMD_FORCE_INLINE btScalar btCos(btScalar x) { return cosf(x); }
SIMD_FORCE_INLINE btScalar btSin(btScalar x) { return sinf(x); }
SIMD_FORCE_INLINE btScalar btTan(btScalar x) { return tanf(x); }
SIMD_FORCE_INLINE btScalar btAcos(btScalar x) { return acosf(x); }
SIMD_FORCE_INLINE btScalar btAsin(btScalar x) { return asinf(x); }
SIMD_FORCE_INLINE btScalar btAtan(btScalar x) { return atanf(x); }
SIMD_FORCE_INLINE btScalar btAtan2(btScalar x, btScalar y) { return atan2f(x, y); }
SIMD_FORCE_INLINE btScalar btExp(btScalar x) { return expf(x); }
SIMD_FORCE_INLINE btScalar btLog(btScalar x) { return logf(x); }
SIMD_FORCE_INLINE btScalar btPow(btScalar x,btScalar y) { return powf(x,y); }
	
#endif


#define SIMD_2_PI         6.283185307179586232f
#define SIMD_PI           (SIMD_2_PI * btScalar(0.5f))
#define SIMD_HALF_PI      (SIMD_2_PI * btScalar(0.25f))
#define SIMD_RADS_PER_DEG (SIMD_2_PI / btScalar(360.0f))
#define SIMD_DEGS_PER_RAD  (btScalar(360.0f) / SIMD_2_PI)
#define SIMD_EPSILON      FLT_EPSILON
#define SIMD_INFINITY     FLT_MAX

SIMD_FORCE_INLINE bool      btFuzzyZero(btScalar x) { return btFabs(x) < SIMD_EPSILON; }

SIMD_FORCE_INLINE bool	btEqual(btScalar a, btScalar eps) {
	return (((a) <= eps) && !((a) < -eps));
}
SIMD_FORCE_INLINE bool	btGreaterEqual (btScalar a, btScalar eps) {
	return (!((a) <= eps));
}

/*SIMD_FORCE_INLINE btScalar btCos(btScalar x) { return cosf(x); }
SIMD_FORCE_INLINE btScalar btSin(btScalar x) { return sinf(x); }
SIMD_FORCE_INLINE btScalar btTan(btScalar x) { return tanf(x); }
SIMD_FORCE_INLINE btScalar btAcos(btScalar x) { return acosf(x); }
SIMD_FORCE_INLINE btScalar btAsin(btScalar x) { return asinf(x); }
SIMD_FORCE_INLINE btScalar btAtan(btScalar x) { return atanf(x); }
SIMD_FORCE_INLINE btScalar btAtan2(btScalar x, btScalar y) { return atan2f(x, y); }
*/

SIMD_FORCE_INLINE int       btIsNegative(btScalar x) {
    return x < 0.0f ? 1 : 0;
}

SIMD_FORCE_INLINE btScalar btRadians(btScalar x) { return x * SIMD_RADS_PER_DEG; }
SIMD_FORCE_INLINE btScalar btDegrees(btScalar x) { return x * SIMD_DEGS_PER_RAD; }

#define BT_DECLARE_HANDLE(name) typedef struct name##__ { int unused; } *name


#endif //SIMD___SCALAR_H
