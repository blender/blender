/*
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
 */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/********************************* Init **************************************/

void zero_m2(float m[2][2]);
void zero_m3(float m[3][3]);
void zero_m4(float m[4][4]);

void unit_m2(float m[2][2]);
void unit_m3(float m[3][3]);
void unit_m4(float m[4][4]);
void unit_m4_db(double m[4][4]);

void copy_m2_m2(float m1[2][2], const float m2[2][2]);
void copy_m3_m3(float m1[3][3], const float m2[3][3]);
void copy_m4_m4(float m1[4][4], const float m2[4][4]);
void copy_m3_m4(float m1[3][3], const float m2[4][4]);
void copy_m4_m3(float m1[4][4], const float m2[3][3]);
void copy_m3_m2(float m1[3][3], const float m2[2][2]);
void copy_m4_m2(float m1[4][4], const float m2[2][2]);

void copy_m4_m4_db(double m1[4][4], const double m2[4][4]);

/* double->float */
void copy_m3_m3d(float m1[3][3], const double m2[3][3]);

/* float->double */
void copy_m4d_m4(double m1[4][4], const float m2[4][4]);

void swap_m3m3(float m1[3][3], float m2[3][3]);
void swap_m4m4(float m1[4][4], float m2[4][4]);

/* Build index shuffle matrix */
void shuffle_m4(float R[4][4], const int index[4]);

/******************************** Arithmetic *********************************/

void add_m3_m3m3(float R[3][3], const float A[3][3], const float B[3][3]);
void add_m4_m4m4(float R[4][4], const float A[4][4], const float B[4][4]);

void madd_m3_m3m3fl(float R[3][3], const float A[3][3], const float B[3][3], const float f);
void madd_m4_m4m4fl(float R[4][4], const float A[4][4], const float B[4][4], const float f);

void sub_m3_m3m3(float R[3][3], const float A[3][3], const float B[3][3]);
void sub_m4_m4m4(float R[4][4], const float A[4][4], const float B[4][4]);

void mul_m3_m3m3(float R[3][3], const float A[3][3], const float B[3][3]);
void mul_m4_m3m4(float R[4][4], const float A[3][3], const float B[4][4]);
void mul_m4_m4m3(float R[4][4], const float A[4][4], const float B[3][3]);
void mul_m4_m4m4(float R[4][4], const float A[4][4], const float B[4][4]);
void mul_m3_m3m4(float R[3][3], const float A[3][3], const float B[4][4]);
void mul_m3_m4m3(float R[3][3], const float A[4][4], const float B[3][3]);
void mul_m3_m4m4(float R[3][3], const float A[4][4], const float B[4][4]);

/* special matrix multiplies
 * uniq: R <-- AB, R is neither A nor B
 * pre:  R <-- AR
 * post: R <-- RB
 */
void mul_m3_m3m3_uniq(float R[3][3], const float A[3][3], const float B[3][3]);
void mul_m3_m3_pre(float R[3][3], const float A[3][3]);
void mul_m3_m3_post(float R[3][3], const float B[3][3]);
void mul_m4_m4m4_uniq(float R[4][4], const float A[4][4], const float B[4][4]);
void mul_m4_m4m4_db_uniq(double R[4][4], const double A[4][4], const double B[4][4]);
void mul_m4db_m4db_m4fl_uniq(double R[4][4], const double A[4][4], const float B[4][4]);
void mul_m4_m4_pre(float R[4][4], const float A[4][4]);
void mul_m4_m4_post(float R[4][4], const float B[4][4]);

/* mul_m3_series */
void _va_mul_m3_series_3(float R[3][3], const float M1[3][3], const float M2[3][3]) ATTR_NONNULL();
void _va_mul_m3_series_4(float R[3][3],
                         const float M1[3][3],
                         const float M2[3][3],
                         const float M3[3][3]) ATTR_NONNULL();
void _va_mul_m3_series_5(float R[3][3],
                         const float M1[3][3],
                         const float M2[3][3],
                         const float M3[3][3],
                         const float M4[3][3]) ATTR_NONNULL();
void _va_mul_m3_series_6(float R[3][3],
                         const float M1[3][3],
                         const float M2[3][3],
                         const float M3[3][3],
                         const float M4[3][3],
                         const float M5[3][3]) ATTR_NONNULL();
void _va_mul_m3_series_7(float R[3][3],
                         const float M1[3][3],
                         const float M2[3][3],
                         const float M3[3][3],
                         const float M4[3][3],
                         const float M5[3][3],
                         const float M6[3][3]) ATTR_NONNULL();
void _va_mul_m3_series_8(float R[3][3],
                         const float M1[3][3],
                         const float M2[3][3],
                         const float M3[3][3],
                         const float M4[3][3],
                         const float M5[3][3],
                         const float M6[3][3],
                         const float M7[3][3]) ATTR_NONNULL();
void _va_mul_m3_series_9(float R[3][3],
                         const float M1[3][3],
                         const float M2[3][3],
                         const float M3[3][3],
                         const float M4[3][3],
                         const float M5[3][3],
                         const float M6[3][3],
                         const float M7[3][3],
                         const float M8[3][3]) ATTR_NONNULL();
/* mul_m4_series */
void _va_mul_m4_series_3(float R[4][4], const float M1[4][4], const float M2[4][4]) ATTR_NONNULL();
void _va_mul_m4_series_4(float R[4][4],
                         const float M1[4][4],
                         const float M2[4][4],
                         const float M3[4][4]) ATTR_NONNULL();
void _va_mul_m4_series_5(float R[4][4],
                         const float M1[4][4],
                         const float M2[4][4],
                         const float M3[4][4],
                         const float M4[4][4]) ATTR_NONNULL();
void _va_mul_m4_series_6(float R[4][4],
                         const float M1[4][4],
                         const float M2[4][4],
                         const float M3[4][4],
                         const float M4[4][4],
                         const float M5[4][4]) ATTR_NONNULL();
void _va_mul_m4_series_7(float R[4][4],
                         const float M1[4][4],
                         const float M2[4][4],
                         const float M3[4][4],
                         const float M4[4][4],
                         const float M5[4][4],
                         const float M6[4][4]) ATTR_NONNULL();
void _va_mul_m4_series_8(float R[4][4],
                         const float M1[4][4],
                         const float M2[4][4],
                         const float M3[4][4],
                         const float M4[4][4],
                         const float M5[4][4],
                         const float M6[4][4],
                         const float M7[4][4]) ATTR_NONNULL();
void _va_mul_m4_series_9(float R[4][4],
                         const float M1[4][4],
                         const float M2[4][4],
                         const float M3[4][4],
                         const float M4[4][4],
                         const float M5[4][4],
                         const float M6[4][4],
                         const float M7[4][4],
                         const float M8[4][4]) ATTR_NONNULL();

#define mul_m3_series(...) VA_NARGS_CALL_OVERLOAD(_va_mul_m3_series_, __VA_ARGS__)
#define mul_m4_series(...) VA_NARGS_CALL_OVERLOAD(_va_mul_m4_series_, __VA_ARGS__)

void mul_m4_v3(const float M[4][4], float r[3]);
void mul_v3_m4v3(float r[3], const float M[4][4], const float v[3]);
void mul_v3_m4v3_db(double r[3], const double mat[4][4], const double vec[3]);
void mul_v4_m4v3_db(double r[4], const double mat[4][4], const double vec[3]);
void mul_v2_m4v3(float r[2], const float M[4][4], const float v[3]);
void mul_v2_m2v2(float r[2], const float M[2][2], const float v[2]);
void mul_m2_v2(const float M[2][2], float v[2]);
void mul_mat3_m4_v3(const float M[4][4], float r[3]);
void mul_v3_mat3_m4v3(float r[3], const float M[4][4], const float v[3]);
void mul_v3_mat3_m4v3_db(double r[3], const double M[4][4], const double v[3]);
void mul_m4_v4(const float M[4][4], float r[4]);
void mul_v4_m4v4(float r[4], const float M[4][4], const float v[4]);
void mul_v4_m4v3(float r[4], const float M[4][4], const float v[3]); /* v has implicit w = 1.0f */
void mul_project_m4_v3(const float M[4][4], float vec[3]);
void mul_v3_project_m4_v3(float r[3], const float mat[4][4], const float vec[3]);
void mul_v2_project_m4_v3(float r[2], const float M[4][4], const float vec[3]);

void mul_m3_v2(const float m[3][3], float r[2]);
void mul_v2_m3v2(float r[2], const float m[3][3], const float v[2]);
void mul_m3_v3(const float M[3][3], float r[3]);
void mul_v3_m3v3(float r[3], const float M[3][3], const float a[3]);
void mul_v2_m3v3(float r[2], const float M[3][3], const float a[3]);
void mul_transposed_m3_v3(const float M[3][3], float r[3]);
void mul_transposed_mat3_m4_v3(const float M[4][4], float r[3]);
void mul_m3_v3_double(const float M[3][3], double r[3]);

void mul_m4_m4m4_aligned_scale(float R[4][4], const float A[4][4], const float B[4][4]);

void mul_m3_fl(float R[3][3], float f);
void mul_m4_fl(float R[4][4], float f);
void mul_mat3_m4_fl(float R[4][4], float f);

void negate_m3(float R[3][3]);
void negate_mat3_m4(float R[4][4]);
void negate_m4(float R[4][4]);

bool invert_m3_ex(float m[3][3], const float epsilon);
bool invert_m3_m3_ex(float m1[3][3], const float m2[3][3], const float epsilon);

bool invert_m3(float R[3][3]);
bool invert_m3_m3(float R[3][3], const float A[3][3]);
bool invert_m4(float R[4][4]);
bool invert_m4_m4(float R[4][4], const float A[4][4]);
bool invert_m4_m4_fallback(float R[4][4], const float A[4][4]);

/* double arithmetic (mixed float/double) */
void mul_m4_v4d(const float M[4][4], double r[4]);
void mul_v4d_m4v4d(double r[4], const float M[4][4], const double v[4]);

/* double matrix functions (no mixing types) */
void mul_v3_m3v3_db(double r[3], const double M[3][3], const double a[3]);
void mul_m3_v3_db(const double M[3][3], double r[3]);

/****************************** Linear Algebra *******************************/

void transpose_m3(float R[3][3]);
void transpose_m3_m3(float R[3][3], const float M[3][3]);
void transpose_m3_m4(float R[3][3], const float M[4][4]);
void transpose_m4(float R[4][4]);
void transpose_m4_m4(float R[4][4], const float M[4][4]);

bool compare_m4m4(const float mat1[4][4], const float mat2[4][4], float limit);

void normalize_m2_ex(float R[2][2], float r_scale[2]) ATTR_NONNULL();
void normalize_m2(float R[2][2]) ATTR_NONNULL();
void normalize_m2_m2_ex(float R[2][2], const float M[2][2], float r_scale[2]) ATTR_NONNULL();
void normalize_m2_m2(float R[2][2], const float M[2][2]) ATTR_NONNULL();
void normalize_m3_ex(float R[3][3], float r_scale[3]) ATTR_NONNULL();
void normalize_m3(float R[3][3]) ATTR_NONNULL();
void normalize_m3_m3_ex(float R[3][3], const float M[3][3], float r_scale[3]) ATTR_NONNULL();
void normalize_m3_m3(float R[3][3], const float M[3][3]) ATTR_NONNULL();
void normalize_m4_ex(float R[4][4], float r_scale[3]) ATTR_NONNULL();
void normalize_m4(float R[4][4]) ATTR_NONNULL();
void normalize_m4_m4_ex(float R[4][4], const float M[4][4], float r_scale[3]) ATTR_NONNULL();
void normalize_m4_m4(float R[4][4], const float M[4][4]) ATTR_NONNULL();

void orthogonalize_m3(float R[3][3], int axis);
void orthogonalize_m4(float R[4][4], int axis);

void orthogonalize_m3_stable(float R[3][3], int axis, bool normalize);
void orthogonalize_m4_stable(float R[4][4], int axis, bool normalize);

bool is_orthogonal_m3(const float mat[3][3]);
bool is_orthogonal_m4(const float mat[4][4]);
bool is_orthonormal_m3(const float mat[3][3]);
bool is_orthonormal_m4(const float mat[4][4]);

bool is_uniform_scaled_m3(const float mat[3][3]);
bool is_uniform_scaled_m4(const float m[4][4]);

/* Note: 'adjoint' here means the adjugate (adjunct, "classical adjoint") matrix!
 * Nowadays 'adjoint' usually refers to the conjugate transpose,
 * which for real-valued matrices is simply the transpose.
 */
void adjoint_m2_m2(float R[2][2], const float M[2][2]);
void adjoint_m3_m3(float R[3][3], const float M[3][3]);
void adjoint_m4_m4(float R[4][4], const float M[4][4]);

float determinant_m2(float a, float b, float c, float d);
float determinant_m3(
    float a1, float a2, float a3, float b1, float b2, float b3, float c1, float c2, float c3);
float determinant_m3_array(const float m[3][3]);
float determinant_m4_mat3_array(const float m[4][4]);
float determinant_m4(const float m[4][4]);

#define PSEUDOINVERSE_EPSILON 1e-8f

void svd_m4(float U[4][4], float s[4], float V[4][4], float A[4][4]);
void pseudoinverse_m4_m4(float Ainv[4][4], const float A[4][4], float epsilon);
void pseudoinverse_m3_m3(float Ainv[3][3], const float A[3][3], float epsilon);

bool has_zero_axis_m4(const float matrix[4][4]);

void invert_m4_m4_safe(float Ainv[4][4], const float A[4][4]);

void invert_m3_m3_safe_ortho(float Ainv[3][3], const float A[3][3]);
void invert_m4_m4_safe_ortho(float Ainv[4][4], const float A[4][4]);

/****************************** Transformations ******************************/

void scale_m3_fl(float R[3][3], float scale);
void scale_m4_fl(float R[4][4], float scale);

float mat3_to_volume_scale(const float M[3][3]);
float mat4_to_volume_scale(const float M[4][4]);

float mat3_to_scale(const float M[3][3]);
float mat4_to_scale(const float M[4][4]);
float mat4_to_xy_scale(const float M[4][4]);

void size_to_mat3(float R[3][3], const float size[3]);
void size_to_mat4(float R[4][4], const float size[3]);

void mat3_to_size(float size[3], const float M[3][3]);
void mat4_to_size(float size[3], const float M[4][4]);

void mat4_to_size_fix_shear(float size[3], const float M[4][4]);

void translate_m3(float mat[3][3], float tx, float ty);
void translate_m4(float mat[4][4], float tx, float ty, float tz);
void rotate_m3(float mat[3][3], const float angle);
void rotate_m4(float mat[4][4], const char axis, const float angle);
void rescale_m3(float mat[3][3], const float scale[2]);
void rescale_m4(float mat[4][4], const float scale[3]);
void transform_pivot_set_m3(float mat[3][3], const float pivot[2]);
void transform_pivot_set_m4(float mat[4][4], const float pivot[3]);

void mat4_to_rot(float rot[3][3], const float wmat[4][4]);
void mat3_to_rot_size(float rot[3][3], float size[3], const float mat3[3][3]);
void mat4_to_loc_rot_size(float loc[3], float rot[3][3], float size[3], const float wmat[4][4]);
void mat4_to_loc_quat(float loc[3], float quat[4], const float wmat[4][4]);
void mat4_decompose(float loc[3], float quat[4], float size[3], const float wmat[4][4]);

void mat3_polar_decompose(const float mat3[3][3], float r_U[3][3], float r_P[3][3]);

void loc_rot_size_to_mat3(float R[3][3],
                          const float loc[2],
                          const float angle,
                          const float size[2]);
void loc_rot_size_to_mat4(float R[4][4],
                          const float loc[3],
                          const float rot[3][3],
                          const float size[3]);
void loc_eul_size_to_mat4(float R[4][4],
                          const float loc[3],
                          const float eul[3],
                          const float size[3]);
void loc_eulO_size_to_mat4(
    float R[4][4], const float loc[3], const float eul[3], const float size[3], const short order);
void loc_quat_size_to_mat4(float R[4][4],
                           const float loc[3],
                           const float quat[4],
                           const float size[3]);
void loc_axisangle_size_to_mat4(float R[4][4],
                                const float loc[3],
                                const float axis[4],
                                const float angle,
                                const float size[3]);

void blend_m3_m3m3(float out[3][3],
                   const float dst[3][3],
                   const float src[3][3],
                   const float srcweight);
void blend_m4_m4m4(float out[4][4],
                   const float dst[4][4],
                   const float src[4][4],
                   const float srcweight);

void interp_m3_m3m3(float R[3][3], const float A[3][3], const float B[3][3], const float t);
void interp_m4_m4m4(float R[4][4], const float A[4][4], const float B[4][4], const float t);

bool is_negative_m3(const float mat[3][3]);
bool is_negative_m4(const float mat[4][4]);

bool is_zero_m3(const float mat[3][3]);
bool is_zero_m4(const float mat[4][4]);

bool equals_m3m3(const float mat1[3][3], const float mat2[3][3]);
bool equals_m4m4(const float mat1[4][4], const float mat2[4][4]);

/* SpaceTransform helper */
typedef struct SpaceTransform {
  float local2target[4][4];
  float target2local[4][4];

} SpaceTransform;

void BLI_space_transform_from_matrices(struct SpaceTransform *data,
                                       const float local[4][4],
                                       const float target[4][4]);
void BLI_space_transform_global_from_matrices(struct SpaceTransform *data,
                                              const float local[4][4],
                                              const float target[4][4]);
void BLI_space_transform_apply(const struct SpaceTransform *data, float co[3]);
void BLI_space_transform_invert(const struct SpaceTransform *data, float co[3]);
void BLI_space_transform_apply_normal(const struct SpaceTransform *data, float no[3]);
void BLI_space_transform_invert_normal(const struct SpaceTransform *data, float no[3]);

#define BLI_SPACE_TRANSFORM_SETUP(data, local, target) \
  BLI_space_transform_from_matrices((data), (local)->obmat, (target)->obmat)

/*********************************** Other ***********************************/

void print_m3(const char *str, const float M[3][3]);
void print_m4(const char *str, const float M[4][4]);

#define print_m3_id(M) print_m3(STRINGIFY(M), M)
#define print_m4_id(M) print_m4(STRINGIFY(M), M)

#ifdef __cplusplus
}
#endif
