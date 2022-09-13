/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_math_base.h"
#include "BLI_utildefines.h"
#include "DNA_vec_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/** \name Conversion Defines
 * \{ */

#define RAD2DEG(_rad) ((_rad) * (180.0 / M_PI))
#define DEG2RAD(_deg) ((_deg) * (M_PI / 180.0))

#define RAD2DEGF(_rad) ((_rad) * (float)(180.0 / M_PI))
#define DEG2RADF(_deg) ((_deg) * (float)(M_PI / 180.0))

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternions
 * Stored in (w, x, y, z) order.
 * \{ */

/* Initialize */

/* Convenience, avoids setting Y axis everywhere. */

void unit_axis_angle(float axis[3], float *angle);
void unit_qt(float q[4]);
void copy_qt_qt(float q[4], const float a[4]);

/* Arithmetic. */

void mul_qt_qtqt(float q[4], const float a[4], const float b[4]);
/**
 * \note
 * Assumes a unit quaternion?
 *
 * in fact not, but you may want to use a unit quaternion read on...
 *
 * Shortcut for 'q v q*' when \a v is actually a quaternion.
 * This removes the need for converting a vector to a quaternion,
 * calculating q's conjugate and converting back to a vector.
 * It also happens to be faster (17+,24* vs * 24+,32*).
 * If \a q is not a unit quaternion, then \a v will be both rotated by
 * the same amount as if q was a unit quaternion, and scaled by the square of
 * the length of q.
 *
 * For people used to python mathutils, its like:
 * def mul_qt_v3(q, v): (q * Quaternion((0.0, v[0], v[1], v[2])) * q.conjugated())[1:]
 *
 * \note Multiplying by 3x3 matrix is ~25% faster.
 */
void mul_qt_v3(const float q[4], float r[3]);
/**
 * Simple multiply.
 */
void mul_qt_fl(float q[4], float f);

/**
 * Raise a unit quaternion to the specified power.
 */
void pow_qt_fl_normalized(float q[4], float fac);

void sub_qt_qtqt(float q[4], const float a[4], const float b[4]);

void invert_qt(float q[4]);
void invert_qt_qt(float q1[4], const float q2[4]);
/**
 * This is just conjugate_qt for cases we know \a q is unit-length.
 * we could use #conjugate_qt directly, but use this function to show intent,
 * and assert if its ever becomes non-unit-length.
 */
void invert_qt_normalized(float q[4]);
void invert_qt_qt_normalized(float q1[4], const float q2[4]);
void conjugate_qt(float q[4]);
void conjugate_qt_qt(float q1[4], const float q2[4]);
float dot_qtqt(const float a[4], const float b[4]);
float normalize_qt(float q[4]);
float normalize_qt_qt(float r[4], const float q[4]);

/* Comparison. */

bool is_zero_qt(const float q[4]);

/* interpolation */
/**
 * Generic function for implementing slerp
 * (quaternions and spherical vector coords).
 *
 * \param t: factor in [0..1]
 * \param cosom: dot product from normalized vectors/quats.
 * \param r_w: calculated weights.
 */
void interp_dot_slerp(float t, float cosom, float r_w[2]);
void interp_qt_qtqt(float q[4], const float a[4], const float b[4], float t);
void add_qt_qtqt(float q[4], const float a[4], const float b[4], float t);

/* Conversion. */

void quat_to_mat3(float m[3][3], const float q[4]);
void quat_to_mat4(float m[4][4], const float q[4]);

/**
 * Apply the rotation of \a a to \a q keeping the values compatible with \a old.
 * Avoid axis flipping for animated f-curves for eg.
 */
void quat_to_compatible_quat(float q[4], const float a[4], const float old[4]);

/**
 * A version of #mat3_normalized_to_quat that skips error checking.
 */
void mat3_normalized_to_quat_fast(float q[4], const float mat[3][3]);

void mat3_normalized_to_quat(float q[4], const float mat[3][3]);
void mat4_normalized_to_quat(float q[4], const float mat[4][4]);
void mat3_to_quat(float q[4], const float mat[3][3]);
void mat4_to_quat(float q[4], const float mat[4][4]);
/**
 * Same as tri_to_quat() but takes pre-computed normal from the triangle
 * used for ngons when we know their normal.
 */
void tri_to_quat_ex(float quat[4],
                    const float v1[3],
                    const float v2[3],
                    const float v3[3],
                    const float no_orig[3]);
/**
 * \return the length of the normal, use to test for degenerate triangles.
 */
float tri_to_quat(float q[4], const float a[3], const float b[3], const float c[3]);
void vec_to_quat(float q[4], const float vec[3], short axis, short upflag);
/**
 * Calculate a rotation matrix from 2 normalized vectors.
 * \note `v1` and `v2` must be normalized.
 */
void rotation_between_vecs_to_mat3(float m[3][3], const float v1[3], const float v2[3]);
/**
 * \note Expects vectors to be normalized.
 */
void rotation_between_vecs_to_quat(float q[4], const float v1[3], const float v2[3]);
void rotation_between_quats_to_quat(float q[4], const float q1[4], const float q2[4]);

/**
 * Decompose a quaternion into a swing rotation (quaternion with the selected
 * axis component locked at zero), followed by a twist rotation around the axis.
 *
 * \param q: input quaternion.
 * \param axis: twist axis in [0,1,2]
 * \param r_swing: if not NULL, receives the swing quaternion.
 * \param r_twist: if not NULL, receives the twist quaternion.
 * \returns twist angle.
 */
float quat_split_swing_and_twist(const float q_in[4],
                                 int axis,
                                 float r_swing[4],
                                 float r_twist[4]);

float angle_normalized_qt(const float q[4]);
float angle_normalized_qtqt(const float q1[4], const float q2[4]);
float angle_qt(const float q[4]);
float angle_qtqt(const float q1[4], const float q2[4]);

float angle_signed_normalized_qt(const float q[4]);
float angle_signed_normalized_qtqt(const float q1[4], const float q2[4]);
float angle_signed_qt(const float q[4]);
float angle_signed_qtqt(const float q1[4], const float q2[4]);

/**
 * Legacy matrix to quaternion conversion, keep to prevent changes to existing
 * boids & particle-system behavior. Use #mat3_to_quat for new code.
 */
void mat3_to_quat_legacy(float q[4], const float wmat[3][3]);

/* Other. */

/**
 * Utility that performs `sinf` & `cosf` intended for plotting a 2D circle,
 * where the values of the coordinates with are exactly symmetrical although this
 * favors even numbers as odd numbers can only be symmetrical on a single axis.
 *
 * Besides adjustments to precision, this function is the equivalent of:
 * \code {.c}
 * float phi = (2 * M_PI) * (float)i / (float)denominator;
 * *r_sin = sinf(phi);
 * *r_cos = cosf(phi);
 * \endcode
 *
 * \param numerator: An integer factor in [0..denominator] (inclusive).
 * \param denominator: The fraction denominator (typically the number of segments of the circle).
 * \param r_sin: The resulting sine.
 * \param r_cos: The resulting cosine.
 */
void sin_cos_from_fraction(int numerator, int denominator, float *r_sin, float *r_cos);

void print_qt(const char *str, const float q[4]);

#define print_qt_id(q) print_qt(STRINGIFY(q), q)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Axis Angle
 * \{ */

/* Conversion. */

void axis_angle_normalized_to_quat(float r[4], const float axis[3], float angle);
void axis_angle_to_quat(float r[4], const float axis[3], float angle);
/**
 * Axis angle to 3x3 matrix - safer version (normalization of axis performed).
 */
void axis_angle_to_mat3(float R[3][3], const float axis[3], float angle);
/**
 * axis angle to 3x3 matrix
 *
 * This takes the angle with sin/cos applied so we can avoid calculating it in some cases.
 *
 * \param axis: rotation axis (must be normalized).
 * \param angle_sin: sin(angle)
 * \param angle_cos: cos(angle)
 */
void axis_angle_normalized_to_mat3_ex(float mat[3][3],
                                      const float axis[3],
                                      float angle_sin,
                                      float angle_cos);
void axis_angle_normalized_to_mat3(float R[3][3], const float axis[3], float angle);
/**
 * Axis angle to 4x4 matrix - safer version (normalization of axis performed).
 */
void axis_angle_to_mat4(float R[4][4], const float axis[3], float angle);

/**
 * 3x3 matrix to axis angle.
 */
void mat3_normalized_to_axis_angle(float axis[3], float *angle, const float mat[3][3]);
/**
 * 4x4 matrix to axis angle.
 */
void mat4_normalized_to_axis_angle(float axis[3], float *angle, const float mat[4][4]);
void mat3_to_axis_angle(float axis[3], float *angle, const float mat[3][3]);
/**
 * 4x4 matrix to axis angle.
 */
void mat4_to_axis_angle(float axis[3], float *angle, const float mat[4][4]);
/**
 * Quaternions to Axis Angle.
 */
void quat_to_axis_angle(float axis[3], float *angle, const float q[4]);

void angle_to_mat2(float R[2][2], float angle);
/**
 * Create a 3x3 rotation matrix from a single axis.
 */
void axis_angle_to_mat3_single(float R[3][3], char axis, float angle);
/**
 * Create a 4x4 rotation matrix from a single axis.
 */
void axis_angle_to_mat4_single(float R[4][4], char axis, float angle);

void axis_angle_to_quat_single(float q[4], char axis, float angle);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Exponential Map
 * \{ */

void quat_to_expmap(float expmap[3], const float q[4]);
void quat_normalized_to_expmap(float expmap[3], const float q[4]);
void expmap_to_quat(float r[4], const float expmap[3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name XYZ Eulers
 * \{ */

void eul_to_quat(float quat[4], const float eul[3]);
void eul_to_mat3(float mat[3][3], const float eul[3]);
void eul_to_mat4(float mat[4][4], const float eul[3]);

void mat3_normalized_to_eul(float eul[3], const float mat[3][3]);
void mat4_normalized_to_eul(float eul[3], const float m[4][4]);
void mat3_to_eul(float eul[3], const float mat[3][3]);
void mat4_to_eul(float eul[3], const float mat[4][4]);
void quat_to_eul(float eul[3], const float quat[4]);

void mat3_normalized_to_compatible_eul(float eul[3], const float oldrot[3], float mat[3][3]);
void mat3_to_compatible_eul(float eul[3], const float oldrot[3], float mat[3][3]);
void quat_to_compatible_eul(float eul[3], const float oldrot[3], const float quat[4]);
void rotate_eul(float beul[3], char axis, float angle);

/* Order independent. */

void compatible_eul(float eul[3], const float oldrot[3]);

void add_eul_euleul(float r_eul[3], float a[3], float b[3], short order);
void sub_eul_euleul(float r_eul[3], float a[3], float b[3], short order);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Arbitrary Order Eulers
 * \{ */

/* WARNING: must match the #eRotationModes in `DNA_action_types.h`
 * order matters - types are saved to file. */

typedef enum eEulerRotationOrders {
  EULER_ORDER_DEFAULT = 1, /* blender classic = XYZ */
  EULER_ORDER_XYZ = 1,
  EULER_ORDER_XZY,
  EULER_ORDER_YXZ,
  EULER_ORDER_YZX,
  EULER_ORDER_ZXY,
  EULER_ORDER_ZYX,
  /* There are 6 more entries with duplicate entries included. */
} eEulerRotationOrders;

/**
 * Construct quaternion from Euler angles (in radians).
 */
void eulO_to_quat(float q[4], const float e[3], short order);
/**
 * Construct 3x3 matrix from Euler angles (in radians).
 */
void eulO_to_mat3(float M[3][3], const float e[3], short order);
/**
 * Construct 4x4 matrix from Euler angles (in radians).
 */
void eulO_to_mat4(float mat[4][4], const float e[3], short order);
/**
 * Euler Rotation to Axis Angle.
 */
void eulO_to_axis_angle(float axis[3], float *angle, const float eul[3], short order);
/**
 * The matrix is written to as 3 axis vectors.
 */
void eulO_to_gimbal_axis(float gmat[3][3], const float eul[3], short order);

/**
 * Convert 3x3 matrix to Euler angles (in radians).
 */
void mat3_normalized_to_eulO(float eul[3], short order, const float m[3][3]);
/**
 * Convert 4x4 matrix to Euler angles (in radians).
 */
void mat4_normalized_to_eulO(float eul[3], short order, const float m[4][4]);
void mat3_to_eulO(float eul[3], short order, const float m[3][3]);
void mat4_to_eulO(float eul[3], short order, const float m[4][4]);
/**
 * Convert quaternion to Euler angles (in radians).
 */
void quat_to_eulO(float e[3], short order, const float q[4]);
/**
 * Axis Angle to Euler Rotation.
 */
void axis_angle_to_eulO(float eul[3], short order, const float axis[3], float angle);

/* Uses 2 methods to retrieve eulers, and picks the closest. */

void mat3_normalized_to_compatible_eulO(float eul[3],
                                        const float oldrot[3],
                                        short order,
                                        const float mat[3][3]);
void mat4_normalized_to_compatible_eulO(float eul[3],
                                        const float oldrot[3],
                                        short order,
                                        const float mat[4][4]);
void mat3_to_compatible_eulO(float eul[3],
                             const float oldrot[3],
                             short order,
                             const float mat[3][3]);
void mat4_to_compatible_eulO(float eul[3],
                             const float oldrot[3],
                             short order,
                             const float mat[4][4]);
void quat_to_compatible_eulO(float eul[3],
                             const float oldrot[3],
                             short order,
                             const float quat[4]);

void rotate_eulO(float beul[3], short order, char axis, float angle);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dual Quaternions
 * \{ */

void copy_dq_dq(DualQuat *r, const DualQuat *dq);
void normalize_dq(DualQuat *dq, float totweight);
void add_weighted_dq_dq(DualQuat *dq_sum, const DualQuat *dq, float weight);
void mul_v3m3_dq(float r[3], float R[3][3], DualQuat *dq);

void mat4_to_dquat(DualQuat *dq, const float basemat[4][4], const float mat[4][4]);
void dquat_to_mat4(float R[4][4], const DualQuat *dq);

/**
 * Axis matches #eTrackToAxis_Modes.
 */
void quat_apply_track(float quat[4], short axis, short upflag);
void vec_apply_track(float vec[3], short axis);

/**
 * Lens/angle conversion (radians).
 */
float focallength_to_fov(float focal_length, float sensor);
float fov_to_focallength(float hfov, float sensor);

float angle_wrap_rad(float angle);
float angle_wrap_deg(float angle);

/**
 * Returns an angle compatible with angle_compat.
 */
float angle_compat_rad(float angle, float angle_compat);

/**
 * Each argument us an axis in ['X', 'Y', 'Z', '-X', '-Y', '-Z']
 * where the first 2 are a source and the second 2 are the target.
 */
bool mat3_from_axis_conversion(
    int src_forward, int src_up, int dst_forward, int dst_up, float r_mat[3][3]);
/**
 * Use when the second axis can be guessed.
 */
bool mat3_from_axis_conversion_single(int src_axis, int dst_axis, float r_mat[3][3]);

/** \} */

#ifdef __cplusplus
}
#endif
