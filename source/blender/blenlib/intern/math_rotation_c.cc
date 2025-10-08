/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_math_rotation.h"

#include "BLI_math_base_safe.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

/******************************** Quaternions ********************************/

/* used to test is a quat is not normalized (only used for debug prints) */
#ifndef NDEBUG
#  define QUAT_EPSILON 0.0001
#endif

/**
 * The threshold for using a zeroed 3rd (typically Z) value when calculating the euler.
 * NOTE(@ideasman42): A reasonable range for this value is (0.0002 .. 0.00002).
 * This was previously `16 * FLT_EPSILON` however it caused imprecision at times,
 * see examples from: #116880.
 */
#define EULER_HYPOT_EPSILON 0.0000375

void unit_axis_angle(float axis[3], float *angle)
{
  axis[0] = 0.0f;
  axis[1] = 1.0f;
  axis[2] = 0.0f;
  *angle = 0.0f;
}

void unit_qt(float q[4])
{
  q[0] = 1.0f;
  q[1] = q[2] = q[3] = 0.0f;
}

void copy_qt_qt(float q[4], const float a[4])
{
  q[0] = a[0];
  q[1] = a[1];
  q[2] = a[2];
  q[3] = a[3];
}

bool is_zero_qt(const float q[4])
{
  return (q[0] == 0 && q[1] == 0 && q[2] == 0 && q[3] == 0);
}

void mul_qt_qtqt(float q[4], const float a[4], const float b[4])
{
  float t0, t1, t2;

  t0 = a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3];
  t1 = a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2];
  t2 = a[0] * b[2] + a[2] * b[0] + a[3] * b[1] - a[1] * b[3];
  q[3] = a[0] * b[3] + a[3] * b[0] + a[1] * b[2] - a[2] * b[1];
  q[0] = t0;
  q[1] = t1;
  q[2] = t2;
}

void mul_qt_v3(const float q[4], float r[3])
{
  float t0, t1, t2;

  t0 = -q[1] * r[0] - q[2] * r[1] - q[3] * r[2];
  t1 = q[0] * r[0] + q[2] * r[2] - q[3] * r[1];
  t2 = q[0] * r[1] + q[3] * r[0] - q[1] * r[2];
  r[2] = q[0] * r[2] + q[1] * r[1] - q[2] * r[0];
  r[0] = t1;
  r[1] = t2;

  t1 = t0 * -q[1] + r[0] * q[0] - r[1] * q[3] + r[2] * q[2];
  t2 = t0 * -q[2] + r[1] * q[0] - r[2] * q[1] + r[0] * q[3];
  r[2] = t0 * -q[3] + r[2] * q[0] - r[0] * q[2] + r[1] * q[1];
  r[0] = t1;
  r[1] = t2;
}

void conjugate_qt_qt(float q1[4], const float q2[4])
{
  q1[0] = q2[0];
  q1[1] = -q2[1];
  q1[2] = -q2[2];
  q1[3] = -q2[3];
}

void conjugate_qt(float q[4])
{
  q[1] = -q[1];
  q[2] = -q[2];
  q[3] = -q[3];
}

float dot_qtqt(const float a[4], const float b[4])
{
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
}

void invert_qt(float q[4])
{
  const float f = dot_qtqt(q, q);

  if (f == 0.0f) {
    return;
  }

  conjugate_qt(q);
  mul_qt_fl(q, 1.0f / f);
}

void invert_qt_qt(float q1[4], const float q2[4])
{
  copy_qt_qt(q1, q2);
  invert_qt(q1);
}

void invert_qt_normalized(float q[4])
{
  BLI_ASSERT_UNIT_QUAT(q);
  conjugate_qt(q);
}

void invert_qt_qt_normalized(float q1[4], const float q2[4])
{
  copy_qt_qt(q1, q2);
  invert_qt_normalized(q1);
}

void mul_qt_fl(float q[4], const float f)
{
  q[0] *= f;
  q[1] *= f;
  q[2] *= f;
  q[3] *= f;
}

void sub_qt_qtqt(float q[4], const float a[4], const float b[4])
{
  float n_b[4];

  n_b[0] = -b[0];
  n_b[1] = b[1];
  n_b[2] = b[2];
  n_b[3] = b[3];

  mul_qt_qtqt(q, a, n_b);
}

void pow_qt_fl_normalized(float q[4], const float fac)
{
  BLI_ASSERT_UNIT_QUAT(q);
  const float angle = fac * safe_acosf(q[0]); /* quat[0] = cos(0.5 * angle),
                                               * but now the 0.5 and 2.0 rule out */
  const float co = cosf(angle);
  const float si = sinf(angle);
  q[0] = co;
  normalize_v3_length(q + 1, si);
}

void quat_to_compatible_quat(float q[4], const float a[4], const float old[4])
{
  const float eps = 1e-4f;
  BLI_ASSERT_UNIT_QUAT(a);
  float old_unit[4];
  /* Skips `!finite_v4(old)` case too. */
  if (normalize_qt_qt(old_unit, old) > eps) {
    float q_negate[4];
    float delta[4];
    rotation_between_quats_to_quat(delta, old_unit, a);
    mul_qt_qtqt(q, old, delta);
    negate_v4_v4(q_negate, q);
    if (len_squared_v4v4(q_negate, old) < len_squared_v4v4(q, old)) {
      copy_qt_qt(q, q_negate);
    }
  }
  else {
    copy_qt_qt(q, a);
  }
}

/* Skip error check, currently only needed by #mat3_to_quat_legacy. */
static void quat_to_mat3_no_error(float m[3][3], const float q[4])
{
  double q0, q1, q2, q3, qda, qdb, qdc, qaa, qab, qac, qbb, qbc, qcc;

  q0 = M_SQRT2 * double(q[0]);
  q1 = M_SQRT2 * double(q[1]);
  q2 = M_SQRT2 * double(q[2]);
  q3 = M_SQRT2 * double(q[3]);

  qda = q0 * q1;
  qdb = q0 * q2;
  qdc = q0 * q3;
  qaa = q1 * q1;
  qab = q1 * q2;
  qac = q1 * q3;
  qbb = q2 * q2;
  qbc = q2 * q3;
  qcc = q3 * q3;

  m[0][0] = float(1.0 - qbb - qcc);
  m[0][1] = float(qdc + qab);
  m[0][2] = float(-qdb + qac);

  m[1][0] = float(-qdc + qab);
  m[1][1] = float(1.0 - qaa - qcc);
  m[1][2] = float(qda + qbc);

  m[2][0] = float(qdb + qac);
  m[2][1] = float(-qda + qbc);
  m[2][2] = float(1.0 - qaa - qbb);
}

void quat_to_mat3(float m[3][3], const float q[4])
{
#ifndef NDEBUG
  float f;
  if (!((f = dot_qtqt(q, q)) == 0.0f || (fabsf(f - 1.0f) < float(QUAT_EPSILON)))) {
    fprintf(stderr,
            "Warning! quat_to_mat3() called with non-normalized: size %.8f *** report a bug ***\n",
            f);
  }
#endif

  quat_to_mat3_no_error(m, q);
}

void quat_to_mat4(float m[4][4], const float q[4])
{
  double q0, q1, q2, q3, qda, qdb, qdc, qaa, qab, qac, qbb, qbc, qcc;

#ifndef NDEBUG
  if (!((q0 = dot_qtqt(q, q)) == 0.0 || (fabs(q0 - 1.0) < QUAT_EPSILON))) {
    fprintf(stderr,
            "Warning! quat_to_mat4() called with non-normalized: size %.8f *** report a bug ***\n",
            float(q0));
  }
#endif

  q0 = M_SQRT2 * double(q[0]);
  q1 = M_SQRT2 * double(q[1]);
  q2 = M_SQRT2 * double(q[2]);
  q3 = M_SQRT2 * double(q[3]);

  qda = q0 * q1;
  qdb = q0 * q2;
  qdc = q0 * q3;
  qaa = q1 * q1;
  qab = q1 * q2;
  qac = q1 * q3;
  qbb = q2 * q2;
  qbc = q2 * q3;
  qcc = q3 * q3;

  m[0][0] = float(1.0 - qbb - qcc);
  m[0][1] = float(qdc + qab);
  m[0][2] = float(-qdb + qac);
  m[0][3] = 0.0f;

  m[1][0] = float(-qdc + qab);
  m[1][1] = float(1.0 - qaa - qcc);
  m[1][2] = float(qda + qbc);
  m[1][3] = 0.0f;

  m[2][0] = float(qdb + qac);
  m[2][1] = float(-qda + qbc);
  m[2][2] = float(1.0 - qaa - qbb);
  m[2][3] = 0.0f;

  m[3][0] = m[3][1] = m[3][2] = 0.0f;
  m[3][3] = 1.0f;
}

void mat3_normalized_to_quat_fast(float q[4], const float mat[3][3])
{
  BLI_ASSERT_UNIT_M3(mat);
  /* Caller must ensure matrices aren't negative for valid results, see: #24291, #94231. */
  BLI_assert(!is_negative_m3(mat));

  /* Method outlined by Mike Day, ref: https://math.stackexchange.com/a/3183435/220949
   * with an additional `sqrtf(..)` for higher precision result.
   * Removing the `sqrt` causes tests to fail unless the precision is set to 1e-6 or larger. */

  if (mat[2][2] < 0.0f) {
    if (mat[0][0] > mat[1][1]) {
      const float trace = 1.0f + mat[0][0] - mat[1][1] - mat[2][2];
      float s = 2.0f * sqrtf(trace);
      if (mat[1][2] < mat[2][1]) {
        /* Ensure W is non-negative for a canonical result. */
        s = -s;
      }
      q[1] = 0.25f * s;
      s = 1.0f / s;
      q[0] = (mat[1][2] - mat[2][1]) * s;
      q[2] = (mat[0][1] + mat[1][0]) * s;
      q[3] = (mat[2][0] + mat[0][2]) * s;
      if (UNLIKELY((trace == 1.0f) && (q[0] == 0.0f && q[2] == 0.0f && q[3] == 0.0f))) {
        /* Avoids the need to normalize the degenerate case. */
        q[1] = 1.0f;
      }
    }
    else {
      const float trace = 1.0f - mat[0][0] + mat[1][1] - mat[2][2];
      float s = 2.0f * sqrtf(trace);
      if (mat[2][0] < mat[0][2]) {
        /* Ensure W is non-negative for a canonical result. */
        s = -s;
      }
      q[2] = 0.25f * s;
      s = 1.0f / s;
      q[0] = (mat[2][0] - mat[0][2]) * s;
      q[1] = (mat[0][1] + mat[1][0]) * s;
      q[3] = (mat[1][2] + mat[2][1]) * s;
      if (UNLIKELY((trace == 1.0f) && (q[0] == 0.0f && q[1] == 0.0f && q[3] == 0.0f))) {
        /* Avoids the need to normalize the degenerate case. */
        q[2] = 1.0f;
      }
    }
  }
  else {
    if (mat[0][0] < -mat[1][1]) {
      const float trace = 1.0f - mat[0][0] - mat[1][1] + mat[2][2];
      float s = 2.0f * sqrtf(trace);
      if (mat[0][1] < mat[1][0]) {
        /* Ensure W is non-negative for a canonical result. */
        s = -s;
      }
      q[3] = 0.25f * s;
      s = 1.0f / s;
      q[0] = (mat[0][1] - mat[1][0]) * s;
      q[1] = (mat[2][0] + mat[0][2]) * s;
      q[2] = (mat[1][2] + mat[2][1]) * s;
      if (UNLIKELY((trace == 1.0f) && (q[0] == 0.0f && q[1] == 0.0f && q[2] == 0.0f))) {
        /* Avoids the need to normalize the degenerate case. */
        q[3] = 1.0f;
      }
    }
    else {
      /* NOTE(@ideasman42): A zero matrix will fall through to this block,
       * needed so a zero scaled matrices to return a quaternion without rotation, see: #101848. */
      const float trace = 1.0f + mat[0][0] + mat[1][1] + mat[2][2];
      float s = 2.0f * sqrtf(trace);
      q[0] = 0.25f * s;
      s = 1.0f / s;
      q[1] = (mat[1][2] - mat[2][1]) * s;
      q[2] = (mat[2][0] - mat[0][2]) * s;
      q[3] = (mat[0][1] - mat[1][0]) * s;
      if (UNLIKELY((trace == 1.0f) && (q[1] == 0.0f && q[2] == 0.0f && q[3] == 0.0f))) {
        /* Avoids the need to normalize the degenerate case. */
        q[0] = 1.0f;
      }
    }
  }

  BLI_assert(!(q[0] < 0.0f));

  /* Sometimes normalization is necessary due to round-off errors in the above
   * calculations. The comparison here uses tighter tolerances than
   * BLI_ASSERT_UNIT_QUAT(), so it's likely that even after a few more
   * transformations the quaternion will still be considered unit-ish. */
  const float q_len_squared = dot_qtqt(q, q);
  const float threshold = 0.0002f /* #BLI_ASSERT_UNIT_EPSILON */ * 3;
  if (fabs(q_len_squared - 1.0f) >= threshold) {
    normalize_qt(q);
  }
}

static void mat3_normalized_to_quat_with_checks(float q[4], float mat[3][3])
{
  const float det = determinant_m3_array(mat);
  if (UNLIKELY(!isfinite(det))) {
    unit_m3(mat);
  }
  else if (UNLIKELY(det < 0.0f)) {
    negate_m3(mat);
  }
  mat3_normalized_to_quat_fast(q, mat);
}

void mat3_normalized_to_quat(float q[4], const float mat[3][3])
{
  float unit_mat_abs[3][3];
  copy_m3_m3(unit_mat_abs, mat);
  mat3_normalized_to_quat_with_checks(q, unit_mat_abs);
}

void mat3_to_quat(float q[4], const float mat[3][3])
{
  float unit_mat_abs[3][3];
  normalize_m3_m3(unit_mat_abs, mat);
  mat3_normalized_to_quat_with_checks(q, unit_mat_abs);
}

void mat4_normalized_to_quat(float q[4], const float mat[4][4])
{
  float unit_mat_abs[3][3];
  copy_m3_m4(unit_mat_abs, mat);
  mat3_normalized_to_quat_with_checks(q, unit_mat_abs);
}

void mat4_to_quat(float q[4], const float mat[4][4])
{
  float unit_mat_abs[3][3];
  copy_m3_m4(unit_mat_abs, mat);
  normalize_m3(unit_mat_abs);
  mat3_normalized_to_quat_with_checks(q, unit_mat_abs);
}

void mat3_to_quat_legacy(float q[4], const float wmat[3][3])
{
  /* Legacy version of #mat3_to_quat which has slightly different behavior.
   * Keep for particle-system & boids since replacing this will make subtle changes
   * that impact hair in existing files. See: D15772. */

  float mat[3][3], matr[3][3], matn[3][3], q1[4], q2[4], angle, si, co, nor[3];

  /* work on a copy */
  copy_m3_m3(mat, wmat);
  normalize_m3(mat);

  /* rotate z-axis of matrix to z-axis */

  nor[0] = mat[2][1]; /* cross product with (0,0,1) */
  nor[1] = -mat[2][0];
  nor[2] = 0.0;
  normalize_v3(nor);

  co = mat[2][2];
  angle = 0.5f * safe_acosf(co);

  co = cosf(angle);
  si = sinf(angle);
  q1[0] = co;
  q1[1] = -nor[0] * si; /* negative here, but why? */
  q1[2] = -nor[1] * si;
  q1[3] = -nor[2] * si;

  /* rotate back x-axis from mat, using inverse q1 */
  quat_to_mat3_no_error(matr, q1);
  invert_m3_m3(matn, matr);
  mul_m3_v3(matn, mat[0]);

  /* and align x-axes */
  angle = 0.5f * atan2f(mat[0][1], mat[0][0]);

  co = cosf(angle);
  si = sinf(angle);
  q2[0] = co;
  q2[1] = 0.0f;
  q2[2] = 0.0f;
  q2[3] = si;

  mul_qt_qtqt(q, q1, q2);
}

float normalize_qt(float q[4])
{
  const float len = sqrtf(dot_qtqt(q, q));

  if (len != 0.0f) {
    mul_qt_fl(q, 1.0f / len);
  }
  else {
    q[1] = 1.0f;
    q[0] = q[2] = q[3] = 0.0f;
  }

  return len;
}

float normalize_qt_qt(float r[4], const float q[4])
{
  copy_qt_qt(r, q);
  return normalize_qt(r);
}

void rotation_between_vecs_to_mat3(float m[3][3], const float v1[3], const float v2[3])
{
  float axis[3];
  /* avoid calculating the angle */
  float angle_sin;
  float angle_cos;

  BLI_ASSERT_UNIT_V3(v1);
  BLI_ASSERT_UNIT_V3(v2);

  cross_v3_v3v3(axis, v1, v2);

  angle_sin = normalize_v3(axis);
  angle_cos = dot_v3v3(v1, v2);

  if (angle_sin > FLT_EPSILON) {
  axis_calc:
    BLI_ASSERT_UNIT_V3(axis);
    axis_angle_normalized_to_mat3_ex(m, axis, angle_sin, angle_cos);
    BLI_ASSERT_UNIT_M3(m);
  }
  else {
    if (angle_cos > 0.0f) {
      /* Same vectors, zero rotation... */
      unit_m3(m);
    }
    else {
      /* Colinear but opposed vectors, 180 rotation... */
      ortho_v3_v3(axis, v1);
      normalize_v3(axis);
      angle_sin = 0.0f;  /* sin(M_PI) */
      angle_cos = -1.0f; /* cos(M_PI) */
      goto axis_calc;
    }
  }
}

void rotation_between_vecs_to_quat(float q[4], const float v1[3], const float v2[3])
{
  float axis[3];

  cross_v3_v3v3(axis, v1, v2);

  if (normalize_v3(axis) > FLT_EPSILON) {
    float angle;

    angle = angle_normalized_v3v3(v1, v2);

    axis_angle_normalized_to_quat(q, axis, angle);
  }
  else {
    /* degenerate case */

    if (dot_v3v3(v1, v2) > 0.0f) {
      /* Same vectors, zero rotation... */
      unit_qt(q);
    }
    else {
      /* Colinear but opposed vectors, 180 rotation... */
      ortho_v3_v3(axis, v1);
      axis_angle_to_quat(q, axis, float(M_PI));
    }
  }
}

void rotation_between_quats_to_quat(float q[4], const float q1[4], const float q2[4])
{
  float tquat[4];

  conjugate_qt_qt(tquat, q1);

  mul_qt_fl(tquat, 1.0f / dot_qtqt(tquat, tquat));

  mul_qt_qtqt(q, tquat, q2);
}

float quat_split_swing_and_twist(const float q_in[4],
                                 const int axis,
                                 float r_swing[4],
                                 float r_twist[4])
{
  BLI_assert(axis >= 0 && axis <= 2);

  /* The calculation requires a canonical quaternion. */
  float q[4];

  if (q_in[0] < 0) {
    negate_v4_v4(q, q_in);
  }
  else {
    copy_v4_v4(q, q_in);
  }

  /* Half-twist angle can be computed directly. */
  float t = atan2f(q[axis + 1], q[0]);

  if (r_swing || r_twist) {
    float sin_t = sinf(t), cos_t = cosf(t);

    /* Compute swing by multiplying the original quaternion by inverted twist. */
    if (r_swing) {
      float twist_inv[4];

      twist_inv[0] = cos_t;
      zero_v3(twist_inv + 1);
      twist_inv[axis + 1] = -sin_t;

      mul_qt_qtqt(r_swing, q, twist_inv);

      BLI_assert(fabsf(r_swing[axis + 1]) < BLI_ASSERT_UNIT_EPSILON);
    }

    /* Output twist last just in case q overlaps r_twist. */
    if (r_twist) {
      r_twist[0] = cos_t;
      zero_v3(r_twist + 1);
      r_twist[axis + 1] = sin_t;
    }
  }

  return 2.0f * t;
}

/* -------------------------------------------------------------------- */
/** \name Quaternion Angle
 *
 * Unlike the angle between vectors, this does NOT return the shortest angle.
 * See signed functions below for this.
 *
 * \{ */

float angle_normalized_qt(const float q[4])
{
  BLI_ASSERT_UNIT_QUAT(q);
  return 2.0f * safe_acosf(q[0]);
}

float angle_qt(const float q[4])
{
  float tquat[4];

  normalize_qt_qt(tquat, q);

  return angle_normalized_qt(tquat);
}

float angle_normalized_qtqt(const float q1[4], const float q2[4])
{
  float qdelta[4];

  BLI_ASSERT_UNIT_QUAT(q1);
  BLI_ASSERT_UNIT_QUAT(q2);

  rotation_between_quats_to_quat(qdelta, q1, q2);

  return angle_normalized_qt(qdelta);
}

float angle_qtqt(const float q1[4], const float q2[4])
{
  float quat1[4], quat2[4];

  normalize_qt_qt(quat1, q1);
  normalize_qt_qt(quat2, q2);

  return angle_normalized_qtqt(quat1, quat2);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Angle (Signed)
 *
 * Angles with quaternion calculation can exceed 180d,
 * Having signed versions of these functions allows 'fabsf(angle_signed_qtqt(...))'
 * to give us the shortest angle between quaternions.
 * With higher precision than subtracting pi afterwards.
 *
 * \{ */

float angle_signed_normalized_qt(const float q[4])
{
  BLI_ASSERT_UNIT_QUAT(q);
  if (q[0] >= 0.0f) {
    return 2.0f * safe_acosf(q[0]);
  }

  return -2.0f * safe_acosf(-q[0]);
}

float angle_signed_normalized_qtqt(const float q1[4], const float q2[4])
{
  if (dot_qtqt(q1, q2) >= 0.0f) {
    return angle_normalized_qtqt(q1, q2);
  }

  float q2_copy[4];
  negate_v4_v4(q2_copy, q2);
  return -angle_normalized_qtqt(q1, q2_copy);
}

float angle_signed_qt(const float q[4])
{
  float tquat[4];

  normalize_qt_qt(tquat, q);

  return angle_signed_normalized_qt(tquat);
}

float angle_signed_qtqt(const float q1[4], const float q2[4])
{
  if (dot_qtqt(q1, q2) >= 0.0f) {
    return angle_qtqt(q1, q2);
  }

  float q2_copy[4];
  negate_v4_v4(q2_copy, q2);
  return -angle_qtqt(q1, q2_copy);
}

/** \} */

void vec_to_quat(float q[4], const float vec[3], short axis, const short upflag)
{
  const float eps = 1e-4f;
  float nor[3], tvec[3];
  float angle, si, co, len;

  BLI_assert(axis >= 0 && axis <= 5);
  BLI_assert(upflag >= 0 && upflag <= 2);

  /* first set the quat to unit */
  unit_qt(q);

  len = len_v3(vec);

  if (UNLIKELY(len == 0.0f)) {
    return;
  }

  /* rotate to axis */
  if (axis > 2) {
    copy_v3_v3(tvec, vec);
    axis = short(axis - 3);
  }
  else {
    negate_v3_v3(tvec, vec);
  }

  /* nasty! I need a good routine for this...
   * problem is a rotation of an Y axis to the negative Y-axis for example.
   */

  if (axis == 0) { /* x-axis */
    nor[0] = 0.0;
    nor[1] = -tvec[2];
    nor[2] = tvec[1];

    if (fabsf(tvec[1]) + fabsf(tvec[2]) < eps) {
      nor[1] = 1.0f;
    }

    co = tvec[0];
  }
  else if (axis == 1) { /* y-axis */
    nor[0] = tvec[2];
    nor[1] = 0.0;
    nor[2] = -tvec[0];

    if (fabsf(tvec[0]) + fabsf(tvec[2]) < eps) {
      nor[2] = 1.0f;
    }

    co = tvec[1];
  }
  else { /* z-axis */
    nor[0] = -tvec[1];
    nor[1] = tvec[0];
    nor[2] = 0.0;

    if (fabsf(tvec[0]) + fabsf(tvec[1]) < eps) {
      nor[0] = 1.0f;
    }

    co = tvec[2];
  }
  co /= len;

  normalize_v3(nor);

  axis_angle_normalized_to_quat(q, nor, safe_acosf(co));

  if (axis != upflag) {
    float mat[3][3];
    float q2[4];
    const float *fp = mat[2];
    quat_to_mat3(mat, q);

    if (axis == 0) {
      if (upflag == 1) {
        angle = 0.5f * atan2f(fp[2], fp[1]);
      }
      else {
        angle = -0.5f * atan2f(fp[1], fp[2]);
      }
    }
    else if (axis == 1) {
      if (upflag == 0) {
        angle = -0.5f * atan2f(fp[2], fp[0]);
      }
      else {
        angle = 0.5f * atan2f(fp[0], fp[2]);
      }
    }
    else {
      if (upflag == 0) {
        angle = 0.5f * atan2f(-fp[1], -fp[0]);
      }
      else {
        angle = -0.5f * atan2f(-fp[0], -fp[1]);
      }
    }

    co = cosf(angle);
    si = sinf(angle) / len;
    q2[0] = co;
    q2[1] = tvec[0] * si;
    q2[2] = tvec[1] * si;
    q2[3] = tvec[2] * si;

    mul_qt_qtqt(q, q2, q);
  }
}

#if 0

/* A & M Watt, Advanced animation and rendering techniques, 1992 ACM press */
void QuatInterpolW(float *result, float quat1[4], float quat2[4], float t)
{
  float omega, cosom, sinom, sc1, sc2;

  cosom = quat1[0] * quat2[0] + quat1[1] * quat2[1] + quat1[2] * quat2[2] + quat1[3] * quat2[3];

  /* rotate around shortest angle */
  if ((1.0f + cosom) > 0.0001f) {

    if ((1.0f - cosom) > 0.0001f) {
      omega = float(acos(cosom));
      sinom = sinf(omega);
      sc1 = sinf((1.0 - t) * omega) / sinom;
      sc2 = sinf(t * omega) / sinom;
    }
    else {
      sc1 = 1.0f - t;
      sc2 = t;
    }
    result[0] = sc1 * quat1[0] + sc2 * quat2[0];
    result[1] = sc1 * quat1[1] + sc2 * quat2[1];
    result[2] = sc1 * quat1[2] + sc2 * quat2[2];
    result[3] = sc1 * quat1[3] + sc2 * quat2[3];
  }
  else {
    result[0] = quat2[3];
    result[1] = -quat2[2];
    result[2] = quat2[1];
    result[3] = -quat2[0];

    sc1 = sinf((1.0 - t) * M_PI_2);
    sc2 = sinf(t * M_PI_2);

    result[0] = sc1 * quat1[0] + sc2 * result[0];
    result[1] = sc1 * quat1[1] + sc2 * result[1];
    result[2] = sc1 * quat1[2] + sc2 * result[2];
    result[3] = sc1 * quat1[3] + sc2 * result[3];
  }
}
#endif

void interp_dot_slerp(const float t, const float cosom, float r_w[2])
{
  const float eps = 1e-4f;

  BLI_assert(IN_RANGE_INCL(cosom, -1.0001f, 1.0001f));

  /* within [-1..1] range, avoid aligned axis */
  if (LIKELY(fabsf(cosom) < (1.0f - eps))) {
    float omega, sinom;

    omega = acosf(cosom);
    sinom = sinf(omega);
    r_w[0] = sinf((1.0f - t) * omega) / sinom;
    r_w[1] = sinf(t * omega) / sinom;
  }
  else {
    /* fall back to lerp */
    r_w[0] = 1.0f - t;
    r_w[1] = t;
  }
}

void interp_qt_qtqt(float q[4], const float a[4], const float b[4], const float t)
{
  float quat[4], cosom, w[2];

  BLI_ASSERT_UNIT_QUAT(a);
  BLI_ASSERT_UNIT_QUAT(b);

  cosom = dot_qtqt(a, b);

  /* rotate around shortest angle */
  if (cosom < 0.0f) {
    cosom = -cosom;
    negate_v4_v4(quat, a);
  }
  else {
    copy_qt_qt(quat, a);
  }

  interp_dot_slerp(t, cosom, w);

  q[0] = w[0] * quat[0] + w[1] * b[0];
  q[1] = w[0] * quat[1] + w[1] * b[1];
  q[2] = w[0] * quat[2] + w[1] * b[2];
  q[3] = w[0] * quat[3] + w[1] * b[3];
}

void add_qt_qtqt(float q[4], const float a[4], const float b[4], const float t)
{
  q[0] = a[0] + t * b[0];
  q[1] = a[1] + t * b[1];
  q[2] = a[2] + t * b[2];
  q[3] = a[3] + t * b[3];
}

void tri_to_quat_ex(
    float quat[4], const float v1[3], const float v2[3], const float v3[3], const float no_orig[3])
{
  /* imaginary x-axis, y-axis triangle is being rotated */
  float vec[3], q1[4], q2[4], n[3], si, co, angle, mat[3][3], imat[3][3];

  /* move z-axis to face-normal */
#if 0
  normal_tri_v3(vec, v1, v2, v3);
#else
  copy_v3_v3(vec, no_orig);
  (void)v3;
#endif

  n[0] = vec[1];
  n[1] = -vec[0];
  n[2] = 0.0f;
  normalize_v3(n);

  if (n[0] == 0.0f && n[1] == 0.0f) {
    n[0] = 1.0f;
  }

  angle = -0.5f * safe_acosf(vec[2]);
  co = cosf(angle);
  si = sinf(angle);
  q1[0] = co;
  q1[1] = n[0] * si;
  q1[2] = n[1] * si;
  q1[3] = 0.0f;

  /* rotate back line v1-v2 */
  quat_to_mat3(mat, q1);
  invert_m3_m3(imat, mat);
  sub_v3_v3v3(vec, v2, v1);
  mul_m3_v3(imat, vec);

  /* what angle has this line with x-axis? */
  vec[2] = 0.0f;
  normalize_v3(vec);

  angle = 0.5f * atan2f(vec[1], vec[0]);
  co = cosf(angle);
  si = sinf(angle);
  q2[0] = co;
  q2[1] = 0.0f;
  q2[2] = 0.0f;
  q2[3] = si;

  mul_qt_qtqt(quat, q1, q2);
}

float tri_to_quat(float q[4], const float a[3], const float b[3], const float c[3])
{
  float vec[3];
  const float len = normal_tri_v3(vec, a, b, c);

  tri_to_quat_ex(q, a, b, c, vec);
  return len;
}

void sin_cos_from_fraction(int numerator, int denominator, float *r_sin, float *r_cos)
{
  /* By default, creating a circle from an integer: calling #sinf & #cosf on the fraction doesn't
   * create symmetrical values (because floats can't represent Pi exactly).
   * Resolve this when the rotation is calculated from a fraction by mapping the `numerator`
   * to lower values so X/Y values for points around a circle are exactly symmetrical, see #87779.
   *
   * Multiply both the `numerator` and `denominator` by eight to ensure we can divide the circle
   * into 8 octants. For each octant, we then use symmetry and negation to bring the `numerator`
   * closer to the origin where precision is highest.
   *
   * Cases 2, 4, 5 and 7, use the trigonometric identity sin(-x) == -sin(x).
   * Cases 1, 2, 5 and 6, swap the pointers `r_sin` and `r_cos`.
   */
  BLI_assert(0 <= numerator);
  BLI_assert(numerator <= denominator);
  BLI_assert(denominator > 0);

  numerator *= 8;                             /* Multiply numerator the same as denominator. */
  const int octant = numerator / denominator; /* Determine the octant. */
  denominator *= 8;                           /* Ensure denominator is a multiple of eight. */
  float cos_sign = 1.0f;                      /* Either 1.0f or -1.0f. */

  switch (octant) {
    case 0:
      /* Primary octant, nothing to do. */
      break;
    case 1:
    case 2:
      numerator = (denominator / 4) - numerator;
      SWAP(float *, r_sin, r_cos);
      break;
    case 3:
    case 4:
      numerator = (denominator / 2) - numerator;
      cos_sign = -1.0f;
      break;
    case 5:
    case 6:
      numerator = numerator - (denominator * 3 / 4);
      SWAP(float *, r_sin, r_cos);
      cos_sign = -1.0f;
      break;
    case 7:
      numerator = numerator - denominator;
      break;
    default:
      BLI_assert_unreachable();
  }

  BLI_assert(-denominator / 4 <= numerator); /* Numerator may be negative. */
  BLI_assert(numerator <= denominator / 4);
  BLI_assert(ELEM(cos_sign, -1.0f, 1.0f));

  const float angle = float(2.0 * M_PI) * (float(numerator) / float(denominator));
  *r_sin = sinf(angle);
  *r_cos = cosf(angle) * cos_sign;
}

void print_qt(const char *str, const float q[4])
{
  printf("%s: %.3f %.3f %.3f %.3f\n", str, q[0], q[1], q[2], q[3]);
}

/******************************** Axis Angle *********************************/

void axis_angle_normalized_to_quat(float r[4], const float axis[3], const float angle)
{
  const float phi = 0.5f * angle;
  const float si = sinf(phi);
  const float co = cosf(phi);
  BLI_ASSERT_UNIT_V3(axis);
  r[0] = co;
  mul_v3_v3fl(r + 1, axis, si);
}

void axis_angle_to_quat(float r[4], const float axis[3], const float angle)
{
  float nor[3];

  if (LIKELY(normalize_v3_v3(nor, axis) != 0.0f)) {
    axis_angle_normalized_to_quat(r, nor, angle);
  }
  else {
    unit_qt(r);
  }
}

void quat_to_axis_angle(float axis[3], float *angle, const float q[4])
{
  float ha, si;

#ifndef NDEBUG
  if (!((ha = dot_qtqt(q, q)) == 0.0f || (fabsf(ha - 1.0f) < float(QUAT_EPSILON)))) {
    fprintf(stderr,
            "Warning! quat_to_axis_angle() called with non-normalized: size %.8f *** report a bug "
            "***\n",
            ha);
  }
#endif

  /* calculate angle/2, and sin(angle/2) */
  ha = acosf(q[0]);
  si = sinf(ha);

  /* from half-angle to angle */
  *angle = ha * 2;

  /* prevent division by zero for axis conversion */
  if (fabsf(si) < 0.0005f) {
    si = 1.0f;
  }

  axis[0] = q[1] / si;
  axis[1] = q[2] / si;
  axis[2] = q[3] / si;
  if (is_zero_v3(axis)) {
    axis[1] = 1.0f;
  }
}

void axis_angle_to_eulO(float eul[3], const short order, const float axis[3], const float angle)
{
  float q[4];

  /* use quaternions as intermediate representation for now... */
  axis_angle_to_quat(q, axis, angle);
  quat_to_eulO(eul, order, q);
}

void eulO_to_axis_angle(float axis[3], float *angle, const float eul[3], const short order)
{
  float q[4];

  /* use quaternions as intermediate representation for now... */
  eulO_to_quat(q, eul, order);
  quat_to_axis_angle(axis, angle, q);
}

void axis_angle_normalized_to_mat3_ex(float mat[3][3],
                                      const float axis[3],
                                      const float angle_sin,
                                      const float angle_cos)
{
  float nsi[3], ico;
  float n_00, n_01, n_11, n_02, n_12, n_22;

  BLI_ASSERT_UNIT_V3(axis);

  /* now convert this to a 3x3 matrix */

  ico = (1.0f - angle_cos);
  nsi[0] = axis[0] * angle_sin;
  nsi[1] = axis[1] * angle_sin;
  nsi[2] = axis[2] * angle_sin;

  n_00 = (axis[0] * axis[0]) * ico;
  n_01 = (axis[0] * axis[1]) * ico;
  n_11 = (axis[1] * axis[1]) * ico;
  n_02 = (axis[0] * axis[2]) * ico;
  n_12 = (axis[1] * axis[2]) * ico;
  n_22 = (axis[2] * axis[2]) * ico;

  mat[0][0] = n_00 + angle_cos;
  mat[0][1] = n_01 + nsi[2];
  mat[0][2] = n_02 - nsi[1];
  mat[1][0] = n_01 - nsi[2];
  mat[1][1] = n_11 + angle_cos;
  mat[1][2] = n_12 + nsi[0];
  mat[2][0] = n_02 + nsi[1];
  mat[2][1] = n_12 - nsi[0];
  mat[2][2] = n_22 + angle_cos;
}

void axis_angle_normalized_to_mat3(float R[3][3], const float axis[3], const float angle)
{
  axis_angle_normalized_to_mat3_ex(R, axis, sinf(angle), cosf(angle));
}

void axis_angle_to_mat3(float R[3][3], const float axis[3], const float angle)
{
  float nor[3];

  /* normalize the axis first (to remove unwanted scaling) */
  if (normalize_v3_v3(nor, axis) == 0.0f) {
    unit_m3(R);
    return;
  }

  axis_angle_normalized_to_mat3(R, nor, angle);
}

void axis_angle_to_mat4(float R[4][4], const float axis[3], const float angle)
{
  float tmat[3][3];

  axis_angle_to_mat3(tmat, axis, angle);
  unit_m4(R);
  copy_m4_m3(R, tmat);
}

void mat3_normalized_to_axis_angle(float axis[3], float *angle, const float mat[3][3])
{
  float q[4];

  /* use quaternions as intermediate representation */
  /* TODO: it would be nicer to go straight there... */
  mat3_normalized_to_quat(q, mat);
  quat_to_axis_angle(axis, angle, q);
}
void mat3_to_axis_angle(float axis[3], float *angle, const float mat[3][3])
{
  float q[4];

  /* use quaternions as intermediate representation */
  /* TODO: it would be nicer to go straight there... */
  mat3_to_quat(q, mat);
  quat_to_axis_angle(axis, angle, q);
}

void mat4_normalized_to_axis_angle(float axis[3], float *angle, const float mat[4][4])
{
  float q[4];

  /* use quaternions as intermediate representation */
  /* TODO: it would be nicer to go straight there... */
  mat4_normalized_to_quat(q, mat);
  quat_to_axis_angle(axis, angle, q);
}

void mat4_to_axis_angle(float axis[3], float *angle, const float mat[4][4])
{
  float q[4];

  /* use quaternions as intermediate representation */
  /* TODO: it would be nicer to go straight there... */
  mat4_to_quat(q, mat);
  quat_to_axis_angle(axis, angle, q);
}

void axis_angle_to_mat4_single(float R[4][4], const char axis, const float angle)
{
  float mat3[3][3];
  axis_angle_to_mat3_single(mat3, axis, angle);
  copy_m4_m3(R, mat3);
}

void axis_angle_to_mat3_single(float R[3][3], const char axis, const float angle)
{
  const float angle_cos = cosf(angle);
  const float angle_sin = sinf(angle);

  switch (axis) {
    case 'X': /* rotation around X */
      R[0][0] = 1.0f;
      R[0][1] = 0.0f;
      R[0][2] = 0.0f;
      R[1][0] = 0.0f;
      R[1][1] = angle_cos;
      R[1][2] = angle_sin;
      R[2][0] = 0.0f;
      R[2][1] = -angle_sin;
      R[2][2] = angle_cos;
      break;
    case 'Y': /* rotation around Y */
      R[0][0] = angle_cos;
      R[0][1] = 0.0f;
      R[0][2] = -angle_sin;
      R[1][0] = 0.0f;
      R[1][1] = 1.0f;
      R[1][2] = 0.0f;
      R[2][0] = angle_sin;
      R[2][1] = 0.0f;
      R[2][2] = angle_cos;
      break;
    case 'Z': /* rotation around Z */
      R[0][0] = angle_cos;
      R[0][1] = angle_sin;
      R[0][2] = 0.0f;
      R[1][0] = -angle_sin;
      R[1][1] = angle_cos;
      R[1][2] = 0.0f;
      R[2][0] = 0.0f;
      R[2][1] = 0.0f;
      R[2][2] = 1.0f;
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
}

void angle_to_mat2(float R[2][2], const float angle)
{
  const float angle_cos = cosf(angle);
  const float angle_sin = sinf(angle);

  /* 2D rotation matrix */
  R[0][0] = angle_cos;
  R[0][1] = angle_sin;
  R[1][0] = -angle_sin;
  R[1][1] = angle_cos;
}

void axis_angle_to_quat_single(float q[4], const char axis, const float angle)
{
  const float angle_half = angle * 0.5f;
  const float angle_cos = cosf(angle_half);
  const float angle_sin = sinf(angle_half);
  const int axis_index = (axis - 'X');

  BLI_assert(axis >= 'X' && axis <= 'Z');

  q[0] = angle_cos;
  zero_v3(q + 1);
  q[axis_index + 1] = angle_sin;
}

/****************************** Exponential Map ******************************/

void quat_normalized_to_expmap(float expmap[3], const float q[4])
{
  float angle;
  BLI_ASSERT_UNIT_QUAT(q);

  /* Obtain axis/angle representation. */
  quat_to_axis_angle(expmap, &angle, q);

  /* Convert to exponential map. */
  mul_v3_fl(expmap, angle);
}

void quat_to_expmap(float expmap[3], const float q[4])
{
  float q_no[4];
  normalize_qt_qt(q_no, q);
  quat_normalized_to_expmap(expmap, q_no);
}

void expmap_to_quat(float r[4], const float expmap[3])
{
  float axis[3];
  float angle;

  /* Obtain axis/angle representation. */
  if (LIKELY((angle = normalize_v3_v3(axis, expmap)) != 0.0f)) {
    axis_angle_normalized_to_quat(r, axis, angle_wrap_rad(angle));
  }
  else {
    unit_qt(r);
  }
}

/******************************** XYZ Eulers *********************************/

void eul_to_mat3(float mat[3][3], const float eul[3])
{
  double ci, cj, ch, si, sj, sh, cc, cs, sc, ss;

  ci = cos(eul[0]);
  cj = cos(eul[1]);
  ch = cos(eul[2]);
  si = sin(eul[0]);
  sj = sin(eul[1]);
  sh = sin(eul[2]);
  cc = ci * ch;
  cs = ci * sh;
  sc = si * ch;
  ss = si * sh;

  mat[0][0] = float(cj * ch);
  mat[1][0] = float(sj * sc - cs);
  mat[2][0] = float(sj * cc + ss);
  mat[0][1] = float(cj * sh);
  mat[1][1] = float(sj * ss + cc);
  mat[2][1] = float(sj * cs - sc);
  mat[0][2] = float(-sj);
  mat[1][2] = float(cj * si);
  mat[2][2] = float(cj * ci);
}

void eul_to_mat4(float mat[4][4], const float eul[3])
{
  double ci, cj, ch, si, sj, sh, cc, cs, sc, ss;

  ci = cos(eul[0]);
  cj = cos(eul[1]);
  ch = cos(eul[2]);
  si = sin(eul[0]);
  sj = sin(eul[1]);
  sh = sin(eul[2]);
  cc = ci * ch;
  cs = ci * sh;
  sc = si * ch;
  ss = si * sh;

  mat[0][0] = float(cj * ch);
  mat[1][0] = float(sj * sc - cs);
  mat[2][0] = float(sj * cc + ss);
  mat[0][1] = float(cj * sh);
  mat[1][1] = float(sj * ss + cc);
  mat[2][1] = float(sj * cs - sc);
  mat[0][2] = float(-sj);
  mat[1][2] = float(cj * si);
  mat[2][2] = float(cj * ci);

  mat[3][0] = mat[3][1] = mat[3][2] = mat[0][3] = mat[1][3] = mat[2][3] = 0.0f;
  mat[3][3] = 1.0f;
}

/* returns two euler calculation methods, so we can pick the best */

/* XYZ order */
static void mat3_normalized_to_eul2(const float mat[3][3], float eul1[3], float eul2[3])
{
  const float cy = hypotf(mat[0][0], mat[0][1]);

  BLI_ASSERT_UNIT_M3(mat);

  if (cy > float(EULER_HYPOT_EPSILON)) {
    eul1[0] = atan2f(mat[1][2], mat[2][2]);
    eul1[1] = atan2f(-mat[0][2], cy);
    eul1[2] = atan2f(mat[0][1], mat[0][0]);

    eul2[0] = atan2f(-mat[1][2], -mat[2][2]);
    eul2[1] = atan2f(-mat[0][2], -cy);
    eul2[2] = atan2f(-mat[0][1], -mat[0][0]);
  }
  else {
    eul1[0] = atan2f(-mat[2][1], mat[1][1]);
    eul1[1] = atan2f(-mat[0][2], cy);
    eul1[2] = 0.0f;

    copy_v3_v3(eul2, eul1);
  }
}

void mat3_normalized_to_eul(float eul[3], const float mat[3][3])
{
  float eul1[3], eul2[3];

  mat3_normalized_to_eul2(mat, eul1, eul2);

  /* return best, which is just the one with lowest values it in */
  if (fabsf(eul1[0]) + fabsf(eul1[1]) + fabsf(eul1[2]) >
      fabsf(eul2[0]) + fabsf(eul2[1]) + fabsf(eul2[2]))
  {
    copy_v3_v3(eul, eul2);
  }
  else {
    copy_v3_v3(eul, eul1);
  }
}
void mat3_to_eul(float eul[3], const float mat[3][3])
{
  float unit_mat[3][3];
  normalize_m3_m3(unit_mat, mat);
  mat3_normalized_to_eul(eul, unit_mat);
}

void mat4_normalized_to_eul(float eul[3], const float m[4][4])
{
  float mat3[3][3];
  copy_m3_m4(mat3, m);
  mat3_normalized_to_eul(eul, mat3);
}
void mat4_to_eul(float eul[3], const float mat[4][4])
{
  float mat3[3][3];
  copy_m3_m4(mat3, mat);
  mat3_to_eul(eul, mat3);
}

void quat_to_eul(float eul[3], const float quat[4])
{
  float unit_mat[3][3];
  quat_to_mat3(unit_mat, quat);
  mat3_normalized_to_eul(eul, unit_mat);
}

void eul_to_quat(float quat[4], const float eul[3])
{
  float ti, tj, th, ci, cj, ch, si, sj, sh, cc, cs, sc, ss;

  ti = eul[0] * 0.5f;
  tj = eul[1] * 0.5f;
  th = eul[2] * 0.5f;
  ci = cosf(ti);
  cj = cosf(tj);
  ch = cosf(th);
  si = sinf(ti);
  sj = sinf(tj);
  sh = sinf(th);
  cc = ci * ch;
  cs = ci * sh;
  sc = si * ch;
  ss = si * sh;

  quat[0] = cj * cc + sj * ss;
  quat[1] = cj * sc - sj * cs;
  quat[2] = cj * ss + sj * cc;
  quat[3] = cj * cs - sj * sc;
}

void rotate_eul(float beul[3], const char axis, const float angle)
{
  float eul[3], mat1[3][3], mat2[3][3], totmat[3][3];

  BLI_assert(axis >= 'X' && axis <= 'Z');

  eul[0] = eul[1] = eul[2] = 0.0f;
  if (axis == 'X') {
    eul[0] = angle;
  }
  else if (axis == 'Y') {
    eul[1] = angle;
  }
  else {
    eul[2] = angle;
  }

  eul_to_mat3(mat1, eul);
  eul_to_mat3(mat2, beul);

  mul_m3_m3m3(totmat, mat2, mat1);

  mat3_to_eul(beul, totmat);
}

void compatible_eul(float eul[3], const float oldrot[3])
{
  /* When the rotation exceeds 180 degrees, it can be wrapped by 360 degrees
   * to produce a closer match.
   * NOTE: Values between `pi` & `2 * pi` work, where `pi` has the lowest number of
   * discontinuities and values approaching `2 * pi` center the resulting rotation around zero,
   * at the expense of the result being less compatible, see !104856. */
  const float pi_thresh = float(M_PI);
  const float pi_x2 = (2.0f * float(M_PI));

  float deul[3];
  uint i;

  /* Correct differences around 360 degrees first. */
  for (i = 0; i < 3; i++) {
    deul[i] = eul[i] - oldrot[i];
    if (deul[i] > pi_thresh) {
      eul[i] -= floorf((deul[i] / pi_x2) + 0.5f) * pi_x2;
      deul[i] = eul[i] - oldrot[i];
    }
    else if (deul[i] < -pi_thresh) {
      eul[i] += floorf((-deul[i] / pi_x2) + 0.5f) * pi_x2;
      deul[i] = eul[i] - oldrot[i];
    }
  }

  uint j = 1, k = 2;
  for (i = 0; i < 3; j = k, k = i++) {
    /* Check if this axis of rotations larger than 180 degrees and
     * the others are smaller than 90 degrees. */
    if (fabsf(deul[i]) > M_PI && fabsf(deul[j]) < M_PI_2 && fabsf(deul[k]) < M_PI_2) {
      if (deul[i] > 0.0f) {
        eul[i] -= pi_x2;
      }
      else {
        eul[i] += pi_x2;
      }
    }
  }
}

/* uses 2 methods to retrieve eulers, and picks the closest */

void mat3_normalized_to_compatible_eul(float eul[3], const float oldrot[3], float mat[3][3])
{
  float eul1[3], eul2[3];
  float d1, d2;

  mat3_normalized_to_eul2(mat, eul1, eul2);

  compatible_eul(eul1, oldrot);
  compatible_eul(eul2, oldrot);

  d1 = fabsf(eul1[0] - oldrot[0]) + fabsf(eul1[1] - oldrot[1]) + fabsf(eul1[2] - oldrot[2]);
  d2 = fabsf(eul2[0] - oldrot[0]) + fabsf(eul2[1] - oldrot[1]) + fabsf(eul2[2] - oldrot[2]);

  /* return best, which is just the one with lowest difference */
  if (d1 > d2) {
    copy_v3_v3(eul, eul2);
  }
  else {
    copy_v3_v3(eul, eul1);
  }
}
void mat3_to_compatible_eul(float eul[3], const float oldrot[3], float mat[3][3])
{
  float unit_mat[3][3];
  normalize_m3_m3(unit_mat, mat);
  mat3_normalized_to_compatible_eul(eul, oldrot, unit_mat);
}

void quat_to_compatible_eul(float eul[3], const float oldrot[3], const float quat[4])
{
  float unit_mat[3][3];
  quat_to_mat3(unit_mat, quat);
  mat3_normalized_to_compatible_eul(eul, oldrot, unit_mat);
}

/************************** Arbitrary Order Eulers ***************************/

/* Euler Rotation Order Code:
 * was adapted from
 *      ANSI C code from the article
 *      "Euler Angle Conversion"
 *      by Ken Shoemake <shoemake@graphics.cis.upenn.edu>
 *      in "Graphics Gems IV", Academic Press, 1994
 * for use in Blender
 */

/* Type for rotation order info - see wiki for derivation details */
struct RotOrderInfo {
  short axis[3];
  short parity; /* parity of axis permutation (even=0, odd=1) - 'n' in original code */
};

/* Array of info for Rotation Order calculations
 * WARNING: must be kept in same order as eEulerRotationOrders
 */
static const RotOrderInfo rotOrders[] = {
    /* i, j, k, n */
    {{0, 1, 2}, 0}, /* XYZ */
    {{0, 2, 1}, 1}, /* XZY */
    {{1, 0, 2}, 1}, /* YXZ */
    {{1, 2, 0}, 0}, /* YZX */
    {{2, 0, 1}, 0}, /* ZXY */
    {{2, 1, 0}, 1}  /* ZYX */
};

/* Get relevant pointer to rotation order set from the array
 * NOTE: since we start at 1 for the values, but arrays index from 0,
 *       there is -1 factor involved in this process...
 */
static const RotOrderInfo *get_rotation_order_info(const short order)
{
  BLI_assert(order >= 0 && order <= 6);
  if (order < 1) {
    return &rotOrders[0];
  }
  if (order < 6) {
    return &rotOrders[order - 1];
  }

  return &rotOrders[5];
}

void eulO_to_quat(float q[4], const float e[3], const short order)
{
  const RotOrderInfo *R = get_rotation_order_info(order);
  short i = R->axis[0], j = R->axis[1], k = R->axis[2];
  double ti, tj, th, ci, cj, ch, si, sj, sh, cc, cs, sc, ss;
  double a[3];

  ti = double(e[i]) * 0.5;
  tj = double(e[j]) * (R->parity ? -0.5 : 0.5);
  th = double(e[k]) * 0.5;

  ci = cos(ti);
  cj = cos(tj);
  ch = cos(th);
  si = sin(ti);
  sj = sin(tj);
  sh = sin(th);

  cc = ci * ch;
  cs = ci * sh;
  sc = si * ch;
  ss = si * sh;

  a[i] = cj * sc - sj * cs;
  a[j] = cj * ss + sj * cc;
  a[k] = cj * cs - sj * sc;

  q[0] = float(cj * cc + sj * ss);
  q[1] = float(a[0]);
  q[2] = float(a[1]);
  q[3] = float(a[2]);

  if (R->parity) {
    q[j + 1] = -q[j + 1];
  }
}

void quat_to_eulO(float e[3], short const order, const float q[4])
{
  float unit_mat[3][3];

  quat_to_mat3(unit_mat, q);
  mat3_normalized_to_eulO(e, order, unit_mat);
}

void eulO_to_mat3(float M[3][3], const float e[3], const short order)
{
  const RotOrderInfo *R = get_rotation_order_info(order);
  short i = R->axis[0], j = R->axis[1], k = R->axis[2];
  double ti, tj, th, ci, cj, ch, si, sj, sh, cc, cs, sc, ss;

  if (R->parity) {
    ti = -e[i];
    tj = -e[j];
    th = -e[k];
  }
  else {
    ti = e[i];
    tj = e[j];
    th = e[k];
  }

  ci = cos(ti);
  cj = cos(tj);
  ch = cos(th);
  si = sin(ti);
  sj = sin(tj);
  sh = sin(th);

  cc = ci * ch;
  cs = ci * sh;
  sc = si * ch;
  ss = si * sh;

  M[i][i] = float(cj * ch);
  M[j][i] = float(sj * sc - cs);
  M[k][i] = float(sj * cc + ss);
  M[i][j] = float(cj * sh);
  M[j][j] = float(sj * ss + cc);
  M[k][j] = float(sj * cs - sc);
  M[i][k] = float(-sj);
  M[j][k] = float(cj * si);
  M[k][k] = float(cj * ci);
}

/* returns two euler calculation methods, so we can pick the best */
static void mat3_normalized_to_eulo2(const float mat[3][3],
                                     float eul1[3],
                                     float eul2[3],
                                     const short order)
{
  const RotOrderInfo *R = get_rotation_order_info(order);
  short i = R->axis[0], j = R->axis[1], k = R->axis[2];
  float cy;

  BLI_ASSERT_UNIT_M3(mat);

  cy = hypotf(mat[i][i], mat[i][j]);

  if (cy > float(EULER_HYPOT_EPSILON)) {
    eul1[i] = atan2f(mat[j][k], mat[k][k]);
    eul1[j] = atan2f(-mat[i][k], cy);
    eul1[k] = atan2f(mat[i][j], mat[i][i]);

    eul2[i] = atan2f(-mat[j][k], -mat[k][k]);
    eul2[j] = atan2f(-mat[i][k], -cy);
    eul2[k] = atan2f(-mat[i][j], -mat[i][i]);
  }
  else {
    eul1[i] = atan2f(-mat[k][j], mat[j][j]);
    eul1[j] = atan2f(-mat[i][k], cy);
    eul1[k] = 0;

    copy_v3_v3(eul2, eul1);
  }

  if (R->parity) {
    negate_v3(eul1);
    negate_v3(eul2);
  }
}

void eulO_to_mat4(float mat[4][4], const float e[3], const short order)
{
  float unit_mat[3][3];

  /* for now, we'll just do this the slow way (i.e. copying matrices) */
  eulO_to_mat3(unit_mat, e, order);
  copy_m4_m3(mat, unit_mat);
}

void mat3_normalized_to_eulO(float eul[3], const short order, const float m[3][3])
{
  float eul1[3], eul2[3];
  float d1, d2;

  mat3_normalized_to_eulo2(m, eul1, eul2, order);

  d1 = fabsf(eul1[0]) + fabsf(eul1[1]) + fabsf(eul1[2]);
  d2 = fabsf(eul2[0]) + fabsf(eul2[1]) + fabsf(eul2[2]);

  /* return best, which is just the one with lowest values it in */
  if (d1 > d2) {
    copy_v3_v3(eul, eul2);
  }
  else {
    copy_v3_v3(eul, eul1);
  }
}
void mat3_to_eulO(float eul[3], const short order, const float m[3][3])
{
  float unit_mat[3][3];
  normalize_m3_m3(unit_mat, m);
  mat3_normalized_to_eulO(eul, order, unit_mat);
}

void mat4_normalized_to_eulO(float eul[3], const short order, const float m[4][4])
{
  float mat3[3][3];

  /* for now, we'll just do this the slow way (i.e. copying matrices) */
  copy_m3_m4(mat3, m);
  mat3_normalized_to_eulO(eul, order, mat3);
}

void mat4_to_eulO(float eul[3], const short order, const float m[4][4])
{
  float mat3[3][3];
  copy_m3_m4(mat3, m);
  normalize_m3(mat3);
  mat3_normalized_to_eulO(eul, order, mat3);
}

void mat3_normalized_to_compatible_eulO(float eul[3],
                                        const float oldrot[3],
                                        const short order,
                                        const float mat[3][3])
{
  float eul1[3], eul2[3];
  float d1, d2;

  mat3_normalized_to_eulo2(mat, eul1, eul2, order);

  compatible_eul(eul1, oldrot);
  compatible_eul(eul2, oldrot);

  d1 = fabsf(eul1[0] - oldrot[0]) + fabsf(eul1[1] - oldrot[1]) + fabsf(eul1[2] - oldrot[2]);
  d2 = fabsf(eul2[0] - oldrot[0]) + fabsf(eul2[1] - oldrot[1]) + fabsf(eul2[2] - oldrot[2]);

  /* return best, which is just the one with lowest difference */
  if (d1 > d2) {
    copy_v3_v3(eul, eul2);
  }
  else {
    copy_v3_v3(eul, eul1);
  }
}
void mat3_to_compatible_eulO(float eul[3],
                             const float oldrot[3],
                             const short order,
                             const float mat[3][3])
{
  float unit_mat[3][3];

  normalize_m3_m3(unit_mat, mat);
  mat3_normalized_to_compatible_eulO(eul, oldrot, order, unit_mat);
}

void mat4_normalized_to_compatible_eulO(float eul[3],
                                        const float oldrot[3],
                                        const short order,
                                        const float mat[4][4])
{
  float mat3[3][3];

  /* for now, we'll just do this the slow way (i.e. copying matrices) */
  copy_m3_m4(mat3, mat);
  mat3_normalized_to_compatible_eulO(eul, oldrot, order, mat3);
}
void mat4_to_compatible_eulO(float eul[3],
                             const float oldrot[3],
                             const short order,
                             const float mat[4][4])
{
  float mat3[3][3];

  /* for now, we'll just do this the slow way (i.e. copying matrices) */
  copy_m3_m4(mat3, mat);
  normalize_m3(mat3);
  mat3_normalized_to_compatible_eulO(eul, oldrot, order, mat3);
}

void quat_to_compatible_eulO(float eul[3],
                             const float oldrot[3],
                             const short order,
                             const float quat[4])
{
  float unit_mat[3][3];

  quat_to_mat3(unit_mat, quat);
  mat3_normalized_to_compatible_eulO(eul, oldrot, order, unit_mat);
}

/* rotate the given euler by the given angle on the specified axis */
/* NOTE: is this safe to do with different axis orders? */

void rotate_eulO(float beul[3], const short order, const char axis, const float angle)
{
  float eul[3], mat1[3][3], mat2[3][3], totmat[3][3];

  BLI_assert(axis >= 'X' && axis <= 'Z');

  zero_v3(eul);

  if (axis == 'X') {
    eul[0] = angle;
  }
  else if (axis == 'Y') {
    eul[1] = angle;
  }
  else {
    eul[2] = angle;
  }

  eulO_to_mat3(mat1, eul, order);
  eulO_to_mat3(mat2, beul, order);

  mul_m3_m3m3(totmat, mat2, mat1);

  mat3_to_eulO(beul, order, totmat);
}

void eulO_to_gimbal_axis(float gmat[3][3], const float eul[3], const short order)
{
  const RotOrderInfo *R = get_rotation_order_info(order);

  float mat[3][3];
  float teul[3];

  /* first axis is local */
  eulO_to_mat3(mat, eul, order);
  copy_v3_v3(gmat[R->axis[0]], mat[R->axis[0]]);

  /* second axis is local minus first rotation */
  copy_v3_v3(teul, eul);
  teul[R->axis[0]] = 0;
  eulO_to_mat3(mat, teul, order);
  copy_v3_v3(gmat[R->axis[1]], mat[R->axis[1]]);

  /* Last axis is global */
  zero_v3(gmat[R->axis[2]]);
  gmat[R->axis[2]][R->axis[2]] = 1;
}

void add_eul_euleul(float r_eul[3], float a[3], float b[3], const short order)
{
  float quat[4], quat_b[4];

  eulO_to_quat(quat, a, order);
  eulO_to_quat(quat_b, b, order);

  mul_qt_qtqt(quat, quat_b, quat);

  quat_to_eulO(r_eul, order, quat);
}

void sub_eul_euleul(float r_eul[3], float a[3], float b[3], const short order)
{
  float quat[4], quat_b[4];

  eulO_to_quat(quat, a, order);
  eulO_to_quat(quat_b, b, order);

  invert_qt_normalized(quat_b);
  mul_qt_qtqt(quat, quat_b, quat);

  quat_to_eulO(r_eul, order, quat);
}

/******************************* Dual Quaternions ****************************/

/* Conversion routines between (regular quaternion, translation) and dual quaternion.
 *
 * Version 1.0.0, February 7th, 2007
 *
 * SPDX-License-Identifier: Zlib
 * Copyright 2006-2007 University of Dublin, Trinity College, All Rights Reserved.
 *
 * Changes for Blender:
 * - renaming, style changes and optimization's
 * - added support for scaling
 */

void mat4_to_dquat(DualQuat *dq, const float basemat[4][4], const float mat[4][4])
{
  float dscale[3], scale[3], basequat[4], mat3[3][3];
  float baseRS[4][4], baseinv[4][4], baseR[4][4], baseRinv[4][4];
  float R[4][4], S[4][4];

  /* split scaling and rotation, there is probably a faster way to do
   * this, it's done like this now to correctly get negative scaling */
  mul_m4_m4m4(baseRS, mat, basemat);
  mat4_to_size(scale, baseRS);

  dscale[0] = scale[0] - 1.0f;
  dscale[1] = scale[1] - 1.0f;
  dscale[2] = scale[2] - 1.0f;

  copy_m3_m4(mat3, mat);

  if (!is_orthonormal_m3(mat3) || (determinant_m4(mat) < 0.0f) ||
      len_squared_v3(dscale) > square_f(1e-4f))
  {
    /* Extract R and S. */
    float tmp[4][4];

    /* extra orthogonalize, to avoid flipping with stretched bones */
    copy_m4_m4(tmp, baseRS);
    orthogonalize_m4(tmp, 1);
    mat4_to_quat(basequat, tmp);

    quat_to_mat4(baseR, basequat);
    copy_v3_v3(baseR[3], baseRS[3]);

    invert_m4_m4(baseinv, basemat);
    mul_m4_m4m4(R, baseR, baseinv);

    invert_m4_m4(baseRinv, baseR);
    mul_m4_m4m4(S, baseRinv, baseRS);

    /* set scaling part */
    mul_m4_series(dq->scale, basemat, S, baseinv);
    dq->scale_weight = 1.0f;
  }
  else {
    /* matrix does not contain scaling */
    copy_m4_m4(R, mat);
    dq->scale_weight = 0.0f;
  }

  /* non-dual part */
  mat4_to_quat(dq->quat, R);

  /* dual part */
  const float *t = R[3];
  const float *q = dq->quat;
  dq->trans[0] = -0.5f * (t[0] * q[1] + t[1] * q[2] + t[2] * q[3]);
  dq->trans[1] = 0.5f * (t[0] * q[0] + t[1] * q[3] - t[2] * q[2]);
  dq->trans[2] = 0.5f * (-t[0] * q[3] + t[1] * q[0] + t[2] * q[1]);
  dq->trans[3] = 0.5f * (t[0] * q[2] - t[1] * q[1] + t[2] * q[0]);
}

void dquat_to_mat4(float R[4][4], const DualQuat *dq)
{
  float len, q0[4];
  const float *t;

  /* regular quaternion */
  copy_qt_qt(q0, dq->quat);

  /* normalize */
  len = sqrtf(dot_qtqt(q0, q0));
  if (len != 0.0f) {
    len = 1.0f / len;
  }
  mul_qt_fl(q0, len);

  /* rotation */
  quat_to_mat4(R, q0);

  /* translation */
  t = dq->trans;
  R[3][0] = 2.0f * (-t[0] * q0[1] + t[1] * q0[0] - t[2] * q0[3] + t[3] * q0[2]) * len;
  R[3][1] = 2.0f * (-t[0] * q0[2] + t[1] * q0[3] + t[2] * q0[0] - t[3] * q0[1]) * len;
  R[3][2] = 2.0f * (-t[0] * q0[3] - t[1] * q0[2] + t[2] * q0[1] + t[3] * q0[0]) * len;

  /* scaling */
  if (dq->scale_weight) {
    mul_m4_m4m4(R, R, dq->scale);
  }
}

void add_weighted_dq_dq(DualQuat *dq_sum, const DualQuat *dq, float weight)
{
  bool flipped = false;

  /* make sure we interpolate quats in the right direction */
  if (dot_qtqt(dq->quat, dq_sum->quat) < 0) {
    flipped = true;
    weight = -weight;
  }

  /* interpolate rotation and translation */
  dq_sum->quat[0] += weight * dq->quat[0];
  dq_sum->quat[1] += weight * dq->quat[1];
  dq_sum->quat[2] += weight * dq->quat[2];
  dq_sum->quat[3] += weight * dq->quat[3];

  dq_sum->trans[0] += weight * dq->trans[0];
  dq_sum->trans[1] += weight * dq->trans[1];
  dq_sum->trans[2] += weight * dq->trans[2];
  dq_sum->trans[3] += weight * dq->trans[3];

  /* Interpolate scale - but only if there is scale present. If any dual
   * quaternions without scale are added, they will be compensated for in
   * normalize_dq. */
  if (dq->scale_weight) {
    float wmat[4][4];

    if (flipped) {
      /* we don't want negative weights for scaling */
      weight = -weight;
    }

    copy_m4_m4(wmat, (float (*)[4])dq->scale);
    mul_m4_fl(wmat, weight);
    add_m4_m4m4(dq_sum->scale, dq_sum->scale, wmat);
    dq_sum->scale_weight += weight;
  }
}

void add_weighted_dq_dq_pivot(DualQuat *dq_sum,
                              const DualQuat *dq,
                              const float pivot[3],
                              const float weight,
                              const bool compute_scale_matrix)
{
  /* NOTE: If the resulting dual quaternion would only be used to transform the pivot point itself,
   * this function can avoid fully computing the combined scale matrix to get a performance
   * boost without affecting the result. */

  /* FIX #32022, #43188, #100373 - bad deformation when combining scaling and rotation. */
  if (dq->scale_weight) {
    DualQuat mdq = *dq;

    /* Compute the translation induced by scale at the pivot point. */
    float dst[3];
    mul_v3_m4v3(dst, mdq.scale, pivot);
    sub_v3_v3(dst, pivot);

    /* Apply the scale translation to the translation part of the DualQuat. */
    mdq.trans[0] -= .5f * (mdq.quat[1] * dst[0] + mdq.quat[2] * dst[1] + mdq.quat[3] * dst[2]);
    mdq.trans[1] += .5f * (mdq.quat[0] * dst[0] + mdq.quat[2] * dst[2] - mdq.quat[3] * dst[1]);
    mdq.trans[2] += .5f * (mdq.quat[0] * dst[1] + mdq.quat[3] * dst[0] - mdq.quat[1] * dst[2]);
    mdq.trans[3] += .5f * (mdq.quat[0] * dst[2] + mdq.quat[1] * dst[1] - mdq.quat[2] * dst[0]);

    /* Neutralize the scale matrix at the pivot point. */
    if (compute_scale_matrix) {
      /* This translates the matrix to transform the pivot point to itself. */
      sub_v3_v3(mdq.scale[3], dst);
    }
    else {
      /* This completely discards the scale matrix - if the resulting DualQuat
       * is converted to a matrix, it would have no scale or shear. */
      mdq.scale_weight = 0.0f;
    }

    add_weighted_dq_dq(dq_sum, &mdq, weight);
  }
  else {
    add_weighted_dq_dq(dq_sum, dq, weight);
  }
}

void normalize_dq(DualQuat *dq, float totweight)
{
  const float scale = 1.0f / totweight;

  mul_qt_fl(dq->quat, scale);
  mul_qt_fl(dq->trans, scale);

  /* Handle scale if needed. */
  if (dq->scale_weight) {
    /* Compensate for any dual quaternions added without scale. This is an
     * optimization so that we can skip the scale part when not needed. */
    float addweight = totweight - dq->scale_weight;

    if (addweight) {
      dq->scale[0][0] += addweight;
      dq->scale[1][1] += addweight;
      dq->scale[2][2] += addweight;
      dq->scale[3][3] += addweight;
    }

    mul_m4_fl(dq->scale, scale);
    dq->scale_weight = 1.0f;
  }
}

void mul_v3m3_dq(float r[3], float R[3][3], DualQuat *dq)
{
  float M[3][3], t[3], scalemat[3][3], len2;
  float w = dq->quat[0], x = dq->quat[1], y = dq->quat[2], z = dq->quat[3];
  float t0 = dq->trans[0], t1 = dq->trans[1], t2 = dq->trans[2], t3 = dq->trans[3];

  /* rotation matrix */
  M[0][0] = w * w + x * x - y * y - z * z;
  M[1][0] = 2 * (x * y - w * z);
  M[2][0] = 2 * (x * z + w * y);

  M[0][1] = 2 * (x * y + w * z);
  M[1][1] = w * w + y * y - x * x - z * z;
  M[2][1] = 2 * (y * z - w * x);

  M[0][2] = 2 * (x * z - w * y);
  M[1][2] = 2 * (y * z + w * x);
  M[2][2] = w * w + z * z - x * x - y * y;

  len2 = dot_qtqt(dq->quat, dq->quat);
  if (len2 > 0.0f) {
    len2 = 1.0f / len2;
  }

  /* translation */
  t[0] = 2 * (-t0 * x + w * t1 - t2 * z + y * t3);
  t[1] = 2 * (-t0 * y + t1 * z - x * t3 + w * t2);
  t[2] = 2 * (-t0 * z + x * t2 + w * t3 - t1 * y);

  /* apply scaling */
  if (dq->scale_weight) {
    mul_m4_v3(dq->scale, r);
  }

  /* apply rotation and translation */
  mul_m3_v3(M, r);
  r[0] = (r[0] + t[0]) * len2;
  r[1] = (r[1] + t[1]) * len2;
  r[2] = (r[2] + t[2]) * len2;

  /* Compute crazy-space correction matrix. */
  if (R) {
    if (dq->scale_weight) {
      copy_m3_m4(scalemat, dq->scale);
      mul_m3_m3m3(R, M, scalemat);
    }
    else {
      copy_m3_m3(R, M);
    }
    mul_m3_fl(R, len2);
  }
}

void copy_dq_dq(DualQuat *r, const DualQuat *dq)
{
  memcpy(r, dq, sizeof(DualQuat));
}

void quat_apply_track(float quat[4], short axis, short upflag)
{
  /* rotations are hard coded to match vec_to_quat */
  const float sqrt_1_2 = float(M_SQRT1_2);
  const float quat_track[][4] = {
      /* pos-y90 */
      {sqrt_1_2, 0.0, -sqrt_1_2, 0.0},
      /* Quaternion((1,0,0), radians(90)) * Quaternion((0,1,0), radians(90)) */
      {0.5, 0.5, 0.5, 0.5},
      /* pos-z90 */
      {sqrt_1_2, 0.0, 0.0, sqrt_1_2},
      /* neg-y90 */
      {sqrt_1_2, 0.0, sqrt_1_2, 0.0},
      /* Quaternion((1,0,0), radians(-90)) * Quaternion((0,1,0), radians(-90)) */
      {0.5, -0.5, -0.5, 0.5},
      /* no rotation */
      {0.0, sqrt_1_2, sqrt_1_2, 0.0},
  };

  BLI_assert(axis >= 0 && axis <= 5);
  BLI_assert(upflag >= 0 && upflag <= 2);

  mul_qt_qtqt(quat, quat, quat_track[axis]);

  if (axis > 2) {
    axis = short(axis - 3);
  }

  /* there are 2 possible up-axis for each axis used, the 'quat_track' applies so the first
   * up axis is used X->Y, Y->X, Z->X, if this first up axis isn't used then rotate 90d
   * the strange bit shift below just find the low axis {X:Y, Y:X, Z:X} */
  if (upflag != (2 - axis) >> 1) {
    float q[4] = {sqrt_1_2, 0.0, 0.0, 0.0};           /* assign 90d rotation axis */
    q[axis + 1] = (axis == 1) ? sqrt_1_2 : -sqrt_1_2; /* flip non Y axis */
    mul_qt_qtqt(quat, quat, q);
  }
}

void vec_apply_track(float vec[3], short axis)
{
  float tvec[3];

  BLI_assert(axis >= 0 && axis <= 5);

  copy_v3_v3(tvec, vec);

  switch (axis) {
    case 0: /* POS-X. */
      // vec[0] =  0.0;
      vec[1] = tvec[2];
      vec[2] = -tvec[1];
      break;
    case 1: /* POS-Y. */
      // vec[0] = tvec[0];
      // vec[1] =  0.0;
      // vec[2] = tvec[2];
      break;
    case 2: /* POS-Z. */
      // vec[0] = tvec[0];
      // vec[1] = tvec[1];
      // vec[2] =  0.0;
      break;
    case 3: /* NEG-X. */
      // vec[0] =  0.0;
      vec[1] = tvec[2];
      vec[2] = -tvec[1];
      break;
    case 4: /* NEG-Y. */
      vec[0] = -tvec[2];
      // vec[1] =  0.0;
      vec[2] = tvec[0];
      break;
    case 5: /* NEG-Z. */
      vec[0] = -tvec[0];
      vec[1] = -tvec[1];
      // vec[2] =  0.0;
      break;
  }
}

float focallength_to_fov(float focal_length, float sensor)
{
  return 2.0f * atanf((sensor / 2.0f) / focal_length);
}

float fov_to_focallength(float hfov, float sensor)
{
  return (sensor / 2.0f) / tanf(hfov * 0.5f);
}

/* `mod_inline(-3, 4)= 1`, `fmod(-3, 4)= -3` */
static float mod_inline(float a, float b)
{
  return a - (b * floorf(a / b));
}

float angle_wrap_rad(float angle)
{
  return mod_inline(angle + float(M_PI), float(M_PI) * 2.0f) - float(M_PI);
}

float angle_wrap_deg(float angle)
{
  return mod_inline(angle + 180.0f, 360.0f) - 180.0f;
}

float angle_compat_rad(float angle, float angle_compat)
{
  return angle_compat + angle_wrap_rad(angle - angle_compat);
}

/* axis conversion */
static float _axis_convert_matrix[23][3][3] = {
    {{-1.0, 0.0, 0.0}, {0.0, -1.0, 0.0}, {0.0, 0.0, 1.0}},
    {{-1.0, 0.0, 0.0}, {0.0, 0.0, -1.0}, {0.0, -1.0, 0.0}},
    {{-1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {0.0, 1.0, 0.0}},
    {{-1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, -1.0}},
    {{0.0, -1.0, 0.0}, {-1.0, 0.0, 0.0}, {0.0, 0.0, -1.0}},
    {{0.0, 0.0, 1.0}, {-1.0, 0.0, 0.0}, {0.0, -1.0, 0.0}},
    {{0.0, 0.0, -1.0}, {-1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}},
    {{0.0, 1.0, 0.0}, {-1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}},
    {{0.0, -1.0, 0.0}, {0.0, 0.0, 1.0}, {-1.0, 0.0, 0.0}},
    {{0.0, 0.0, -1.0}, {0.0, -1.0, 0.0}, {-1.0, 0.0, 0.0}},
    {{0.0, 0.0, 1.0}, {0.0, 1.0, 0.0}, {-1.0, 0.0, 0.0}},
    {{0.0, 1.0, 0.0}, {0.0, 0.0, -1.0}, {-1.0, 0.0, 0.0}},
    {{0.0, -1.0, 0.0}, {0.0, 0.0, -1.0}, {1.0, 0.0, 0.0}},
    {{0.0, 0.0, 1.0}, {0.0, -1.0, 0.0}, {1.0, 0.0, 0.0}},
    {{0.0, 0.0, -1.0}, {0.0, 1.0, 0.0}, {1.0, 0.0, 0.0}},
    {{0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, {1.0, 0.0, 0.0}},
    {{0.0, -1.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}},
    {{0.0, 0.0, -1.0}, {1.0, 0.0, 0.0}, {0.0, -1.0, 0.0}},
    {{0.0, 0.0, 1.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}},
    {{0.0, 1.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 0.0, -1.0}},
    {{1.0, 0.0, 0.0}, {0.0, -1.0, 0.0}, {0.0, 0.0, -1.0}},
    {{1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {0.0, -1.0, 0.0}},
    {{1.0, 0.0, 0.0}, {0.0, 0.0, -1.0}, {0.0, 1.0, 0.0}},
};

static int _axis_convert_lut[23][24] = {
    {0x8C8, 0x4D0, 0x2E0, 0xAE8, 0x701, 0x511, 0x119, 0xB29, 0x682, 0x88A, 0x09A, 0x2A2,
     0x80B, 0x413, 0x223, 0xA2B, 0x644, 0x454, 0x05C, 0xA6C, 0x745, 0x94D, 0x15D, 0x365},
    {0xAC8, 0x8D0, 0x4E0, 0x2E8, 0x741, 0x951, 0x159, 0x369, 0x702, 0xB0A, 0x11A, 0x522,
     0xA0B, 0x813, 0x423, 0x22B, 0x684, 0x894, 0x09C, 0x2AC, 0x645, 0xA4D, 0x05D, 0x465},
    {0x4C8, 0x2D0, 0xAE0, 0x8E8, 0x681, 0x291, 0x099, 0x8A9, 0x642, 0x44A, 0x05A, 0xA62,
     0x40B, 0x213, 0xA23, 0x82B, 0x744, 0x354, 0x15C, 0x96C, 0x705, 0x50D, 0x11D, 0xB25},
    {0x2C8, 0xAD0, 0x8E0, 0x4E8, 0x641, 0xA51, 0x059, 0x469, 0x742, 0x34A, 0x15A, 0x962,
     0x20B, 0xA13, 0x823, 0x42B, 0x704, 0xB14, 0x11C, 0x52C, 0x685, 0x28D, 0x09D, 0x8A5},
    {0x708, 0xB10, 0x120, 0x528, 0x8C1, 0xAD1, 0x2D9, 0x4E9, 0x942, 0x74A, 0x35A, 0x162,
     0x64B, 0xA53, 0x063, 0x46B, 0x804, 0xA14, 0x21C, 0x42C, 0x885, 0x68D, 0x29D, 0x0A5},
    {0xB08, 0x110, 0x520, 0x728, 0x941, 0x151, 0x359, 0x769, 0x802, 0xA0A, 0x21A, 0x422,
     0xA4B, 0x053, 0x463, 0x66B, 0x884, 0x094, 0x29C, 0x6AC, 0x8C5, 0xACD, 0x2DD, 0x4E5},
    {0x508, 0x710, 0xB20, 0x128, 0x881, 0x691, 0x299, 0x0A9, 0x8C2, 0x4CA, 0x2DA, 0xAE2,
     0x44B, 0x653, 0xA63, 0x06B, 0x944, 0x754, 0x35C, 0x16C, 0x805, 0x40D, 0x21D, 0xA25},
    {0x108, 0x510, 0x720, 0xB28, 0x801, 0x411, 0x219, 0xA29, 0x882, 0x08A, 0x29A, 0x6A2,
     0x04B, 0x453, 0x663, 0xA6B, 0x8C4, 0x4D4, 0x2DC, 0xAEC, 0x945, 0x14D, 0x35D, 0x765},
    {0x748, 0x350, 0x160, 0x968, 0xAC1, 0x2D1, 0x4D9, 0x8E9, 0xA42, 0x64A, 0x45A, 0x062,
     0x68B, 0x293, 0x0A3, 0x8AB, 0xA04, 0x214, 0x41C, 0x82C, 0xB05, 0x70D, 0x51D, 0x125},
    {0x948, 0x750, 0x360, 0x168, 0xB01, 0x711, 0x519, 0x129, 0xAC2, 0x8CA, 0x4DA, 0x2E2,
     0x88B, 0x693, 0x2A3, 0x0AB, 0xA44, 0x654, 0x45C, 0x06C, 0xA05, 0x80D, 0x41D, 0x225},
    {0x348, 0x150, 0x960, 0x768, 0xA41, 0x051, 0x459, 0x669, 0xA02, 0x20A, 0x41A, 0x822,
     0x28B, 0x093, 0x8A3, 0x6AB, 0xB04, 0x114, 0x51C, 0x72C, 0xAC5, 0x2CD, 0x4DD, 0x8E5},
    {0x148, 0x950, 0x760, 0x368, 0xA01, 0x811, 0x419, 0x229, 0xB02, 0x10A, 0x51A, 0x722,
     0x08B, 0x893, 0x6A3, 0x2AB, 0xAC4, 0x8D4, 0x4DC, 0x2EC, 0xA45, 0x04D, 0x45D, 0x665},
    {0x688, 0x890, 0x0A0, 0x2A8, 0x4C1, 0x8D1, 0xAD9, 0x2E9, 0x502, 0x70A, 0xB1A, 0x122,
     0x74B, 0x953, 0x163, 0x36B, 0x404, 0x814, 0xA1C, 0x22C, 0x445, 0x64D, 0xA5D, 0x065},
    {0x888, 0x090, 0x2A0, 0x6A8, 0x501, 0x111, 0xB19, 0x729, 0x402, 0x80A, 0xA1A, 0x222,
     0x94B, 0x153, 0x363, 0x76B, 0x444, 0x054, 0xA5C, 0x66C, 0x4C5, 0x8CD, 0xADD, 0x2E5},
    {0x288, 0x690, 0x8A0, 0x0A8, 0x441, 0x651, 0xA59, 0x069, 0x4C2, 0x2CA, 0xADA, 0x8E2,
     0x34B, 0x753, 0x963, 0x16B, 0x504, 0x714, 0xB1C, 0x12C, 0x405, 0x20D, 0xA1D, 0x825},
    {0x088, 0x290, 0x6A0, 0x8A8, 0x401, 0x211, 0xA19, 0x829, 0x442, 0x04A, 0xA5A, 0x662,
     0x14B, 0x353, 0x763, 0x96B, 0x4C4, 0x2D4, 0xADC, 0x8EC, 0x505, 0x10D, 0xB1D, 0x725},
    {0x648, 0x450, 0x060, 0xA68, 0x2C1, 0x4D1, 0x8D9, 0xAE9, 0x282, 0x68A, 0x89A, 0x0A2,
     0x70B, 0x513, 0x123, 0xB2B, 0x204, 0x414, 0x81C, 0xA2C, 0x345, 0x74D, 0x95D, 0x165},
    {0xA48, 0x650, 0x460, 0x068, 0x341, 0x751, 0x959, 0x169, 0x2C2, 0xACA, 0x8DA, 0x4E2,
     0xB0B, 0x713, 0x523, 0x12B, 0x284, 0x694, 0x89C, 0x0AC, 0x205, 0xA0D, 0x81D, 0x425},
    {0x448, 0x050, 0xA60, 0x668, 0x281, 0x091, 0x899, 0x6A9, 0x202, 0x40A, 0x81A, 0xA22,
     0x50B, 0x113, 0xB23, 0x72B, 0x344, 0x154, 0x95C, 0x76C, 0x2C5, 0x4CD, 0x8DD, 0xAE5},
    {0x048, 0xA50, 0x660, 0x468, 0x201, 0xA11, 0x819, 0x429, 0x342, 0x14A, 0x95A, 0x762,
     0x10B, 0xB13, 0x723, 0x52B, 0x2C4, 0xAD4, 0x8DC, 0x4EC, 0x285, 0x08D, 0x89D, 0x6A5},
    {0x808, 0xA10, 0x220, 0x428, 0x101, 0xB11, 0x719, 0x529, 0x142, 0x94A, 0x75A, 0x362,
     0x8CB, 0xAD3, 0x2E3, 0x4EB, 0x044, 0xA54, 0x65C, 0x46C, 0x085, 0x88D, 0x69D, 0x2A5},
    {0xA08, 0x210, 0x420, 0x828, 0x141, 0x351, 0x759, 0x969, 0x042, 0xA4A, 0x65A, 0x462,
     0xACB, 0x2D3, 0x4E3, 0x8EB, 0x084, 0x294, 0x69C, 0x8AC, 0x105, 0xB0D, 0x71D, 0x525},
    {0x408, 0x810, 0xA20, 0x228, 0x081, 0x891, 0x699, 0x2A9, 0x102, 0x50A, 0x71A, 0xB22,
     0x4CB, 0x8D3, 0xAE3, 0x2EB, 0x144, 0x954, 0x75C, 0x36C, 0x045, 0x44D, 0x65D, 0xA65},
};

// _axis_convert_num = {'X': 0, 'Y': 1, 'Z': 2, '-X': 3, '-Y': 4, '-Z': 5}

BLI_INLINE int _axis_signed(const int axis)
{
  return (axis < 3) ? axis : axis - 3;
}

bool mat3_from_axis_conversion(
    int src_forward, int src_up, int dst_forward, int dst_up, float r_mat[3][3])
{
  int value;

  if (src_forward == dst_forward && src_up == dst_up) {
    unit_m3(r_mat);
    return false;
  }

  if ((_axis_signed(src_forward) == _axis_signed(src_up)) ||
      (_axis_signed(dst_forward) == _axis_signed(dst_up)))
  {
    /* we could assert here! */
    unit_m3(r_mat);
    return false;
  }

  value = ((src_forward << (0 * 3)) | (src_up << (1 * 3)) | (dst_forward << (2 * 3)) |
           (dst_up << (3 * 3)));

  for (uint i = 0; i < ARRAY_SIZE(_axis_convert_matrix); i++) {
    for (uint j = 0; j < ARRAY_SIZE(*_axis_convert_lut); j++) {
      if (_axis_convert_lut[i][j] == value) {
        copy_m3_m3(r_mat, _axis_convert_matrix[i]);
        return true;
      }
    }
  }
  //  BLI_assert(0);
  return false;
}

bool mat3_from_axis_conversion_single(int src_axis, int dst_axis, float r_mat[3][3])
{
  if (src_axis == dst_axis) {
    unit_m3(r_mat);
    return false;
  }

  /* Pick predictable next axis. */
  int src_axis_next = (src_axis + 1) % 3;
  int dst_axis_next = (dst_axis + 1) % 3;

  if ((src_axis < 3) != (dst_axis < 3)) {
    /* Flip both axis so matrix sign remains positive. */
    dst_axis_next += 3;
  }

  return mat3_from_axis_conversion(src_axis, src_axis_next, dst_axis, dst_axis_next, r_mat);
}
