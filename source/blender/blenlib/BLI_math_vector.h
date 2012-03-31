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

#ifndef __BLI_MATH_VECTOR_H__
#define __BLI_MATH_VECTOR_H__

/** \file BLI_math_vector.h
 *  \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_math_inline.h"

#ifdef __BLI_MATH_INLINE_H__
#include "intern/math_vector_inline.c"
#endif

/************************************* Init ***********************************/

MINLINE void zero_v2(float r[2]);
MINLINE void zero_v3(float r[3]);
MINLINE void zero_v4(float r[4]);

MINLINE void copy_v2_v2(float r[2], const float a[2]);
MINLINE void copy_v3_v3(float r[3], const float a[3]);
MINLINE void copy_v4_v4(float r[4], const float a[4]);

MINLINE void copy_v2_fl(float r[2], float f);
MINLINE void copy_v3_fl(float r[3], float f);
MINLINE void copy_v4_fl(float r[4], float f);

MINLINE void swap_v2_v2(float a[2], float b[2]);
MINLINE void swap_v3_v3(float a[3], float b[3]);
MINLINE void swap_v4_v4(float a[4], float b[4]);

/* char */
MINLINE void copy_v2_v2_char(char r[2], const char a[2]);
MINLINE void copy_v3_v3_char(char r[3], const char a[3]);
MINLINE void copy_v4_v4_char(char r[4], const char a[4]);
/* short */
MINLINE void copy_v2_v2_short(short r[2], const short a[2]);
MINLINE void copy_v3_v3_short(short r[3], const short a[3]);
MINLINE void copy_v4_v4_short(short r[4], const short a[4]);
/* int */
MINLINE void copy_v2_v2_int(int r[2], const int a[2]);
MINLINE void copy_v3_v3_int(int r[3], const int a[3]);
MINLINE void copy_v4_v4_int(int r[4], const int a[4]);
/* double -> float */
MINLINE void copy_v2fl_v2db(float r[2], const double a[2]);
MINLINE void copy_v3fl_v3db(float r[3], const double a[3]);
MINLINE void copy_v4fl_v4db(float r[4], const double a[4]);
/* float -> double */
MINLINE void copy_v2db_v2fl(double r[2], const float a[2]);
MINLINE void copy_v3db_v3fl(double r[3], const float a[3]);
MINLINE void copy_v4db_v4fl(double r[4], const float a[4]);

/********************************* Arithmetic ********************************/

MINLINE void add_v3_fl(float r[3], float f);
MINLINE void add_v4_fl(float r[4], float f);
MINLINE void add_v2_v2(float r[2], const float a[2]);
MINLINE void add_v2_v2v2(float r[2], const float a[2], const float b[2]);
MINLINE void add_v3_v3(float r[3], const float a[3]);
MINLINE void add_v3_v3v3(float r[3], const float a[3], const float b[3]);

MINLINE void sub_v2_v2(float r[2], const float a[2]);
MINLINE void sub_v2_v2v2(float r[2], const float a[2], const float b[2]);
MINLINE void sub_v3_v3(float r[3], const float a[3]);
MINLINE void sub_v3_v3v3(float r[3], const float a[3], const float b[3]);
MINLINE void sub_v4_v4(float r[4], const float a[4]);
MINLINE void sub_v4_v4v4(float r[4], const float a[4], const float b[4]);

MINLINE void mul_v2_fl(float r[2], float f);
MINLINE void mul_v2_v2fl(float r[2], const float a[2], float f);
MINLINE void mul_v3_fl(float r[3], float f);
MINLINE void mul_v3_v3fl(float r[3], const float a[3], float f);
MINLINE void mul_v2_v2(float r[2], const float a[2]);
MINLINE void mul_v3_v3(float r[3], const float a[3]);
MINLINE void mul_v3_v3v3(float r[3], const float a[3], const float b[3]);
MINLINE void mul_v4_fl(float r[4], float f);

MINLINE void madd_v3_v3fl(float r[3], const float a[3], float f);
MINLINE void madd_v3_v3v3(float r[3], const float a[3], const float b[3]);
MINLINE void madd_v2_v2v2fl(float r[2], const float a[2], const float b[2], float f);
MINLINE void madd_v3_v3v3fl(float r[3], const float a[3], const float b[3], float f);
MINLINE void madd_v3_v3v3v3(float r[3], const float a[3], const float b[3], const float c[3]);
MINLINE void madd_v4_v4fl(float r[4], const float a[4], float f);

MINLINE void negate_v2(float r[2]);
MINLINE void negate_v2_v2(float r[2], const float a[2]);
MINLINE void negate_v3(float r[3]);
MINLINE void negate_v3_v3(float r[3], const float a[3]);
MINLINE void negate_v4(float r[4]);
MINLINE void negate_v4_v4(float r[4], const float a[3]);

MINLINE float dot_v2v2(const float a[2], const float b[2]);
MINLINE float dot_v3v3(const float a[3], const float b[3]);

MINLINE float cross_v2v2(const float a[2], const float b[2]);
MINLINE void cross_v3_v3v3(float r[3], const float a[3], const float b[3]);

MINLINE void star_m3_v3(float rmat[3][3],float a[3]);

/*********************************** Length **********************************/

MINLINE float len_squared_v2(const float v[2]);
MINLINE float len_squared_v3(const float v[3]);
MINLINE float len_v2(const float a[2]);
MINLINE float len_v2v2(const float a[2], const float b[2]);
MINLINE float len_squared_v2v2(const float a[2], const float b[2]);
MINLINE float len_v3(const float a[3]);
MINLINE float len_v3v3(const float a[3], const float b[3]);
MINLINE float len_squared_v3v3(const float a[3], const float b[3]);

MINLINE float normalize_v2(float r[2]);
MINLINE float normalize_v2_v2(float r[2], const float a[2]);
MINLINE float normalize_v3(float r[3]);
MINLINE float normalize_v3_v3(float r[3], const float a[3]);

/******************************* Interpolation *******************************/

void interp_v2_v2v2(float r[2], const float a[2], const float b[2], const float t);
void interp_v2_v2v2v2(float r[2], const float a[2], const float b[2], const float c[3], const float t[3]);
void interp_v3_v3v3(float r[3], const float a[3], const float b[3], const float t);
void interp_v3_v3v3v3(float p[3], const float v1[3], const float v2[3], const float v3[3], const float w[3]);
void interp_v3_v3v3v3v3(float p[3], const float v1[3], const float v2[3], const float v3[3], const float v4[3], const float w[4]);
void interp_v4_v4v4(float r[4], const float a[4], const float b[4], const float t);
void interp_v4_v4v4v4(float p[4], const float v1[4], const float v2[4], const float v3[4], const float w[3]);
void interp_v4_v4v4v4v4(float p[4], const float v1[4], const float v2[4], const float v3[4], const float v4[4], const float w[4]);

void mid_v3_v3v3(float r[3], const float a[3], const float b[3]);

/********************************* Comparison ********************************/

MINLINE int is_zero_v3(const float a[3]);
MINLINE int is_zero_v4(const float a[4]);
MINLINE int is_one_v3(const float a[3]);

MINLINE int equals_v2v2(const float v1[2], const float v2[2]);
MINLINE int equals_v3v3(const float a[3], const float b[3]);
MINLINE int compare_v3v3(const float a[3], const float b[3], const float limit);
MINLINE int compare_len_v3v3(const float a[3], const float b[3], const float limit);

MINLINE int compare_v4v4(const float a[4], const float b[4], const float limit);
MINLINE int equals_v4v4(const float a[4], const float b[4]);

MINLINE float line_point_side_v2(const float l1[2], const float l2[2], const float pt[2]);

/********************************** Angles ***********************************/
/* - angle with 2 arguments is angle between vector                          */
/* - angle with 3 arguments is angle between 3 points at the middle point    */
/* - angle_normalized_* is faster equivalent if vectors are normalized       */

float angle_v2v2(const float a[2], const float b[2]);
float angle_signed_v2v2(const float v1[2], const float v2[2]);
float angle_v2v2v2(const float a[2], const float b[2], const float c[2]);
float angle_normalized_v2v2(const float a[2], const float b[2]);
float angle_v3v3(const float a[3], const float b[3]);
float angle_v3v3v3(const float a[3], const float b[3], const float c[3]);
float angle_normalized_v3v3(const float v1[3], const float v2[3]);
void angle_tri_v3(float angles[3], const float v1[3], const float v2[3], const float v3[3]);
void angle_quad_v3(float angles[4], const float v1[3], const float v2[3], const float v3[3], const float v4[3]);
void angle_poly_v3(float* angles, const float* verts[3], int len);

/********************************* Geometry **********************************/

void project_v2_v2v2(float c[2], const float v1[2], const float v2[2]);
void project_v3_v3v3(float r[3], const float p[3], const float n[3]);
void project_v3_plane(float v[3], const float n[3], const float p[3]);
void reflect_v3_v3v3(float r[3], const float v[3], const float n[3]);
void ortho_basis_v3v3_v3(float r1[3], float r2[3], const float a[3]);
void bisect_v3_v3v3v3(float r[3], const float a[3], const float b[3], const float c[3]);
void rotate_v3_v3v3fl(float v[3], const float p[3], const float axis[3], const float angle);
void rotate_normalized_v3_v3v3fl(float v[3], const float p[3], const float axis[3], const float angle);

/*********************************** Other ***********************************/

void print_v2(const char *str, const float a[2]);
void print_v3(const char *str, const float a[3]);
void print_v4(const char *str, const float a[4]);

MINLINE void normal_short_to_float_v3(float r[3], const short n[3]);
MINLINE void normal_float_to_short_v3(short r[3], const float n[3]);

void minmax_v3v3_v3(float min[3], float max[3], const float vec[3]);

/***************************** Array Functions *******************************/
/* attempted to follow fixed length vertex functions. names could be improved*/
double dot_vn_vn(const float *array_src_a, const float *array_src_b, const int size);
float normalize_vn_vn(float *array_tar, const float *array_src, const int size);
float normalize_vn(float *array_tar, const int size);
void range_vn_i(int *array_tar, const int size, const int start);
void range_vn_fl(float *array_tar, const int size, const float start, const float step);
void negate_vn(float *array_tar, const int size);
void negate_vn_vn(float *array_tar, const float *array_src, const int size);
void mul_vn_fl(float *array_tar, const int size, const float f);
void mul_vn_vn_fl(float *array_tar, const float *array_src, const int size, const float f);
void add_vn_vn(float *array_tar, const float *array_src, const int size);
void add_vn_vnvn(float *array_tar, const float *array_src_a, const float *array_src_b, const int size);
void sub_vn_vn(float *array_tar, const float *array_src, const int size);
void sub_vn_vnvn(float *array_tar, const float *array_src_a, const float *array_src_b, const int size);
void fill_vn_i(int *array_tar, const int size, const int val);
void fill_vn_fl(float *array_tar, const int size, const float val);

#ifdef __cplusplus
}
#endif

#endif /* __BLI_MATH_VECTOR_H__ */

