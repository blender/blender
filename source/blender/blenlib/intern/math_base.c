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

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "BLI_math.h"

/* A few small defines. Keep'em local! */
#define SMALL_NUMBER	1.e-8

float sqrt3f(float f)
{
	if(f==0.0) return 0;
	if(f<0) return (float)(-exp(log(-f)/3));
	else return (float)(exp(log(f)/3));
}

double sqrt3d(double d)
{
	if(d==0.0) return 0;
	if(d<0) return -exp(log(-d)/3);
	else return exp(log(d)/3);
}

float saacos(float fac)
{
	if(fac<= -1.0f) return (float)M_PI;
	else if(fac>=1.0f) return 0.0;
	else return (float)acos(fac);
}

float saasin(float fac)
{
	if(fac<= -1.0f) return (float)-M_PI/2.0f;
	else if(fac>=1.0f) return (float)M_PI/2.0f;
	else return (float)asin(fac);
}

float sasqrt(float fac)
{
	if(fac<=0.0) return 0.0;
	return (float)sqrt(fac);
}

float saacosf(float fac)
{
	if(fac<= -1.0f) return (float)M_PI;
	else if(fac>=1.0f) return 0.0f;
	else return (float)acosf(fac);
}

float saasinf(float fac)
{
	if(fac<= -1.0f) return (float)-M_PI/2.0f;
	else if(fac>=1.0f) return (float)M_PI/2.0f;
	else return (float)asinf(fac);
}

float sasqrtf(float fac)
{
	if(fac<=0.0) return 0.0;
	return (float)sqrtf(fac);
}

float interpf(float target, float origin, float fac)
{
	return (fac*target) + (1.0f-fac)*origin;
}

/* useful to calculate an even width shell, by taking the angle between 2 planes.
 * The return value is a scale on the offset.
 * no angle between planes is 1.0, as the angle between the 2 planes approches 180d
 * the distance gets very high, 180d would be inf, but this case isn't valid */
float shell_angle_to_dist(const float angle)
{
	return (angle < SMALL_NUMBER) ? 1.0f : fabsf(1.0f / cosf(angle * (M_PI/180.0f)));
}

/* used for zoom values*/
float power_of_2(float val)
{
	return (float)pow(2, ceil(log(val) / log(2)));
}

/* from python 3.1 floatobject.c
 * ndigits must be between 0 and 21 */
double double_round(double x, int ndigits) {
	double pow1, pow2, y, z;
	if (ndigits >= 0) {
		pow1 = pow(10.0, (double)ndigits);
		pow2 = 1.0;
		y = (x*pow1)*pow2;
		/* if y overflows, then rounded value is exactly x */
		if (!finite(y))
			return x;
	}
	else {
		pow1 = pow(10.0, (double)-ndigits);
		pow2 = 1.0; /* unused; silences a gcc compiler warning */
		y = x / pow1;
	}

	z = round(y);
	if (fabs(y-z) == 0.5)
		/* halfway between two integers; use round-half-even */
		z = 2.0*round(y/2.0);

	if (ndigits >= 0)
		z = (z / pow2) / pow1;
	else
		z *= pow1;

	/* if computation resulted in overflow, raise OverflowError */
	return z;
}
