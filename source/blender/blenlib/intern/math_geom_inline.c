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

/** \file blender/blenlib/intern/math_geom_inline.c
 *  \ingroup bli
 */


#include "BLI_math.h"

#ifndef __MATH_GEOM_INLINE_C__
#define __MATH_GEOM_INLINE_C__

/****************************** Spherical Harmonics **************************/

MINLINE void zero_sh(float r[9])
{
	memset(r, 0, sizeof(float) * 9);
}

MINLINE void copy_sh_sh(float r[9], const float a[9])
{
	memcpy(r, a, sizeof(float) * 9);
}

MINLINE void mul_sh_fl(float r[9], const float f)
{
	int i;

	for (i = 0; i < 9; i++)
		r[i] *= f;
}

MINLINE void add_sh_shsh(float r[9], const float a[9], const float b[9])
{
	int i;

	for (i = 0; i < 9; i++)
		r[i] = a[i] + b[i];
}

MINLINE float dot_shsh(float a[9], float b[9])
{
	float r = 0.0f;
	int i;

	for (i = 0; i < 9; i++)
		r += a[i] * b[i];

	return r;
}

MINLINE float diffuse_shv3(float sh[9], const float v[3])
{
	/* See formula (13) in:
	 * "An Efficient Representation for Irradiance Environment Maps" */
	static const float c1 = 0.429043f, c2 = 0.511664f, c3 = 0.743125f;
	static const float c4 = 0.886227f, c5 = 0.247708f;
	float x, y, z, sum;

	x = v[0];
	y = v[1];
	z = v[2];

	sum = c1 * sh[8] * (x * x - y * y);
	sum += c3 * sh[6] * z * z;
	sum += c4 * sh[0];
	sum += -c5 * sh[6];
	sum += 2.0f * c1 * (sh[4] * x * y + sh[7] * x * z + sh[5] * y * z);
	sum += 2.0f * c2 * (sh[3] * x + sh[1] * y + sh[2] * z);

	return sum;
}

MINLINE void vec_fac_to_sh(float r[9], const float v[3], const float f)
{
	/* See formula (3) in:
	 * "An Efficient Representation for Irradiance Environment Maps" */
	float sh[9], x, y, z;

	x = v[0];
	y = v[1];
	z = v[2];

	sh[0] = 0.282095f;

	sh[1] = 0.488603f * y;
	sh[2] = 0.488603f * z;
	sh[3] = 0.488603f * x;

	sh[4] = 1.092548f * x * y;
	sh[5] = 1.092548f * y * z;
	sh[6] = 0.315392f * (3.0f * z * z - 1.0f);
	sh[7] = 1.092548f * x * z;
	sh[8] = 0.546274f * (x * x - y * y);

	mul_sh_fl(sh, f);
	copy_sh_sh(r, sh);
}

MINLINE float eval_shv3(float sh[9], const float v[3])
{
	float tmp[9];

	vec_fac_to_sh(tmp, v, 1.0f);
	return dot_shsh(tmp, sh);
}

MINLINE void madd_sh_shfl(float r[9], const float sh[9], const float f)
{
	float tmp[9];

	copy_sh_sh(tmp, sh);
	mul_sh_fl(tmp, f);
	add_sh_shsh(r, r, tmp);
}

#endif /* __MATH_GEOM_INLINE_C__ */
