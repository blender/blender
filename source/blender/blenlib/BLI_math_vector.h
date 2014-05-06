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

#include "BLI_compiler_attrs.h"
#include "BLI_math_inline.h"

/************************************* Init ***********************************/

#ifdef BLI_MATH_GCC_WARN_PRAGMA
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wredundant-decls"
#endif


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
MINLINE void zero_v3_int(int r[3]);
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
/* float args -> vec */
MINLINE void copy_v2_fl2(float v[2], float x, float y);
MINLINE void copy_v3_fl3(float v[3], float x, float y, float z);
MINLINE void copy_v4_fl4(float v[4], float x, float y, float z, float w);

/********************************* Arithmetic ********************************/

MINLINE void add_v2_fl(float r[2], float f);
MINLINE void add_v3_fl(float r[3], float f);
MINLINE void add_v4_fl(float r[4], float f);
MINLINE void add_v2_v2(float r[2], const float a[2]);
MINLINE void add_v2_v2v2(float r[2], const float a[2], const float b[2]);
MINLINE void add_v2_v2v2_int(int r[2], const int a[2], const int b[2]);
MINLINE void add_v3_v3(float r[3], const float a[3]);
MINLINE void add_v3_v3v3(float r[3], const float a[3], const float b[3]);
MINLINE void add_v4_v4(float r[4], const float a[4]);
MINLINE void add_v4_v4v4(float r[4], const float a[4], const float b[4]);

MINLINE void sub_v2_v2(float r[2], const float a[2]);
MINLINE void sub_v2_v2v2(float r[2], const float a[2], const float b[2]);
MINLINE void sub_v2_v2v2_int(int r[2], const int a[2], const int b[2]);
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
MINLINE void mul_v4_v4fl(float r[3], const float a[3], float f);
MINLINE void mul_v2_v2_cw(float r[2], const float mat[2], const float vec[2]);
MINLINE void mul_v2_v2_ccw(float r[2], const float mat[2], const float vec[2]);
MINLINE float mul_project_m4_v3_zfac(float mat[4][4], const float co[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float dot_m3_v3_row_x(float M[3][3], const float a[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float dot_m3_v3_row_y(float M[3][3], const float a[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float dot_m3_v3_row_z(float M[3][3], const float a[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float dot_m4_v3_row_x(float M[4][4], const float a[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float dot_m4_v3_row_y(float M[4][4], const float a[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float dot_m4_v3_row_z(float M[4][4], const float a[3]) ATTR_WARN_UNUSED_RESULT;

MINLINE void madd_v2_v2fl(float r[2], const float a[2], float f);
MINLINE void madd_v3_v3fl(float r[3], const float a[3], float f);
MINLINE void madd_v3_v3v3(float r[3], const float a[3], const float b[3]);
MINLINE void madd_v2_v2v2fl(float r[2], const float a[2], const float b[2], float f);
MINLINE void madd_v3_v3v3fl(float r[3], const float a[3], const float b[3], float f);
MINLINE void madd_v3_v3v3v3(float r[3], const float a[3], const float b[3], const float c[3]);
MINLINE void madd_v4_v4fl(float r[4], const float a[4], float f);
MINLINE void madd_v4_v4v4(float r[4], const float a[4], const float b[4]);

MINLINE void negate_v2(float r[2]);
MINLINE void negate_v2_v2(float r[2], const float a[2]);
MINLINE void negate_v3(float r[3]);
MINLINE void negate_v3_v3(float r[3], const float a[3]);
MINLINE void negate_v4(float r[4]);
MINLINE void negate_v4_v4(float r[4], const float a[3]);

MINLINE void negate_v3_short(short r[3]);

MINLINE float dot_v2v2(const float a[2], const float b[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE float dot_v3v3(const float a[3], const float b[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float dot_v4v4(const float a[4], const float b[4]) ATTR_WARN_UNUSED_RESULT;

MINLINE float cross_v2v2(const float a[2], const float b[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE void cross_v3_v3v3(float r[3], const float a[3], const float b[3]);

MINLINE void add_newell_cross_v3_v3v3(float n[3], const float v_prev[3], const float v_curr[3]);

MINLINE void star_m3_v3(float rmat[3][3], float a[3]);

/*********************************** Length **********************************/

MINLINE float len_squared_v2(const float v[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE float len_squared_v3(const float v[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float len_manhattan_v2(const float v[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE int   len_manhattan_v2_int(const int v[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE float len_manhattan_v3(const float v[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float len_v2(const float a[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE float len_v2v2(const float a[2], const float b[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE float len_v2v2_int(const int v1[2], const int v2[2]);
MINLINE float len_squared_v2v2(const float a[2], const float b[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE float len_squared_v3v3(const float a[3], const float b[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float len_manhattan_v2v2(const float a[2], const float b[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE int   len_manhattan_v2v2_int(const int a[2], const int b[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE float len_manhattan_v3v3(const float a[3], const float b[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float len_v3(const float a[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float len_v3v3(const float a[3], const float b[3]) ATTR_WARN_UNUSED_RESULT;

MINLINE float normalize_v2(float r[2]);
MINLINE float normalize_v2_v2(float r[2], const float a[2]);
MINLINE float normalize_v3(float r[3]);
MINLINE float normalize_v3_v3(float r[3], const float a[3]);
MINLINE double normalize_v3_d(double n[3]);

/******************************* Interpolation *******************************/

void interp_v2_v2v2(float r[2], const float a[2], const float b[2], const float t);
void interp_v2_v2v2v2(float r[2], const float a[2], const float b[2], const float c[2], const float t[3]);
void interp_v3_v3v3(float r[3], const float a[3], const float b[3], const float t);
void interp_v3_v3v3v3(float p[3], const float v1[3], const float v2[3], const float v3[3], const float w[3]);
void interp_v3_v3v3v3v3(float p[3], const float v1[3], const float v2[3], const float v3[3], const float v4[3], const float w[4]);
void interp_v4_v4v4(float r[4], const float a[4], const float b[4], const float t);
void interp_v4_v4v4v4(float p[4], const float v1[4], const float v2[4], const float v3[4], const float w[3]);
void interp_v4_v4v4v4v4(float p[4], const float v1[4], const float v2[4], const float v3[4], const float v4[4], const float w[4]);
void interp_v3_v3v3v3_uv(float p[3], const float v1[3], const float v2[3], const float v3[3], const float uv[2]);

bool interp_v3_v3v3_slerp(float target[3], const float a[3], const float b[3], const float t)  ATTR_WARN_UNUSED_RESULT;
bool interp_v2_v2v2_slerp(float target[2], const float a[2], const float b[2], const float t)  ATTR_WARN_UNUSED_RESULT;

void interp_v3_v3v3_slerp_safe(float target[3], const float a[3], const float b[3], const float t);
void interp_v2_v2v2_slerp_safe(float target[2], const float a[2], const float b[2], const float t);

void interp_v3_v3v3_char(char target[3], const char a[3], const char b[3], const float t);
void interp_v3_v3v3_uchar(unsigned char target[3], const unsigned char a[3], const unsigned char b[3], const float t);
void interp_v4_v4v4_char(char target[4], const char a[4], const char b[4], const float t);
void interp_v4_v4v4_uchar(unsigned char target[4], const unsigned char a[4], const unsigned char b[4], const float t);

void mid_v3_v3v3(float r[3], const float a[3], const float b[3]);
void mid_v2_v2v2(float r[2], const float a[2], const float b[2]);
void mid_v3_v3v3v3(float v[3], const float v1[3], const float v2[3], const float v3[3]);

void mid_v3_v3v3_angle_weighted(float r[3], const float a[3], const float b[3]);
void mid_v3_angle_weighted(float r[3]);

void flip_v4_v4v4(float v[4], const float v1[4], const float v2[4]);
void flip_v3_v3v3(float v[3], const float v1[3], const float v2[3]);
void flip_v2_v2v2(float v[2], const float v1[2], const float v2[2]);

/********************************* Comparison ********************************/

MINLINE bool is_zero_v2(const float a[3])  ATTR_WARN_UNUSED_RESULT;
MINLINE bool is_zero_v3(const float a[3])  ATTR_WARN_UNUSED_RESULT;
MINLINE bool is_zero_v4(const float a[4])  ATTR_WARN_UNUSED_RESULT;

MINLINE bool is_finite_v2(const float a[3])  ATTR_WARN_UNUSED_RESULT;
MINLINE bool is_finite_v3(const float a[3])  ATTR_WARN_UNUSED_RESULT;
MINLINE bool is_finite_v4(const float a[4])  ATTR_WARN_UNUSED_RESULT;

MINLINE bool is_one_v3(const float a[3])  ATTR_WARN_UNUSED_RESULT;

MINLINE bool equals_v2v2(const float v1[2], const float v2[2])  ATTR_WARN_UNUSED_RESULT;
MINLINE bool equals_v3v3(const float a[3], const float b[3])  ATTR_WARN_UNUSED_RESULT;
MINLINE bool compare_v2v2(const float a[2], const float b[2], const float limit)  ATTR_WARN_UNUSED_RESULT;
MINLINE bool compare_v3v3(const float a[3], const float b[3], const float limit)  ATTR_WARN_UNUSED_RESULT;
MINLINE bool compare_len_v3v3(const float a[3], const float b[3], const float limit)  ATTR_WARN_UNUSED_RESULT;

MINLINE bool compare_v4v4(const float a[4], const float b[4], const float limit)  ATTR_WARN_UNUSED_RESULT;
MINLINE bool equals_v4v4(const float a[4], const float b[4])  ATTR_WARN_UNUSED_RESULT;

MINLINE float line_point_side_v2(const float l1[2], const float l2[2], const float pt[2]) ATTR_WARN_UNUSED_RESULT;

/********************************** Angles ***********************************/
/* - angle with 2 arguments is angle between vector                          */
/* - angle with 3 arguments is angle between 3 points at the middle point    */
/* - angle_normalized_* is faster equivalent if vectors are normalized       */

float angle_v2v2(const float a[2], const float b[2]) ATTR_WARN_UNUSED_RESULT;
float angle_signed_v2v2(const float v1[2], const float v2[2]) ATTR_WARN_UNUSED_RESULT;
float angle_v2v2v2(const float a[2], const float b[2], const float c[2]) ATTR_WARN_UNUSED_RESULT;
float angle_normalized_v2v2(const float a[2], const float b[2]) ATTR_WARN_UNUSED_RESULT;
float angle_v3v3(const float a[3], const float b[3]) ATTR_WARN_UNUSED_RESULT;
float angle_v3v3v3(const float a[3], const float b[3], const float c[3]) ATTR_WARN_UNUSED_RESULT;
float cos_v3v3v3(const float p1[3], const float p2[3], const float p3[3]) ATTR_WARN_UNUSED_RESULT;
float angle_normalized_v3v3(const float v1[3], const float v2[3]) ATTR_WARN_UNUSED_RESULT;
float angle_on_axis_v3v3v3_v3(const float v1[3], const float v2[3], const float v3[3], const float axis[3]) ATTR_WARN_UNUSED_RESULT;
void angle_tri_v3(float angles[3], const float v1[3], const float v2[3], const float v3[3]);
void angle_quad_v3(float angles[4], const float v1[3], const float v2[3], const float v3[3], const float v4[3]);
void angle_poly_v3(float *angles, const float *verts[3], int len);

/********************************* Geometry **********************************/

void project_v2_v2v2(float c[2], const float v1[2], const float v2[2]);
void project_v3_v3v3(float r[3], const float p[3], const float n[3]);
void project_v3_plane(float v[3], const float n[3], const float p[3]);
void reflect_v3_v3v3(float r[3], const float v[3], const float n[3]);
void ortho_basis_v3v3_v3(float r_n1[3], float r_n2[3], const float n[3]);
void ortho_v3_v3(float p[3], const float v[3]);
void ortho_v2_v2(float p[3], const float v[3]);
void bisect_v3_v3v3v3(float r[3], const float a[3], const float b[3], const float c[3]);
void rotate_v3_v3v3fl(float v[3], const float p[3], const float axis[3], const float angle);
void rotate_normalized_v3_v3v3fl(float v[3], const float p[3], const float axis[3], const float angle);

/*********************************** Other ***********************************/

void print_v2(const char *str, const float a[2]);
void print_v3(const char *str, const float a[3]);
void print_v4(const char *str, const float a[4]);
void print_vn(const char *str, const float v[], const int n);

#define print_v2_id(v) print_v2(STRINGIFY(v), v)
#define print_v3_id(v) print_v3(STRINGIFY(v), v)
#define print_v4_id(v) print_v4(STRINGIFY(v), v)
#define print_vn_id(v, n) print_vn(STRINGIFY(v), v, n)

MINLINE void normal_short_to_float_v3(float r[3], const short n[3]);
MINLINE void normal_float_to_short_v3(short r[3], const float n[3]);

void minmax_v3v3_v3(float min[3], float max[3], const float vec[3]);
void minmax_v2v2_v2(float min[2], float max[2], const float vec[2]);

void dist_ensure_v3_v3fl(float v1[3], const float v2[3], const float dist);
void dist_ensure_v2_v2fl(float v1[2], const float v2[2], const float dist);

void axis_sort_v3(const float axis_values[3], int r_axis_order[3]);

/***************************** Array Functions *******************************/
/* attempted to follow fixed length vertex functions. names could be improved*/
double dot_vn_vn(const float *array_src_a, const float *array_src_b, const int size) ATTR_WARN_UNUSED_RESULT;
double len_squared_vn(const float *array, const int size) ATTR_WARN_UNUSED_RESULT;
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
void madd_vn_vn(float *array_tar, const float *array_src, const float f, const int size);
void madd_vn_vnvn(float *array_tar, const float *array_src_a, const float *array_src_b, const float f, const int size);
void sub_vn_vn(float *array_tar, const float *array_src, const int size);
void sub_vn_vnvn(float *array_tar, const float *array_src_a, const float *array_src_b, const int size);
void msub_vn_vn(float *array_tar, const float *array_src, const float f, const int size);
void msub_vn_vnvn(float *array_tar, const float *array_src_a, const float *array_src_b, const float f, const int size);
void interp_vn_vn(float *array_tar, const float *array_src, const float t, const int size);
void fill_vn_i(int *array_tar, const int size, const int val);
void fill_vn_short(short *array_tar, const int size, const short val);
void fill_vn_ushort(unsigned short *array_tar, const int size, const unsigned short val);
void fill_vn_fl(float *array_tar, const int size, const float val);

/**************************** Inline Definitions ******************************/

#if BLI_MATH_DO_INLINE
#include "intern/math_vector_inline.c"
#endif

#ifdef BLI_MATH_GCC_WARN_PRAGMA
#  pragma GCC diagnostic pop
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BLI_MATH_VECTOR_H__ */
