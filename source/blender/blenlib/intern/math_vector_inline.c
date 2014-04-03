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

/** \file blender/blenlib/intern/math_vector_inline.c
 *  \ingroup bli
 */

#ifndef __MATH_VECTOR_INLINE_C__
#define __MATH_VECTOR_INLINE_C__

#include "BLI_math.h"

/********************************** Init *************************************/

MINLINE void zero_v2(float r[2])
{
	r[0] = 0.0f;
	r[1] = 0.0f;
}

MINLINE void zero_v3(float r[3])
{
	r[0] = 0.0f;
	r[1] = 0.0f;
	r[2] = 0.0f;
}

MINLINE void zero_v4(float r[4])
{
	r[0] = 0.0f;
	r[1] = 0.0f;
	r[2] = 0.0f;
	r[3] = 0.0f;
}

MINLINE void copy_v2_v2(float r[2], const float a[2])
{
	r[0] = a[0];
	r[1] = a[1];
}

MINLINE void copy_v3_v3(float r[3], const float a[3])
{
	r[0] = a[0];
	r[1] = a[1];
	r[2] = a[2];
}

MINLINE void copy_v4_v4(float r[4], const float a[4])
{
	r[0] = a[0];
	r[1] = a[1];
	r[2] = a[2];
	r[3] = a[3];
}

MINLINE void copy_v2_fl(float r[2], float f)
{
	r[0] = f;
	r[1] = f;
}

MINLINE void copy_v3_fl(float r[3], float f)
{
	r[0] = f;
	r[1] = f;
	r[2] = f;
}

MINLINE void copy_v4_fl(float r[4], float f)
{
	r[0] = f;
	r[1] = f;
	r[2] = f;
	r[3] = f;
}

/* short */
MINLINE void copy_v2_v2_char(char r[2], const char a[2])
{
	r[0] = a[0];
	r[1] = a[1];
}

MINLINE void copy_v3_v3_char(char r[3], const char a[3])
{
	r[0] = a[0];
	r[1] = a[1];
	r[2] = a[2];
}

MINLINE void copy_v4_v4_char(char r[4], const char a[4])
{
	r[0] = a[0];
	r[1] = a[1];
	r[2] = a[2];
	r[3] = a[3];
}

/* short */
MINLINE void zero_v3_int(int r[3])
{
	r[0] = 0;
	r[1] = 0;
	r[2] = 0;
}

MINLINE void copy_v2_v2_short(short r[2], const short a[2])
{
	r[0] = a[0];
	r[1] = a[1];
}

MINLINE void copy_v3_v3_short(short r[3], const short a[3])
{
	r[0] = a[0];
	r[1] = a[1];
	r[2] = a[2];
}

MINLINE void copy_v4_v4_short(short r[4], const short a[4])
{
	r[0] = a[0];
	r[1] = a[1];
	r[2] = a[2];
	r[3] = a[3];
}

/* int */
MINLINE void copy_v2_v2_int(int r[2], const int a[2])
{
	r[0] = a[0];
	r[1] = a[1];
}

MINLINE void copy_v3_v3_int(int r[3], const int a[3])
{
	r[0] = a[0];
	r[1] = a[1];
	r[2] = a[2];
}

MINLINE void copy_v4_v4_int(int r[4], const int a[4])
{
	r[0] = a[0];
	r[1] = a[1];
	r[2] = a[2];
	r[3] = a[3];
}

/* double -> float */
MINLINE void copy_v2fl_v2db(float r[2], const double a[2])
{
	r[0] = (float)a[0];
	r[1] = (float)a[1];
}

MINLINE void copy_v3fl_v3db(float r[3], const double a[3])
{
	r[0] = (float)a[0];
	r[1] = (float)a[1];
	r[2] = (float)a[2];
}

MINLINE void copy_v4fl_v4db(float r[4], const double a[4])
{
	r[0] = (float)a[0];
	r[1] = (float)a[1];
	r[2] = (float)a[2];
	r[3] = (float)a[3];
}

/* float -> double */
MINLINE void copy_v2db_v2fl(double r[2], const float a[2])
{
	r[0] = (double)a[0];
	r[1] = (double)a[1];
}

MINLINE void copy_v3db_v3fl(double r[3], const float a[3])
{
	r[0] = (double)a[0];
	r[1] = (double)a[1];
	r[2] = (double)a[2];
}

MINLINE void copy_v4db_v4fl(double r[4], const float a[4])
{
	r[0] = (double)a[0];
	r[1] = (double)a[1];
	r[2] = (double)a[2];
	r[3] = (double)a[3];
}

MINLINE void swap_v2_v2(float a[2], float b[2])
{
	SWAP(float, a[0], b[0]);
	SWAP(float, a[1], b[1]);
}

MINLINE void swap_v3_v3(float a[3], float b[3])
{
	SWAP(float, a[0], b[0]);
	SWAP(float, a[1], b[1]);
	SWAP(float, a[2], b[2]);
}

MINLINE void swap_v4_v4(float a[4], float b[4])
{
	SWAP(float, a[0], b[0]);
	SWAP(float, a[1], b[1]);
	SWAP(float, a[2], b[2]);
	SWAP(float, a[3], b[3]);
}

/* float args -> vec */
MINLINE void copy_v2_fl2(float v[2], float x, float y)
{
	v[0] = x;
	v[1] = y;
}

MINLINE void copy_v3_fl3(float v[3], float x, float y, float z)
{
	v[0] = x;
	v[1] = y;
	v[2] = z;
}

MINLINE void copy_v4_fl4(float v[4], float x, float y, float z, float w)
{
	v[0] = x;
	v[1] = y;
	v[2] = z;
	v[3] = w;
}

/********************************* Arithmetic ********************************/

MINLINE void add_v2_fl(float r[2], float f)
{
	r[0] += f;
	r[1] += f;
}


MINLINE void add_v3_fl(float r[3], float f)
{
	r[0] += f;
	r[1] += f;
	r[2] += f;
}

MINLINE void add_v4_fl(float r[4], float f)
{
	r[0] += f;
	r[1] += f;
	r[2] += f;
	r[3] += f;
}

MINLINE void add_v2_v2(float r[2], const float a[2])
{
	r[0] += a[0];
	r[1] += a[1];
}

MINLINE void add_v2_v2v2(float r[2], const float a[2], const float b[2])
{
	r[0] = a[0] + b[0];
	r[1] = a[1] + b[1];
}

MINLINE void add_v2_v2v2_int(int r[2], const int a[2], const int b[2])
{
	r[0] = a[0] + b[0];
	r[1] = a[1] + b[1];
}

MINLINE void add_v3_v3(float r[3], const float a[3])
{
	r[0] += a[0];
	r[1] += a[1];
	r[2] += a[2];
}

MINLINE void add_v3_v3v3(float r[3], const float a[3], const float b[3])
{
	r[0] = a[0] + b[0];
	r[1] = a[1] + b[1];
	r[2] = a[2] + b[2];
}

MINLINE void add_v4_v4(float r[4], const float a[4])
{
	r[0] += a[0];
	r[1] += a[1];
	r[2] += a[2];
	r[3] += a[3];
}

MINLINE void add_v4_v4v4(float r[4], const float a[4], const float b[4])
{
	r[0] = a[0] + b[0];
	r[1] = a[1] + b[1];
	r[2] = a[2] + b[2];
	r[3] = a[3] + b[3];
}

MINLINE void sub_v2_v2(float r[2], const float a[2])
{
	r[0] -= a[0];
	r[1] -= a[1];
}

MINLINE void sub_v2_v2v2(float r[2], const float a[2], const float b[2])
{
	r[0] = a[0] - b[0];
	r[1] = a[1] - b[1];
}

MINLINE void sub_v2_v2v2_int(int r[2], const int a[2], const int b[2])
{
	r[0] = a[0] - b[0];
	r[1] = a[1] - b[1];
}

MINLINE void sub_v3_v3(float r[3], const float a[3])
{
	r[0] -= a[0];
	r[1] -= a[1];
	r[2] -= a[2];
}

MINLINE void sub_v3_v3v3(float r[3], const float a[3], const float b[3])
{
	r[0] = a[0] - b[0];
	r[1] = a[1] - b[1];
	r[2] = a[2] - b[2];
}

MINLINE void sub_v4_v4(float r[4], const float a[4])
{
	r[0] -= a[0];
	r[1] -= a[1];
	r[2] -= a[2];
	r[3] -= a[3];
}

MINLINE void sub_v4_v4v4(float r[4], const float a[4], const float b[4])
{
	r[0] = a[0] - b[0];
	r[1] = a[1] - b[1];
	r[2] = a[2] - b[2];
	r[3] = a[3] - b[3];
}

MINLINE void mul_v2_fl(float r[2], float f)
{
	r[0] *= f;
	r[1] *= f;
}

MINLINE void mul_v2_v2fl(float r[2], const float a[2], float f)
{
	r[0] = a[0] * f;
	r[1] = a[1] * f;
}

MINLINE void mul_v3_fl(float r[3], float f)
{
	r[0] *= f;
	r[1] *= f;
	r[2] *= f;
}

MINLINE void mul_v3_v3fl(float r[3], const float a[3], float f)
{
	r[0] = a[0] * f;
	r[1] = a[1] * f;
	r[2] = a[2] * f;
}

MINLINE void mul_v2_v2(float r[2], const float a[2])
{
	r[0] *= a[0];
	r[1] *= a[1];
}

MINLINE void mul_v3_v3(float r[3], const float a[3])
{
	r[0] *= a[0];
	r[1] *= a[1];
	r[2] *= a[2];
}

MINLINE void mul_v4_fl(float r[4], float f)
{
	r[0] *= f;
	r[1] *= f;
	r[2] *= f;
	r[3] *= f;
}

MINLINE void mul_v4_v4fl(float r[4], const float a[4], float f)
{
	r[0] = a[0] * f;
	r[1] = a[1] * f;
	r[2] = a[2] * f;
	r[3] = a[3] * f;
}

/**
 * Avoid doing:
 *
 * angle = atan2f(dvec[0], dvec[1]);
 * angle_to_mat2(mat, angle);
 *
 * instead use a vector as a matrix.
 */

MINLINE void mul_v2_v2_cw(float r[2], const float mat[2], const float vec[2])
{
	BLI_assert(r != vec);

	r[0] = mat[0] * vec[0] + (+mat[1]) * vec[1];
	r[1] = mat[1] * vec[0] + (-mat[0]) * vec[1];
}

MINLINE void mul_v2_v2_ccw(float r[2], const float mat[2], const float vec[2])
{
	BLI_assert(r != vec);

	r[0] = mat[0] * vec[0] + (-mat[1]) * vec[1];
	r[1] = mat[1] * vec[0] + (+mat[0]) * vec[1];
}

/* note: could add a matrix inline */
MINLINE float mul_project_m4_v3_zfac(float mat[4][4], const float co[3])
{
	return (mat[0][3] * co[0]) +
	       (mat[1][3] * co[1]) +
	       (mat[2][3] * co[2]) + mat[3][3];
}

/**
 * Has the effect of mul_m3_v3(), on a single axis.
 */
MINLINE float dot_m3_v3_row_x(float M[3][3], const float a[3])
{
	return M[0][0] * a[0] + M[1][0] * a[1] + M[2][0] * a[2];
}
MINLINE float dot_m3_v3_row_y(float M[3][3], const float a[3])
{
	return M[0][1] * a[0] + M[1][1] * a[1] + M[2][1] * a[2];
}
MINLINE float dot_m3_v3_row_z(float M[3][3], const float a[3])
{
	return M[0][2] * a[0] + M[1][2] * a[1] + M[2][2] * a[2];
}

/**
 * Almost like mul_m4_v3(), misses adding translation.
 */
MINLINE float dot_m4_v3_row_x(float M[4][4], const float a[3])
{
	return M[0][0] * a[0] + M[1][0] * a[1] + M[2][0] * a[2];
}
MINLINE float dot_m4_v3_row_y(float M[4][4], const float a[3])
{
	return M[0][1] * a[0] + M[1][1] * a[1] + M[2][1] * a[2];
}
MINLINE float dot_m4_v3_row_z(float M[4][4], const float a[3])
{
	return M[0][2] * a[0] + M[1][2] * a[1] + M[2][2] * a[2];
}

MINLINE void madd_v2_v2fl(float r[2], const float a[2], float f)
{
	r[0] += a[0] * f;
	r[1] += a[1] * f;
}

MINLINE void madd_v3_v3fl(float r[3], const float a[3], float f)
{
	r[0] += a[0] * f;
	r[1] += a[1] * f;
	r[2] += a[2] * f;
}

MINLINE void madd_v3_v3v3(float r[3], const float a[3], const float b[3])
{
	r[0] += a[0] * b[0];
	r[1] += a[1] * b[1];
	r[2] += a[2] * b[2];
}

MINLINE void madd_v2_v2v2fl(float r[2], const float a[2], const float b[2], float f)
{
	r[0] = a[0] + b[0] * f;
	r[1] = a[1] + b[1] * f;
}

MINLINE void madd_v3_v3v3fl(float r[3], const float a[3], const float b[3], float f)
{
	r[0] = a[0] + b[0] * f;
	r[1] = a[1] + b[1] * f;
	r[2] = a[2] + b[2] * f;
}

MINLINE void madd_v3_v3v3v3(float r[3], const float a[3], const float b[3], const float c[3])
{
	r[0] = a[0] + b[0] * c[0];
	r[1] = a[1] + b[1] * c[1];
	r[2] = a[2] + b[2] * c[2];
}

MINLINE void madd_v4_v4fl(float r[4], const float a[4], float f)
{
	r[0] += a[0] * f;
	r[1] += a[1] * f;
	r[2] += a[2] * f;
	r[3] += a[3] * f;
}

MINLINE void madd_v4_v4v4(float r[4], const float a[4], const float b[4])
{
	r[0] += a[0] * b[0];
	r[1] += a[1] * b[1];
	r[2] += a[2] * b[2];
	r[3] += a[3] * b[3];
}

MINLINE void mul_v3_v3v3(float r[3], const float v1[3], const float v2[3])
{
	r[0] = v1[0] * v2[0];
	r[1] = v1[1] * v2[1];
	r[2] = v1[2] * v2[2];
}

MINLINE void negate_v2(float r[2])
{
	r[0] = -r[0];
	r[1] = -r[1];
}

MINLINE void negate_v2_v2(float r[2], const float a[2])
{
	r[0] = -a[0];
	r[1] = -a[1];
}

MINLINE void negate_v3(float r[3])
{
	r[0] = -r[0];
	r[1] = -r[1];
	r[2] = -r[2];
}

MINLINE void negate_v3_v3(float r[3], const float a[3])
{
	r[0] = -a[0];
	r[1] = -a[1];
	r[2] = -a[2];
}

MINLINE void negate_v4(float r[4])
{
	r[0] = -r[0];
	r[1] = -r[1];
	r[2] = -r[2];
	r[3] = -r[3];
}

MINLINE void negate_v4_v4(float r[4], const float a[4])
{
	r[0] = -a[0];
	r[1] = -a[1];
	r[2] = -a[2];
	r[3] = -a[3];
}

/* could add more... */
MINLINE void negate_v3_short(short r[3])
{
	r[0] = (short)-r[0];
	r[1] = (short)-r[1];
	r[2] = (short)-r[2];
}

MINLINE float dot_v2v2(const float a[2], const float b[2])
{
	return a[0] * b[0] + a[1] * b[1];
}

MINLINE float dot_v3v3(const float a[3], const float b[3])
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

MINLINE float dot_v4v4(const float a[4], const float b[4])
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
}

MINLINE float cross_v2v2(const float a[2], const float b[2])
{
	return a[0] * b[1] - a[1] * b[0];
}

MINLINE void cross_v3_v3v3(float r[3], const float a[3], const float b[3])
{
	BLI_assert(r != a && r != b);
	r[0] = a[1] * b[2] - a[2] * b[1];
	r[1] = a[2] * b[0] - a[0] * b[2];
	r[2] = a[0] * b[1] - a[1] * b[0];
}

/* Newell's Method */
/* excuse this fairly specific function,
 * its used for polygon normals all over the place
 * could use a better name */
MINLINE void add_newell_cross_v3_v3v3(float n[3], const float v_prev[3], const float v_curr[3])
{
	n[0] += (v_prev[1] - v_curr[1]) * (v_prev[2] + v_curr[2]);
	n[1] += (v_prev[2] - v_curr[2]) * (v_prev[0] + v_curr[0]);
	n[2] += (v_prev[0] - v_curr[0]) * (v_prev[1] + v_curr[1]);
}

MINLINE void star_m3_v3(float rmat[3][3], float a[3])
{
	rmat[0][0] = rmat[1][1] = rmat[2][2] = 0.0;
	rmat[0][1] = -a[2];
	rmat[0][2] = a[1];
	rmat[1][0] = a[2];
	rmat[1][2] = -a[0];
	rmat[2][0] = -a[1];
	rmat[2][1] = a[0];
}

/*********************************** Length **********************************/

MINLINE float len_squared_v2(const float v[2])
{
	return v[0] * v[0] + v[1] * v[1];
}

MINLINE float len_squared_v3(const float v[3])
{
	return v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
}

MINLINE float len_manhattan_v2(const float v[2])
{
	return fabsf(v[0]) + fabsf(v[1]);
}

MINLINE int len_manhattan_v2_int(const int v[2])
{
	return abs(v[0]) + abs(v[1]);
}

MINLINE float len_manhattan_v3(const float v[3])
{
	return fabsf(v[0]) + fabsf(v[1]) + fabsf(v[2]);
}

MINLINE float len_v2(const float v[2])
{
	return sqrtf(v[0] * v[0] + v[1] * v[1]);
}

MINLINE float len_v2v2(const float v1[2], const float v2[2])
{
	float x, y;

	x = v1[0] - v2[0];
	y = v1[1] - v2[1];
	return sqrtf(x * x + y * y);
}

MINLINE float len_v2v2_int(const int v1[2], const int v2[2])
{
	float x, y;

	x = (float)(v1[0] - v2[0]);
	y = (float)(v1[1] - v2[1]);
	return sqrtf(x * x + y * y);
}

MINLINE float len_v3(const float a[3])
{
	return sqrtf(dot_v3v3(a, a));
}

MINLINE float len_squared_v2v2(const float a[2], const float b[2])
{
	float d[2];

	sub_v2_v2v2(d, b, a);
	return dot_v2v2(d, d);
}

MINLINE float len_squared_v3v3(const float a[3], const float b[3])
{
	float d[3];

	sub_v3_v3v3(d, b, a);
	return dot_v3v3(d, d);
}

MINLINE float len_manhattan_v2v2(const float a[2], const float b[2])
{
	float d[2];

	sub_v2_v2v2(d, b, a);
	return len_manhattan_v2(d);
}

MINLINE int len_manhattan_v2v2_int(const int a[2], const int b[2])
{
	int d[2];

	sub_v2_v2v2_int(d, b, a);
	return len_manhattan_v2_int(d);
}

MINLINE float len_manhattan_v3v3(const float a[3], const float b[3])
{
	float d[3];

	sub_v3_v3v3(d, b, a);
	return len_manhattan_v3(d);
}

MINLINE float len_v3v3(const float a[3], const float b[3])
{
	float d[3];

	sub_v3_v3v3(d, b, a);
	return len_v3(d);
}

MINLINE float normalize_v2_v2(float r[2], const float a[2])
{
	float d = dot_v2v2(a, a);

	if (d > 1.0e-35f) {
		d = sqrtf(d);
		mul_v2_v2fl(r, a, 1.0f / d);
	}
	else {
		zero_v2(r);
		d = 0.0f;
	}

	return d;
}

MINLINE float normalize_v2(float n[2])
{
	return normalize_v2_v2(n, n);
}

MINLINE float normalize_v3_v3(float r[3], const float a[3])
{
	float d = dot_v3v3(a, a);

	/* a larger value causes normalize errors in a
	 * scaled down models with camera extreme close */
	if (d > 1.0e-35f) {
		d = sqrtf(d);
		mul_v3_v3fl(r, a, 1.0f / d);
	}
	else {
		zero_v3(r);
		d = 0.0f;
	}

	return d;
}

MINLINE double normalize_v3_d(double n[3])
{
	double d = n[0] * n[0] + n[1] * n[1] + n[2] * n[2];

	/* a larger value causes normalize errors in a
	 * scaled down models with camera extreme close */
	if (d > 1.0e-35) {
		double mul;

		d = sqrt(d);
		mul = 1.0 / d;

		n[0] *= mul;
		n[1] *= mul;
		n[2] *= mul;
	}
	else {
		n[0] = n[1] = n[2] = 0;
		d = 0.0;
	}

	return d;
}

MINLINE float normalize_v3(float n[3])
{
	return normalize_v3_v3(n, n);
}

MINLINE void normal_short_to_float_v3(float out[3], const short in[3])
{
	out[0] = in[0] * (1.0f / 32767.0f);
	out[1] = in[1] * (1.0f / 32767.0f);
	out[2] = in[2] * (1.0f / 32767.0f);
}

MINLINE void normal_float_to_short_v3(short out[3], const float in[3])
{
	out[0] = (short) (in[0] * 32767.0f);
	out[1] = (short) (in[1] * 32767.0f);
	out[2] = (short) (in[2] * 32767.0f);
}

/********************************* Comparison ********************************/


MINLINE bool is_zero_v2(const float v[2])
{
	return (v[0] == 0 && v[1] == 0);
}

MINLINE bool is_zero_v3(const float v[3])
{
	return (v[0] == 0 && v[1] == 0 && v[2] == 0);
}

MINLINE bool is_zero_v4(const float v[4])
{
	return (v[0] == 0 && v[1] == 0 && v[2] == 0 && v[3] == 0);
}

MINLINE bool is_finite_v2(const float v[2])
{
	return (finite(v[0]) && finite(v[1]));
}

MINLINE bool is_finite_v3(const float v[3])
{
	return (finite(v[0]) && finite(v[1]) && finite(v[2]));
}

MINLINE bool is_finite_v4(const float v[4])
{
	return (finite(v[0]) && finite(v[1]) && finite(v[2]) && finite(v[3]));
}

MINLINE bool is_one_v3(const float v[3])
{
	return (v[0] == 1 && v[1] == 1 && v[2] == 1);
}

MINLINE bool equals_v2v2(const float v1[2], const float v2[2])
{
	return ((v1[0] == v2[0]) && (v1[1] == v2[1]));
}

MINLINE bool equals_v3v3(const float v1[3], const float v2[3])
{
	return ((v1[0] == v2[0]) && (v1[1] == v2[1]) && (v1[2] == v2[2]));
}

MINLINE bool equals_v4v4(const float v1[4], const float v2[4])
{
	return ((v1[0] == v2[0]) && (v1[1] == v2[1]) && (v1[2] == v2[2]) && (v1[3] == v2[3]));
}

MINLINE bool compare_v2v2(const float v1[2], const float v2[2], const float limit)
{
	if (fabsf(v1[0] - v2[0]) < limit)
		if (fabsf(v1[1] - v2[1]) < limit)
			return true;

	return false;
}

MINLINE bool compare_v3v3(const float v1[3], const float v2[3], const float limit)
{
	if (fabsf(v1[0] - v2[0]) < limit)
		if (fabsf(v1[1] - v2[1]) < limit)
			if (fabsf(v1[2] - v2[2]) < limit)
				return true;

	return false;
}

MINLINE bool compare_len_v3v3(const float v1[3], const float v2[3], const float limit)
{
	float x, y, z;

	x = v1[0] - v2[0];
	y = v1[1] - v2[1];
	z = v1[2] - v2[2];

	return ((x * x + y * y + z * z) < (limit * limit));
}

MINLINE bool compare_v4v4(const float v1[4], const float v2[4], const float limit)
{
	if (fabsf(v1[0] - v2[0]) < limit)
		if (fabsf(v1[1] - v2[1]) < limit)
			if (fabsf(v1[2] - v2[2]) < limit)
				if (fabsf(v1[3] - v2[3]) < limit)
					return true;

	return false;
}

MINLINE float line_point_side_v2(const float l1[2], const float l2[2], const float pt[2])
{
	return (((l1[0] - pt[0]) * (l2[1] - pt[1])) -
	        ((l2[0] - pt[0]) * (l1[1] - pt[1])));
}

#endif /* __MATH_VECTOR_INLINE_C__ */
