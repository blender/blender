/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: some of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 * */

#ifndef BLI_MATH_BASE
#define BLI_MATH_BASE

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32
#define _USE_MATH_DEFINES
#endif

#include <math.h>

#ifndef M_PI
#define M_PI        3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2      1.57079632679489661923
#endif
#ifndef M_SQRT2
#define M_SQRT2     1.41421356237309504880
#endif
#ifndef M_SQRT1_2
#define M_SQRT1_2   0.70710678118654752440
#endif
#ifndef M_1_PI
#define M_1_PI      0.318309886183790671538
#endif
#ifndef M_E
#define M_E             2.7182818284590452354
#endif
#ifndef M_LOG2E
#define M_LOG2E         1.4426950408889634074
#endif
#ifndef M_LOG10E
#define M_LOG10E        0.43429448190325182765
#endif
#ifndef M_LN2
#define M_LN2           0.69314718055994530942
#endif
#ifndef M_LN10
#define M_LN10          2.30258509299404568402
#endif

#ifndef sqrtf
#define sqrtf(a) ((float)sqrt(a))
#endif
#ifndef powf
#define powf(a, b) ((float)pow(a, b))
#endif
#ifndef cosf
#define cosf(a) ((float)cos(a))
#endif
#ifndef sinf
#define sinf(a) ((float)sin(a))
#endif
#ifndef acosf
#define acosf(a) ((float)acos(a))
#endif
#ifndef asinf
#define asinf(a) ((float)asin(a))
#endif
#ifndef atan2f
#define atan2f(a, b) ((float)atan2(a, b))
#endif
#ifndef tanf
#define tanf(a) ((float)tan(a))
#endif
#ifndef atanf
#define atanf(a) ((float)atan(a))
#endif
#ifndef floorf
#define floorf(a) ((float)floor(a))
#endif
#ifndef ceilf
#define ceilf(a) ((float)ceil(a))
#endif
#ifndef fabsf
#define fabsf(a) ((float)fabs(a))
#endif
#ifndef logf
#define logf(a) ((float)log(a))
#endif
#ifndef expf
#define expf(a) ((float)exp(a))
#endif
#ifndef fmodf
#define fmodf(a, b) ((float)fmod(a, b))
#endif

#ifdef WIN32
#ifndef FREE_WINDOWS
#define isnan(n) _isnan(n)
#define finite _finite
#endif
#endif

#ifndef SWAP
#define SWAP(type, a, b)	{ type sw_ap; sw_ap=(a); (a)=(b); (b)=sw_ap; }
#endif

#ifndef CLAMP
#define CLAMP(a, b, c)		if((a)<(b)) (a)=(b); else if((a)>(c)) (a)=(c)
#endif

/******************************* Float ******************************/

float sqrt3f(float f);
double sqrt3d(double d);

float saacosf(float f);
float saasinf(float f);
float sasqrtf(float f);
float saacos(float fac);
float saasin(float fac);
float sasqrt(float fac);

float interpf(float a, float b, float t);

float power_of_2(float f);

float shell_angle_to_dist(float angle);

double double_round(double x, int ndigits);

#ifdef __cplusplus
}
#endif

#endif /* BLI_MATH_BASE */

