/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_compiler_attrs.h"

/* -------------------------------------------------------------------- */
/** \name Init
 * \{ */

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

void copy_m4_m4_db(double m1[4][4], const double m2[4][4]);

/* double->float */

void copy_m3_m3d(float m1[3][3], const double m2[3][3]);

/* float->double */

void copy_m3d_m3(double m1[3][3], const float m2[3][3]);
void copy_m4d_m4(double m1[4][4], const float m2[4][4]);

void swap_m4m4(float m1[4][4], float m2[4][4]);

/** Build index shuffle matrix. */
void shuffle_m4(float R[4][4], const int index[4]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Arithmetic
 * \{ */

void add_m3_m3m3(float R[3][3], const float A[3][3], const float B[3][3]);
void add_m4_m4m4(float R[4][4], const float A[4][4], const float B[4][4]);

void madd_m3_m3m3fl(float R[3][3], const float A[3][3], const float B[3][3], float f);
void madd_m4_m4m4fl(float R[4][4], const float A[4][4], const float B[4][4], float f);

void sub_m3_m3m3(float R[3][3], const float A[3][3], const float B[3][3]);

void mul_m3_m3m3(float R[3][3], const float A[3][3], const float B[3][3]);
void mul_m4_m3m4(float R[4][4], const float A[3][3], const float B[4][4]);
void mul_m4_m4m3(float R[4][4], const float A[4][4], const float B[3][3]);
void mul_m4_m4m4(float R[4][4], const float A[4][4], const float B[4][4]);
/**
 * `R = A * B`, ignore the elements on the 4th row/column of A.
 */
void mul_m3_m3m4(float R[3][3], const float A[3][3], const float B[4][4]);
/**
 * `R = A * B`, ignore the elements on the 4th row/column of B.
 */
void mul_m3_m4m3(float R[3][3], const float A[4][4], const float B[3][3]);
void mul_m3_m4m4(float R[3][3], const float A[4][4], const float B[4][4]);

/**
 * Special matrix multiplies
 * - pre:  `R <-- AR`
 * - post: `R <-- RB`.
 */
void mul_m3_m3_pre(float R[3][3], const float A[3][3]);
void mul_m3_m3_post(float R[3][3], const float B[3][3]);
void mul_m4db_m4db_m4fl(double R[4][4], const double A[4][4], const float B[4][4]);
void mul_m4_m4_pre(float R[4][4], const float A[4][4]);
void mul_m4_m4_post(float R[4][4], const float B[4][4]);

/* Implement #mul_m3_series macro. */

void _va_mul_m3_series_3(float r[3][3], const float m1[3][3], const float m2[3][3]) ATTR_NONNULL();
void _va_mul_m3_series_4(float r[3][3],
                         const float m1[3][3],
                         const float m2[3][3],
                         const float m3[3][3]) ATTR_NONNULL();
void _va_mul_m3_series_5(float r[3][3],
                         const float m1[3][3],
                         const float m2[3][3],
                         const float m3[3][3],
                         const float m4[3][3]) ATTR_NONNULL();
void _va_mul_m3_series_6(float r[3][3],
                         const float m1[3][3],
                         const float m2[3][3],
                         const float m3[3][3],
                         const float m4[3][3],
                         const float m5[3][3]) ATTR_NONNULL();
void _va_mul_m3_series_7(float r[3][3],
                         const float m1[3][3],
                         const float m2[3][3],
                         const float m3[3][3],
                         const float m4[3][3],
                         const float m5[3][3],
                         const float m6[3][3]) ATTR_NONNULL();
void _va_mul_m3_series_8(float r[3][3],
                         const float m1[3][3],
                         const float m2[3][3],
                         const float m3[3][3],
                         const float m4[3][3],
                         const float m5[3][3],
                         const float m6[3][3],
                         const float m7[3][3]) ATTR_NONNULL();
void _va_mul_m3_series_9(float r[3][3],
                         const float m1[3][3],
                         const float m2[3][3],
                         const float m3[3][3],
                         const float m4[3][3],
                         const float m5[3][3],
                         const float m6[3][3],
                         const float m7[3][3],
                         const float m8[3][3]) ATTR_NONNULL();

/* Implement #mul_m4_series macro. */

void _va_mul_m4_series_3(float r[4][4], const float m1[4][4], const float m2[4][4]) ATTR_NONNULL();
void _va_mul_m4_series_4(float r[4][4],
                         const float m1[4][4],
                         const float m2[4][4],
                         const float m3[4][4]) ATTR_NONNULL();
void _va_mul_m4_series_5(float r[4][4],
                         const float m1[4][4],
                         const float m2[4][4],
                         const float m3[4][4],
                         const float m4[4][4]) ATTR_NONNULL();
void _va_mul_m4_series_6(float r[4][4],
                         const float m1[4][4],
                         const float m2[4][4],
                         const float m3[4][4],
                         const float m4[4][4],
                         const float m5[4][4]) ATTR_NONNULL();
void _va_mul_m4_series_7(float r[4][4],
                         const float m1[4][4],
                         const float m2[4][4],
                         const float m3[4][4],
                         const float m4[4][4],
                         const float m5[4][4],
                         const float m6[4][4]) ATTR_NONNULL();
void _va_mul_m4_series_8(float r[4][4],
                         const float m1[4][4],
                         const float m2[4][4],
                         const float m3[4][4],
                         const float m4[4][4],
                         const float m5[4][4],
                         const float m6[4][4],
                         const float m7[4][4]) ATTR_NONNULL();
void _va_mul_m4_series_9(float r[4][4],
                         const float m1[4][4],
                         const float m2[4][4],
                         const float m3[4][4],
                         const float m4[4][4],
                         const float m5[4][4],
                         const float m6[4][4],
                         const float m7[4][4],
                         const float m8[4][4]) ATTR_NONNULL();

#define mul_m3_series(...) VA_NARGS_CALL_OVERLOAD(_va_mul_m3_series_, __VA_ARGS__)
#define mul_m4_series(...) VA_NARGS_CALL_OVERLOAD(_va_mul_m4_series_, __VA_ARGS__)

void mul_m4_v3(const float M[4][4], float r[3]);
void mul_v3_m4v3(float r[3], const float mat[4][4], const float vec[3]);
void mul_v3_m4v3_db(double r[3], const double mat[4][4], const double vec[3]);
void mul_v4_m4v3_db(double r[4], const double mat[4][4], const double vec[3]);
void mul_v2_m4v3(float r[2], const float mat[4][4], const float vec[3]);
void mul_v2_m2v2(float r[2], const float mat[2][2], const float vec[2]);
void mul_m2_v2(const float mat[2][2], float vec[2]);
/** Same as #mul_m4_v3() but doesn't apply translation component. */
void mul_mat3_m4_v3(const float mat[4][4], float r[3]);
void mul_v3_mat3_m4v3(float r[3], const float mat[4][4], const float vec[3]);
void mul_v3_mat3_m4v3_db(double r[3], const double mat[4][4], const double vec[3]);
void mul_m4_v4(const float mat[4][4], float r[4]);
void mul_v4_m4v4(float r[4], const float mat[4][4], const float v[4]);
void mul_v4_m4v3(float r[4], const float M[4][4], const float v[3]); /* v has implicit w = 1.0f */
void mul_project_m4_v3(const float mat[4][4], float vec[3]);
void mul_v3_project_m4_v3(float r[3], const float mat[4][4], const float vec[3]);
void mul_v2_project_m4_v3(float r[2], const float mat[4][4], const float vec[3]);

void mul_m3_v2(const float m[3][3], float r[2]);
void mul_v2_m3v2(float r[2], const float m[3][3], const float v[2]);
void mul_m3_v3(const float M[3][3], float r[3]);
void mul_v3_m3v3(float r[3], const float M[3][3], const float a[3]);
void mul_v2_m3v3(float r[2], const float M[3][3], const float a[3]);
void mul_transposed_m3_v3(const float M[3][3], float r[3]);
void mul_transposed_mat3_m4_v3(const float M[4][4], float r[3]);

/**
 * Combines transformations, handling scale separately in a manner equivalent
 * to the Aligned Inherit Scale mode, in order to avoid creating shear.
 * If A scale is uniform, the result is equivalent to ordinary multiplication.
 *
 * NOTE: this effectively takes output location from simple multiplication,
 *       and uses mul_m4_m4m4_split_channels for rotation and scale.
 */
void mul_m4_m4m4_aligned_scale(float R[4][4], const float A[4][4], const float B[4][4]);
/**
 * Separately combines location, rotation and scale of the input matrices.
 */
void mul_m4_m4m4_split_channels(float R[4][4], const float A[4][4], const float B[4][4]);

void mul_m3_fl(float R[3][3], float f);
void mul_m4_fl(float R[4][4], float f);
void mul_mat3_m4_fl(float R[4][4], float f);

void negate_m3(float R[3][3]);
void negate_mat3_m4(float R[4][4]);
void negate_m4(float R[4][4]);

bool invert_m3(float mat[3][3]);
bool invert_m2_m2(float inverse[2][2], const float mat[2][2]);
bool invert_m3_m3(float inverse[3][3], const float mat[3][3]);
bool invert_m4(float mat[4][4]);
bool invert_m4_m4(float inverse[4][4], const float mat[4][4]);
/**
 * Computes the inverse of mat and puts it in inverse.
 * Uses Gaussian Elimination with partial (maximal column) pivoting.
 * \return true on success (i.e. can always find a pivot) and false on failure.
 * Mark Segal - 1992.
 *
 * \note this has worse performance than #EIG_invert_m4_m4 (Eigen), but e.g.
 * for non-invertible scale matrices, finding a partial solution can
 * be useful to have a valid local transform center, see #57767.
 */
bool invert_m4_m4_fallback(float inverse[4][4], const float mat[4][4]);

/* Double matrix functions (no mixing types). */

void mul_v3_m3v3_db(double r[3], const double M[3][3], const double a[3]);
void mul_m3_v3_db(const double M[3][3], double r[3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Linear Algebra
 * \{ */

void transpose_m3(float R[3][3]);
void transpose_m3_m3(float R[3][3], const float M[3][3]);
/**
 * \note Seems obscure but in-fact a common operation.
 */
void transpose_m3_m4(float R[3][3], const float M[4][4]);
void transpose_m4(float R[4][4]);
void transpose_m4_m4(float R[4][4], const float M[4][4]);

bool compare_m4m4(const float mat1[4][4], const float mat2[4][4], float limit);

void normalize_m2_m2(float R[2][2], const float M[2][2]) ATTR_NONNULL();
void normalize_m3(float R[3][3]) ATTR_NONNULL();
void normalize_m3_m3(float R[3][3], const float M[3][3]) ATTR_NONNULL();
void normalize_m4_ex(float R[4][4], float r_scale[3]) ATTR_NONNULL();
void normalize_m4(float R[4][4]) ATTR_NONNULL();
void normalize_m4_m4(float rmat[4][4], const float mat[4][4]) ATTR_NONNULL();

/**
 * Make an orthonormal matrix around the selected axis of the given matrix.
 *
 * \param axis: Axis to build the orthonormal basis around.
 */
void orthogonalize_m3(float R[3][3], int axis);
/**
 * Make an orthonormal matrix around the selected axis of the given matrix.
 *
 * \param axis: Axis to build the orthonormal basis around.
 */
void orthogonalize_m4(float R[4][4], int axis);

/**
 * Make an orthonormal matrix around the selected axis of the given matrix,
 * in a way that is symmetric and stable to variations in the input, and
 * preserving the value of the determinant, i.e. the overall volume change.
 *
 * \param axis: Axis to build the orthonormal basis around.
 * \param normalize: Normalize the matrix instead of preserving volume.
 */
void orthogonalize_m4_stable(float R[4][4], int axis, bool normalize);

bool orthogonalize_m3_zero_axes(float m[3][3], float unit_length);
bool orthogonalize_m4_zero_axes(float m[4][4], float unit_length);

bool is_orthogonal_m3(const float m[3][3]);
bool is_orthogonal_m4(const float m[4][4]);
bool is_orthonormal_m3(const float m[3][3]);
bool is_orthonormal_m4(const float m[4][4]);

bool is_identity_m4(const float m[4][4]);
bool is_uniform_scaled_m3(const float m[3][3]);
bool is_uniform_scaled_m4(const float m[4][4]);

/* NOTE: 'adjoint' here means the adjugate (adjunct, "classical adjoint") matrix!
 * Nowadays 'adjoint' usually refers to the conjugate transpose,
 * which for real-valued matrices is simply the transpose. */

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

/**
 * Compute the Single Value Decomposition of an arbitrary matrix A
 * That is compute the 3 matrices U,W,V with U column orthogonal (m,n)
 * ,W a diagonal matrix and V an orthogonal square matrix `s.t.A = U.W.Vt`.
 * From this decomposition it is trivial to compute the (pseudo-inverse)
 * of `A` as `Ainv = V.Winv.transpose(U)`.
 */
void svd_m4(float U[4][4], float s[4], float V[4][4], float A_[4][4]);
void pseudoinverse_m4_m4(float inverse[4][4], const float mat[4][4], float epsilon);
void pseudoinverse_m3_m3(float inverse[3][3], const float mat[3][3], float epsilon);

bool has_zero_axis_m4(const float matrix[4][4]);

void invert_m4_m4_safe(float inverse[4][4], const float mat[4][4]);

void invert_m3_m3_safe_ortho(float inverse[3][3], const float mat[3][3]);
/**
 * A safe version of invert that uses valid axes, calculating the zeroed axis
 * based on the non-zero ones.
 *
 * This works well for transformation matrices, when a single axis is zeroed.
 */
void invert_m4_m4_safe_ortho(float inverse[4][4], const float mat[4][4]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transformations
 * \{ */

void scale_m3_fl(float R[3][3], float scale);
void scale_m4_fl(float R[4][4], float scale);

/**
 * This computes the overall volume scale factor of a transformation matrix.
 * For an orthogonal matrix, it is the product of all three scale values.
 * Returns a negative value if the transform is flipped by negative scale.
 */
float mat4_to_volume_scale(const float mat[4][4]);

/**
 * This gets the average scale of a matrix, only use when your scaling
 * data that has no idea of scale axis, examples are bone-envelope-radius
 * and curve radius.
 */
float mat3_to_scale(const float mat[3][3]);
float mat4_to_scale(const float mat[4][4]);

void size_to_mat3(float R[3][3], const float size[3]);
void size_to_mat4(float R[4][4], const float size[3]);

void mat3_to_size(float size[3], const float M[3][3]);
void mat4_to_size(float size[3], const float M[4][4]);

/**
 * Return the largest scale on any axis, the equivalent of calling:
 * \code{.c}
 * mat3_to_size(size_v3, mat);
 * size = size_v3[max_axis_v3(size_v3)];
 * \endcode
 * .. without 2x unnecessary `sqrtf` calls.
 * Only the first 3 axes are used.
 */
float mat4_to_size_max_axis(const float M[4][4]);

/**
 * Extract scale factors from the matrix, with correction to ensure
 * exact volume in case of a sheared matrix.
 */
void mat4_to_size_fix_shear(float size[3], const float M[4][4]);

void translate_m4(float mat[4][4], float Tx, float Ty, float Tz);
/**
 * Rotate a matrix in-place.
 *
 * \note To create a new rotation matrix see:
 * #axis_angle_to_mat4_single, #axis_angle_to_mat3_single, #angle_to_mat2
 * (axis & angle args are compatible).
 */
void rotate_m4(float mat[4][4], char axis, float angle);
/** Scale a matrix in-place. */
void rescale_m4(float mat[4][4], const float scale[3]);
/**
 * Scale or rotate around a pivot point,
 * a convenience function to avoid having to do inline.
 *
 * Since its common to make a scale/rotation matrix that pivots around an arbitrary point.
 *
 * Typical use case is to make 3x3 matrix, copy to 4x4, then pass to this function.
 */
void transform_pivot_set_m4(float mat[4][4], const float pivot[3]);

/**
 * \param rot: A 3x3 rotation matrix, normalized never negative.
 * \param size: The scale, negative if `mat3` is negative.
 */
void mat3_to_rot_size(float rot[3][3], float size[3], const float mat3[3][3]);
/**
 * \param rot: A 3x3 rotation matrix, normalized never negative.
 * \param size: The scale, negative if `mat3` is negative.
 */
void mat4_to_loc_rot_size(float loc[3], float rot[3][3], float size[3], const float wmat[4][4]);
void mat4_to_loc_quat(float loc[3], float quat[4], const float wmat[4][4]);
void mat4_decompose(float loc[3], float quat[4], float size[3], const float wmat[4][4]);

void mat3_polar_decompose(const float mat3[3][3], float r_U[3][3], float r_P[3][3]);

/**
 * Make a 4x4 matrix out of 3 transform components.
 * Matrices are made in the order: `scale * rot * loc`
 */
void loc_rot_size_to_mat4(float R[4][4],
                          const float loc[3],
                          const float rot[3][3],
                          const float size[3]);
/**
 * Make a 4x4 matrix out of 3 transform components.
 * Matrices are made in the order: `scale * rot * loc`
 *
 * TODO: need to have a version that allows for rotation order.
 */
void loc_eul_size_to_mat4(float R[4][4],
                          const float loc[3],
                          const float eul[3],
                          const float size[3]);
/**
 * Make a 4x4 matrix out of 3 transform components.
 * Matrices are made in the order: `scale * rot * loc`
 */
void loc_eulO_size_to_mat4(
    float R[4][4], const float loc[3], const float eul[3], const float size[3], short order);
/**
 * Make a 4x4 matrix out of 3 transform components.
 * Matrices are made in the order: `scale * rot * loc`
 */
void loc_quat_size_to_mat4(float R[4][4],
                           const float loc[3],
                           const float quat[4],
                           const float size[3]);

void blend_m3_m3m3(float out[3][3], const float dst[3][3], const float src[3][3], float srcweight);
void blend_m4_m4m4(float out[4][4], const float dst[4][4], const float src[4][4], float srcweight);

/**
 * A polar-decomposition-based interpolation between matrix A and matrix B.
 *
 * \note This code is about five times slower as the 'naive' interpolation done by #blend_m3_m3m3
 * (it typically remains below 2 usec on an average i74700,
 * while #blend_m3_m3m3 remains below 0.4 usec).
 * However, it gives expected results even with non-uniformly scaled matrices,
 * see #46418 for an example.
 *
 * Based on "Matrix Animation and Polar Decomposition", by Ken Shoemake & Tom Duff
 *
 * \param R: Resulting interpolated matrix.
 * \param A: Input matrix which is totally effective with `t = 0.0`.
 * \param B: Input matrix which is totally effective with `t = 1.0`.
 * \param t: Interpolation factor.
 */
void interp_m3_m3m3(float R[3][3], const float A[3][3], const float B[3][3], float t);
/**
 * Complete transform matrix interpolation,
 * based on polar-decomposition-based interpolation from #interp_m3_m3m3.
 *
 * \param R: Resulting interpolated matrix.
 * \param A: Input matrix which is totally effective with `t = 0.0`.
 * \param B: Input matrix which is totally effective with `t = 1.0`.
 * \param t: Interpolation factor.
 */
void interp_m4_m4m4(float R[4][4], const float A[4][4], const float B[4][4], float t);

/**
 * Return true when the matrices determinant is less than zero.
 *
 * \note This is often used to check if a matrix flips content in 3D space,
 * where transforming geometry (for example) would flip the direction of polygon normals
 * from pointing outside a closed volume, to pointing inside (or the reverse).
 *
 * When the matrix is constructed from location, rotation & scale
 * as matrix will be negative when it has an odd number of negative scales.
 */
bool is_negative_m3(const float mat[3][3]);
/** A version of #is_negative_m3 that takes a 4x4 matrix. */
bool is_negative_m4(const float mat[4][4]);

bool is_zero_m4(const float mat[4][4]);

bool equals_m3m3(const float mat1[3][3], const float mat2[3][3]);
bool equals_m4m4(const float mat1[4][4], const float mat2[4][4]);

/**
 * #SpaceTransform struct encapsulates all needed data to convert between two coordinate spaces
 * (where conversion can be represented by a matrix multiplication).
 *
 * A #SpaceTransform is initialized using:
 * - #BLI_SPACE_TRANSFORM_SETUP(&data, ob1, ob2)
 *
 * After that the following calls can be used:
 * - Converts a coordinate in ob1 space to the corresponding ob2 space:
 *   #BLI_space_transform_apply(&data, co);
 * - Converts a coordinate in ob2 space to the corresponding ob1 space:
 *   #BLI_space_transform_invert(&data, co);
 *
 * Same concept as #BLI_space_transform_apply and #BLI_space_transform_invert,
 * but no is normalized after conversion (and not translated at all!):
 * - #BLI_space_transform_apply_normal(&data, no);
 * - #BLI_space_transform_invert_normal(&data, no);
 */
typedef struct SpaceTransform {
  float local2target[4][4];
  float target2local[4][4];

} SpaceTransform;

/**
 * Global-invariant transform.
 *
 * This defines a matrix transforming a point in local space to a point in target space
 * such that its global coordinates remain unchanged.
 *
 * In other words, if we have a global point P with local coordinates (x, y, z)
 * and global coordinates (X, Y, Z),
 * this defines a transform matrix TM such that (x', y', z') = TM * (x, y, z)
 * where (x', y', z') are the coordinates of P' in target space
 * such that it keeps (X, Y, Z) coordinates in global space.
 */
void BLI_space_transform_from_matrices(struct SpaceTransform *data,
                                       const float local[4][4],
                                       const float target[4][4]);
/**
 * Local-invariant transform.
 *
 * This defines a matrix transforming a point in global space
 * such that its local coordinates (from local space to target space) remain unchanged.
 *
 * In other words, if we have a local point p with local coordinates (x, y, z)
 * and global coordinates (X, Y, Z),
 * this defines a transform matrix TM such that (X', Y', Z') = TM * (X, Y, Z)
 * where (X', Y', Z') are the coordinates of p' in global space
 * such that it keeps (x, y, z) coordinates in target space.
 */
void BLI_space_transform_global_from_matrices(struct SpaceTransform *data,
                                              const float local[4][4],
                                              const float target[4][4]);
void BLI_space_transform_apply(const struct SpaceTransform *data, float co[3]);
void BLI_space_transform_invert(const struct SpaceTransform *data, float co[3]);
void BLI_space_transform_apply_normal(const struct SpaceTransform *data, float no[3]);
void BLI_space_transform_invert_normal(const struct SpaceTransform *data, float no[3]);

#define BLI_SPACE_TRANSFORM_SETUP(data, local, target) \
  BLI_space_transform_from_matrices( \
      (data), (local)->object_to_world().ptr(), (target)->object_to_world().ptr())

/** \} */

/* -------------------------------------------------------------------- */
/** \name Other
 * \{ */

void print_m3(const char *str, const float m[3][3]);
void print_m4(const char *str, const float m[4][4]);

#define print_m3_id(M) print_m3(STRINGIFY(M), M)
#define print_m4_id(M) print_m4(STRINGIFY(M), M)

/** \} */
