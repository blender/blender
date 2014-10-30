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
 
 * The Original Code is: some of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 * */

#ifndef __BLI_MATH_MATRIX_H__
#define __BLI_MATH_MATRIX_H__

/** \file BLI_math_matrix.h
 *  \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_compiler_attrs.h"

/********************************* Init **************************************/

void zero_m2(float R[2][2]);
void zero_m3(float R[3][3]);
void zero_m4(float R[4][4]);

void unit_m2(float R[2][2]);
void unit_m3(float R[3][3]);
void unit_m4(float R[4][4]);

void copy_m2_m2(float R[2][2], float A[2][2]);
void copy_m3_m3(float R[3][3], float A[3][3]);
void copy_m4_m4(float R[4][4], float A[4][4]);
void copy_m3_m4(float R[3][3], float A[4][4]);
void copy_m4_m3(float R[4][4], float A[3][3]);

/* double->float */
void copy_m3_m3d(float R[3][3], double A[3][3]);

void swap_m3m3(float A[3][3], float B[3][3]);
void swap_m4m4(float A[4][4], float B[4][4]);

/******************************** Arithmetic *********************************/

void add_m3_m3m3(float R[3][3], float A[3][3], float B[3][3]);
void add_m4_m4m4(float R[4][4], float A[4][4], float B[4][4]);

void sub_m3_m3m3(float R[3][3], float A[3][3], float B[3][3]);
void sub_m4_m4m4(float R[4][4], float A[4][4], float B[4][4]);

void mul_m3_m3m3(float R[3][3], float A[3][3], float B[3][3]);
void mul_m4_m3m4(float R[4][4], float A[3][3], float B[4][4]);
void mul_m4_m4m3(float R[4][4], float A[4][4], float B[3][3]);
void mul_m4_m4m4(float R[4][4], float A[4][4], float B[4][4]);
void mul_m3_m3m4(float R[3][3], float A[4][4], float B[3][3]);

/* mul_m3_series */
void _va_mul_m3_series_3(float R[3][3], float M1[3][3], float M2[3][3]) ATTR_NONNULL();
void _va_mul_m3_series_4(float R[3][3], float M1[3][3], float M2[3][3], float M3[3][3]) ATTR_NONNULL();
void _va_mul_m3_series_5(float R[3][3], float M1[3][3], float M2[3][3], float M3[3][3], float M4[3][3]) ATTR_NONNULL();
void _va_mul_m3_series_6(float R[3][3], float M1[3][3], float M2[3][3], float M3[3][3], float M4[3][3],
                         float M5[3][3]) ATTR_NONNULL();
void _va_mul_m3_series_7(float R[3][3], float M1[3][3], float M2[3][3], float M3[3][3], float M4[3][3],
                         float M5[3][3], float M6[3][3]) ATTR_NONNULL();
void _va_mul_m3_series_8(float R[3][3], float M1[3][3], float M2[3][3], float M3[3][3], float M4[3][3],
                         float M5[3][3], float M6[3][3], float M7[3][3]) ATTR_NONNULL();
void _va_mul_m3_series_9(float R[3][3], float M1[3][3], float M2[3][3], float M3[3][3], float M4[3][3],
                         float M5[3][3], float M6[3][3], float M7[3][3], float M8[3][3]) ATTR_NONNULL();
/* mul_m4_series */
void _va_mul_m4_series_3(float R[4][4], float M1[4][4], float M2[4][4]) ATTR_NONNULL();
void _va_mul_m4_series_4(float R[4][4], float M1[4][4], float M2[4][4], float M3[4][4]) ATTR_NONNULL();
void _va_mul_m4_series_5(float R[4][4], float M1[4][4], float M2[4][4], float M3[4][4], float M4[4][4]) ATTR_NONNULL();
void _va_mul_m4_series_6(float R[4][4], float M1[4][4], float M2[4][4], float M3[4][4], float M4[4][4],
                        float M5[4][4]) ATTR_NONNULL();
void _va_mul_m4_series_7(float R[4][4], float M1[4][4], float M2[4][4], float M3[4][4], float M4[4][4],
                         float M5[4][4], float M6[4][4]) ATTR_NONNULL();
void _va_mul_m4_series_8(float R[4][4], float M1[4][4], float M2[4][4], float M3[4][4], float M4[4][4],
                         float M5[4][4], float M6[4][4], float M7[4][4]) ATTR_NONNULL();
void _va_mul_m4_series_9(float R[4][4], float M1[4][4], float M2[4][4], float M3[4][4], float M4[4][4],
                         float M5[4][4], float M6[4][4], float M7[4][4], float M8[4][4]) ATTR_NONNULL();

#define mul_m3_series(...) VA_NARGS_CALL_OVERLOAD(_va_mul_m3_series_, __VA_ARGS__)
#define mul_m4_series(...) VA_NARGS_CALL_OVERLOAD(_va_mul_m4_series_, __VA_ARGS__)

void mul_m4_v3(float M[4][4], float r[3]);
void mul_v3_m4v3(float r[3], float M[4][4], const float v[3]);
void mul_v2_m4v3(float r[2], float M[4][4], const float v[3]);
void mul_v2_m2v2(float r[2], float M[2][2], const float v[2]);
void mul_m2v2(float M[2][2], float v[2]);
void mul_mat3_m4_v3(float M[4][4], float r[3]);
void mul_m4_v4(float M[4][4], float r[4]);
void mul_v4_m4v4(float r[4], float M[4][4], const float v[4]);
void mul_project_m4_v3(float M[4][4], float vec[3]);
void mul_v3_project_m4_v3(float r[3], float mat[4][4], const float vec[3]);
void mul_v2_project_m4_v3(float r[2], float M[4][4], const float vec[3]);

void mul_m3_v2(float m[3][3], float r[2]);
void mul_v2_m3v2(float r[2], float m[3][3], float v[2]);
void mul_m3_v3(float M[3][3], float r[3]);
void mul_v3_m3v3(float r[3], float M[3][3], const float a[3]);
void mul_v2_m3v3(float r[2], float M[3][3], const float a[3]);
void mul_transposed_m3_v3(float M[3][3], float r[3]);
void mul_transposed_mat3_m4_v3(float M[4][4], float r[3]);
void mul_m3_v3_double(float M[3][3], double r[3]);

void mul_m3_fl(float R[3][3], float f);
void mul_m4_fl(float R[4][4], float f);
void mul_mat3_m4_fl(float R[4][4], float f);

void negate_m3(float R[3][3]);
void negate_m4(float R[4][4]);

bool invert_m3_ex(float m[3][3], const float epsilon);
bool invert_m3_m3_ex(float m1[3][3], float m2[3][3], const float epsilon);

bool invert_m3(float R[3][3]);
bool invert_m3_m3(float R[3][3], float A[3][3]);
bool invert_m4(float R[4][4]);
bool invert_m4_m4(float R[4][4], float A[4][4]);

/* double ariphmetics */
void mul_m4_v4d(float M[4][4], double r[4]);
void mul_v4d_m4v4d(double r[4], float M[4][4], double v[4]);


/****************************** Linear Algebra *******************************/

void transpose_m3(float R[3][3]);
void transpose_m3_m3(float R[3][3], float A[3][3]);
void transpose_m3_m4(float R[3][3], float A[4][4]);
void transpose_m4(float R[4][4]);
void transpose_m4_m4(float R[4][4], float A[4][4]);

int compare_m4m4(float mat1[4][4], float mat2[4][4], float limit);

void normalize_m3(float R[3][3]);
void normalize_m3_m3(float R[3][3], float A[3][3]);
void normalize_m4(float R[4][4]);
void normalize_m4_m4(float R[4][4], float A[4][4]);

void orthogonalize_m3(float R[3][3], int axis);
void orthogonalize_m4(float R[4][4], int axis);

bool is_orthogonal_m3(float mat[3][3]);
bool is_orthogonal_m4(float mat[4][4]);
bool is_orthonormal_m3(float mat[3][3]);
bool is_orthonormal_m4(float mat[4][4]);

bool is_uniform_scaled_m3(float mat[3][3]);
bool is_uniform_scaled_m4(float m[4][4]);

void adjoint_m2_m2(float R[2][2], float A[2][2]);
void adjoint_m3_m3(float R[3][3], float A[3][3]);
void adjoint_m4_m4(float R[4][4], float A[4][4]);

float determinant_m2(float a, float b,
                     float c, float d);
float determinant_m3(float a, float b, float c,
                     float d, float e, float f,
                     float g, float h, float i);
float determinant_m3_array(float m[3][3]);
float determinant_m4(float A[4][4]);

#define PSEUDOINVERSE_EPSILON 1e-8f

void svd_m4(float U[4][4], float s[4], float V[4][4], float A[4][4]);
void pseudoinverse_m4_m4(float Ainv[4][4], float A[4][4], float epsilon);
void pseudoinverse_m3_m3(float Ainv[3][3], float A[3][3], float epsilon);

bool has_zero_axis_m4(float matrix[4][4]);

void invert_m4_m4_safe(float Ainv[4][4], float A[4][4]);

/****************************** Transformations ******************************/

void scale_m3_fl(float R[3][3], float scale);
void scale_m4_fl(float R[4][4], float scale);

float mat3_to_scale(float M[3][3]);
float mat4_to_scale(float M[4][4]);

void size_to_mat3(float R[3][3], const float size[3]);
void size_to_mat4(float R[4][4], const float size[3]);

void mat3_to_size(float r[3], float M[3][3]);
void mat4_to_size(float r[3], float M[4][4]);

void translate_m4(float mat[4][4], float tx, float ty, float tz);
void rotate_m4(float mat[4][4], const char axis, const float angle);
void rotate_m2(float mat[2][2], const float angle);
void transform_pivot_set_m4(float mat[4][4], const float pivot[3]);

void mat3_to_rot_size(float rot[3][3], float size[3], float mat3[3][3]);
void mat4_to_loc_rot_size(float loc[3], float rot[3][3], float size[3], float wmat[4][4]);
void mat4_to_loc_quat(float loc[3], float quat[4], float wmat[4][4]);
void mat4_decompose(float loc[3], float quat[4], float size[3], float wmat[4][4]);

void loc_eul_size_to_mat4(float R[4][4],
                          const float loc[3], const float eul[3], const float size[3]);
void loc_eulO_size_to_mat4(float R[4][4],
                           const float loc[3], const float eul[3], const float size[3], const short order);
void loc_quat_size_to_mat4(float R[4][4],
                           const float loc[3], const float quat[4], const float size[3]);
void loc_axisangle_size_to_mat4(float R[4][4],
                                const float loc[3], const float axis[4], const float angle, const float size[3]);

void blend_m3_m3m3(float R[3][3], float A[3][3], float B[3][3], const float t);
void blend_m4_m4m4(float R[4][4], float A[4][4], float B[4][4], const float t);

bool is_negative_m3(float mat[3][3]);
bool is_negative_m4(float mat[4][4]);

bool is_zero_m3(float mat[3][3]);
bool is_zero_m4(float mat[4][4]);

/* SpaceTransform helper */
typedef struct SpaceTransform {
	float local2target[4][4];
	float target2local[4][4];

} SpaceTransform;

void BLI_space_transform_from_matrices(struct SpaceTransform *data, float local[4][4], float target[4][4]);
void BLI_space_transform_apply(const struct SpaceTransform *data, float co[3]);
void BLI_space_transform_invert(const struct SpaceTransform *data, float co[3]);
void BLI_space_transform_apply_normal(const struct SpaceTransform *data, float no[3]);
void BLI_space_transform_invert_normal(const struct SpaceTransform *data, float no[3]);

#define BLI_SPACE_TRANSFORM_SETUP(data, local, target) \
	BLI_space_transform_from_matrices((data), (local)->obmat, (target)->obmat)

/*********************************** Other ***********************************/

void print_m3(const char *str, float M[3][3]);
void print_m4(const char *str, float M[3][4]);

#define print_m3_id(M) print_m3(STRINGIFY(M), M)
#define print_m4_id(M) print_m4(STRINGIFY(M), M)

#ifdef __cplusplus
}
#endif

#endif /* __BLI_MATH_MATRIX_H__ */

