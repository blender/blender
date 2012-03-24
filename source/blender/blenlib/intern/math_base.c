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

/** \file blender/blenlib/intern/math_base.c
 *  \ingroup bli
 */



#include "BLI_math.h"

/* WARNING: MSVC compiling hack for double_round() */
#if (defined(WIN32) || defined(WIN64)) && !(defined(FREE_WINDOWS))

/* from python 3.1 pymath.c */
double copysign(double x, double y)
{
	/* use atan2 to distinguish -0. from 0. */
	if (y > 0. || (y == 0. && atan2(y, -1.) > 0.)) {
		return fabs(x);
	}
	else {
		return -fabs(x);
	}
}

/* from python 3.1 pymath.c */
double round(double x)
{
	double absx, y;
	absx = fabs(x);
	y = floor(absx);
	if (absx - y >= 0.5)
		y += 1.0;
	return copysign(y, x);
}
#else /* OpenSuse 11.1 seems to need this. */
double round(double x);
#endif


/* from python 3.1 floatobject.c
 * ndigits must be between 0 and 21 */
double double_round(double x, int ndigits)
{
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

