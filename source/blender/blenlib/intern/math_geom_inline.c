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

#ifndef __MATH_GEOM_INLINE_C__
#define __MATH_GEOM_INLINE_C__

#include "BLI_math.h"

#include <string.h>

/********************************** Polygons *********************************/

MINLINE float cross_tri_v2(const float v1[2], const float v2[2], const float v3[2])
{
	return (v1[0] - v2[0]) * (v2[1] - v3[1]) + (v1[1] - v2[1]) * (v3[0] - v2[0]);
}

MINLINE float area_tri_signed_v2(const float v1[2], const float v2[2], const float v3[2])
{
	return 0.5f * ((v1[0] - v2[0]) * (v2[1] - v3[1]) + (v1[1] - v2[1]) * (v3[0] - v2[0]));
}

MINLINE float area_tri_v2(const float v1[2], const float v2[2], const float v3[2])
{
	return fabsf(area_tri_signed_v2(v1, v2, v3));
}

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

MINLINE float dot_shsh(const float a[9], const float b[9])
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

MINLINE int max_axis_v3(const float vec[3])
{
	const float x = vec[0];
	const float y = vec[1];
	const float z = vec[2];
	return ((x > y) ?
	       ((x > z) ? 0 : 2) :
	       ((y > z) ? 1 : 2));
}

MINLINE int min_axis_v3(const float vec[3])
{
	const float x = vec[0];
	const float y = vec[1];
	const float z = vec[2];
	return ((x < y) ?
	       ((x < z) ? 0 : 2) :
	       ((y < z) ? 1 : 2));
}

/**
 * Simple method to find how many tri's we need when we already know the corner+poly count.
 *
 * Formula is:
 *
 *   tri = ((corner_count / poly_count) - 2) * poly_count;
 *
 * Use doubles since this is used for allocating and we
 * don't want float precision to give incorrect results.
 *
 * \param poly_count The number of ngon's/tris (1-2 sided faces will give incorrect results)
 * \param corner_count - also known as loops in BMesh/DNA
 */
MINLINE int poly_to_tri_count(const int poly_count, const int corner_count)
{
	if (poly_count != 0) {
		const double poly_count_d   = (double)poly_count;
		const double corner_count_d = (double)corner_count;
		BLI_assert(poly_count   > 0);
		BLI_assert(corner_count > 0);
		return (int)((((corner_count_d / poly_count_d) - 2.0) * poly_count_d) + 0.5);
	}
	else {
		return 0;
	}
}

MINLINE float plane_point_side_v3(const float plane[4], const float co[3])
{
	return dot_v3v3(co, plane) + plane[3];
}

#endif /* __MATH_GEOM_INLINE_C__ */
