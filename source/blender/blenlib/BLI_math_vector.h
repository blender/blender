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

#ifndef BLI_MATH_VECTOR
#define BLI_MATH_VECTOR

#ifdef __cplusplus
extern "C" {
#endif

/* add platform/compiler checks here if it is not supported */
#define BLI_MATH_INLINE

#ifdef BLI_MATH_INLINE
#ifdef _MSC_VER
#define MINLINE static __inline
#else
#define MINLINE static inline
#endif
#include "intern/math_vector_inline.c"
#else
#define MINLINE
#endif

/************************************* Init ***********************************/

MINLINE void zero_v2(float r[2]);
MINLINE void zero_v3(float r[3]);

MINLINE void copy_v2_v2(float r[2], const float a[2]);
MINLINE void copy_v3_v3(float r[3], const float a[3]);

MINLINE void swap_v2_v2(float a[2], float b[2]);
MINLINE void swap_v3_v3(float a[3], float b[3]);

/********************************* Arithmetic ********************************/

MINLINE void add_v2_v2(float r[2], float a[2]);
MINLINE void add_v2_v2v2(float r[2], float a[2], float b[2]);
MINLINE void add_v3_v3(float r[3], float a[3]);
MINLINE void add_v3_v3v3(float r[3], float a[3], float b[3]);

MINLINE void sub_v2_v2(float r[2], float a[2]);
MINLINE void sub_v2_v2v2(float r[2], float a[2], float b[2]);
MINLINE void sub_v3_v3(float r[3], float a[3]);
MINLINE void sub_v3_v3v3(float r[3], const float a[3], const float b[3]);

MINLINE void mul_v2_fl(float r[2], float f);
MINLINE void mul_v3_fl(float r[3], float f);
MINLINE void mul_v3_v3fl(float r[3], float a[3], float f);
MINLINE void mul_v2_v2(float r[2], const float a[2]);
MINLINE void mul_v3_v3(float r[3], float a[3]);
MINLINE void mul_v3_v3v3(float r[3], float a[3], float b[3]);

MINLINE void madd_v3_v3fl(float r[3], float a[3], float f);
MINLINE void madd_v3_v3v3(float r[3], float a[3], float b[3]);
MINLINE void madd_v2_v2v2fl(float r[2], const float a[2], const float b[2], const float f);
MINLINE void madd_v3_v3v3fl(float r[3], float a[3], float b[3], float f);
MINLINE void madd_v3_v3v3v3(float r[3], float a[3], float b[3], float c[3]);

MINLINE void negate_v3(float r[3]);
MINLINE void negate_v3_v3(float r[3], const float a[3]);

MINLINE float dot_v2v2(const float a[2], const float b[2]);
MINLINE float dot_v3v3(const float a[3], const float b[3]);

MINLINE float cross_v2v2(const float a[2], const float b[2]);
MINLINE void cross_v3_v3v3(float r[3], const float a[3], const float b[3]);

MINLINE void star_m3_v3(float R[3][3],float a[3]);

/*********************************** Length **********************************/

MINLINE float len_v2(const float a[2]);
MINLINE float len_v2v2(const float a[2], const float b[2]);
MINLINE float len_v3(const float a[3]);
MINLINE float len_v3v3(const float a[3], const float b[3]);

MINLINE float normalize_v2(float r[2]);
MINLINE float normalize_v3(float r[3]);

/******************************* Interpolation *******************************/

void interp_v2_v2v2(float r[2], const float a[2], const float b[2], const float t);
void interp_v2_v2v2v2(float r[2], const float a[2], const float b[2], const float c[3], const float t[3]);
void interp_v3_v3v3(float r[3], const float a[3], const float b[3], const float t);
void interp_v3_v3v3v3(float p[3], const float v1[3], const float v2[3], const float v3[3], const float w[3]);
void interp_v3_v3v3v3v3(float p[3], const float v1[3], const float v2[3], const float v3[3], const float v4[3], const float w[4]);

void mid_v3_v3v3(float r[3], float a[3], float b[3]);

/********************************* Comparison ********************************/

int is_zero_v3(float a[3]);
int equals_v3v3(float a[3], float b[3]);
int compare_v3v3(float a[3], float b[3], float limit);
int compare_len_v3v3(float a[3], float b[3], float limit);

int compare_v4v4(float a[4], float b[4], float limit);

/********************************** Angles ***********************************/
/* - angle with 2 arguments is angle between vector                          */
/* - angle with 3 arguments is angle between 3 points at the middle point    */
/* - angle_normalized_* is faster equivalent if vectors are normalized       */

float angle_v2v2(float a[2], float b[2]);
float angle_v2v2v2(float a[2], float b[2], float c[2]);
float angle_normalized_v2v2(float a[2], float b[2]);
float angle_v3v3(float a[2], float b[2]);
float angle_v3v3v3(float a[2], float b[2], float c[2]);
float angle_normalized_v3v3(const float v1[3], const float v2[3]);
void angle_tri_v3(float angles[3], const float v1[3], const float v2[3], const float v3[3]);
void angle_quad_v3(float angles[4], const float v1[3], const float v2[3], const float v3[3], const float v4[3]);

/********************************* Geometry **********************************/

void project_v3_v3v3(float r[3], float p[3], float n[3]);
void reflect_v3_v3v3(float r[3], float v[3], float n[3]);
void ortho_basis_v3v3_v3(float r1[3], float r2[3], float a[3]);
void bisect_v3_v3v3v3(float r[3], float a[3], float b[3], float c[3]);

/*********************************** Other ***********************************/

void print_v2(char *str, float a[2]);
void print_v3(char *str, float a[3]);
void print_v4(char *str, float a[4]);

MINLINE void normal_short_to_float_v3(float r[3], short n[3]);
MINLINE void normal_float_to_short_v3(short r[3], float n[3]);

void minmax_v3_v3v3(float r[3], float min[3], float max[3]);

#ifdef __cplusplus
}
#endif

#endif /* BLI_MATH_VECTOR */

