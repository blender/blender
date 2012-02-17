/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: some of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 * */

/** \file blender/blenlib/intern/math_base_inline.c
 *  \ingroup bli
 */


#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "BLI_math.h"

#ifndef BLI_MATH_BASE_INLINE_H
#define BLI_MATH_BASE_INLINE_H

/* A few small defines. Keep'em local! */
#define SMALL_NUMBER	1.e-8f

MINLINE float sqrt3f(float f)
{
	if(f==0.0f) return 0.0f;
	if(f<0) return (float)(-exp(log(-f)/3));
	else return (float)(exp(log(f)/3));
}

MINLINE double sqrt3d(double d)
{
	if(d==0.0) return 0;
	if(d<0) return -exp(log(-d)/3);
	else return exp(log(d)/3);
}

MINLINE float saacos(float fac)
{
	if(fac<= -1.0f) return (float)M_PI;
	else if(fac>=1.0f) return 0.0;
	else return (float)acos(fac);
}

MINLINE float saasin(float fac)
{
	if(fac<= -1.0f) return (float)-M_PI/2.0f;
	else if(fac>=1.0f) return (float)M_PI/2.0f;
	else return (float)asin(fac);
}

MINLINE float sasqrt(float fac)
{
	if(fac<=0.0f) return 0.0f;
	return (float)sqrt(fac);
}

MINLINE float saacosf(float fac)
{
	if(fac<= -1.0f) return (float)M_PI;
	else if(fac>=1.0f) return 0.0f;
	else return (float)acosf(fac);
}

MINLINE float saasinf(float fac)
{
	if(fac<= -1.0f) return (float)-M_PI/2.0f;
	else if(fac>=1.0f) return (float)M_PI/2.0f;
	else return (float)asinf(fac);
}

MINLINE float sasqrtf(float fac)
{
	if(fac<=0.0f) return 0.0f;
	return (float)sqrtf(fac);
}

MINLINE float interpf(float target, float origin, float fac)
{
	return (fac*target) + (1.0f-fac)*origin;
}

/* useful to calculate an even width shell, by taking the angle between 2 planes.
 * The return value is a scale on the offset.
 * no angle between planes is 1.0, as the angle between the 2 planes approches 180d
 * the distance gets very high, 180d would be inf, but this case isn't valid */
MINLINE float shell_angle_to_dist(const float angle)
{
	return (angle < SMALL_NUMBER) ? 1.0f : fabsf(1.0f / cosf(angle));
}

/* used for zoom values*/
MINLINE float power_of_2(float val)
{
	return (float)pow(2.0, ceil(log((double)val) / M_LN2));
}

MINLINE int is_power_of_2_i(int n)
{
	return (n & (n - 1)) == 0;
}

MINLINE int power_of_2_max_i(int n)
{
	if (is_power_of_2_i(n))
		return n;

	while(!is_power_of_2_i(n))
		n = n & (n - 1);

	return n * 2;
}

MINLINE int power_of_2_min_i(int n)
{
	while (!is_power_of_2_i(n))
		n = n & (n - 1);

	return n;
}


MINLINE float minf(float a, float b)
{
	return (a < b)? a: b;
}

MINLINE float maxf(float a, float b)
{
	return (a > b)? a: b;
}

MINLINE float signf(float f)
{
	return (f < 0.f)? -1.f: 1.f;
}


#endif /* BLI_MATH_BASE_INLINE_H */

