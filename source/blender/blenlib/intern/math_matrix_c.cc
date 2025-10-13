/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_solvers.h"
#include "BLI_math_vector.h"
#include "BLI_simd.hh"

#ifndef MATH_STANDALONE
#  include "eigen_capi.h"
#endif

#include <cstring>

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

/********************************* Init **************************************/

void zero_m3(float m[3][3])
{
  memset(m, 0, sizeof(float[3][3]));
}

void zero_m4(float m[4][4])
{
  memset(m, 0, sizeof(float[4][4]));
}

void unit_m2(float m[2][2])
{
  m[0][0] = m[1][1] = 1.0f;
  m[0][1] = 0.0f;
  m[1][0] = 0.0f;
}

void unit_m3(float m[3][3])
{
  m[0][0] = m[1][1] = m[2][2] = 1.0f;
  m[0][1] = m[0][2] = 0.0f;
  m[1][0] = m[1][2] = 0.0f;
  m[2][0] = m[2][1] = 0.0f;
}

void unit_m4(float m[4][4])
{
  m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1.0f;
  m[0][1] = m[0][2] = m[0][3] = 0.0f;
  m[1][0] = m[1][2] = m[1][3] = 0.0f;
  m[2][0] = m[2][1] = m[2][3] = 0.0f;
  m[3][0] = m[3][1] = m[3][2] = 0.0f;
}

void unit_m4_db(double m[4][4])
{
  m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1.0f;
  m[0][1] = m[0][2] = m[0][3] = 0.0f;
  m[1][0] = m[1][2] = m[1][3] = 0.0f;
  m[2][0] = m[2][1] = m[2][3] = 0.0f;
  m[3][0] = m[3][1] = m[3][2] = 0.0f;
}

void copy_m2_m2(float m1[2][2], const float m2[2][2])
{
  memcpy(m1, m2, sizeof(float[2][2]));
}

void copy_m3_m3(float m1[3][3], const float m2[3][3])
{
  /* destination comes first: */
  memcpy(m1, m2, sizeof(float[3][3]));
}

void copy_m4_m4(float m1[4][4], const float m2[4][4])
{
  memcpy(m1, m2, sizeof(float[4][4]));
}

void copy_m4_m4_db(double m1[4][4], const double m2[4][4])
{
  memcpy(m1, m2, sizeof(double[4][4]));
}

void copy_m3_m4(float m1[3][3], const float m2[4][4])
{
  m1[0][0] = m2[0][0];
  m1[0][1] = m2[0][1];
  m1[0][2] = m2[0][2];

  m1[1][0] = m2[1][0];
  m1[1][1] = m2[1][1];
  m1[1][2] = m2[1][2];

  m1[2][0] = m2[2][0];
  m1[2][1] = m2[2][1];
  m1[2][2] = m2[2][2];
}

void copy_m4_m3(float m1[4][4], const float m2[3][3]) /* no clear */
{
  m1[0][0] = m2[0][0];
  m1[0][1] = m2[0][1];
  m1[0][2] = m2[0][2];

  m1[1][0] = m2[1][0];
  m1[1][1] = m2[1][1];
  m1[1][2] = m2[1][2];

  m1[2][0] = m2[2][0];
  m1[2][1] = m2[2][1];
  m1[2][2] = m2[2][2];

  m1[0][3] = 0.0f;
  m1[1][3] = 0.0f;
  m1[2][3] = 0.0f;

  m1[3][0] = 0.0f;
  m1[3][1] = 0.0f;
  m1[3][2] = 0.0f;
  m1[3][3] = 1.0f;
}

void copy_m3d_m3(double m1[3][3], const float m2[3][3])
{
  m1[0][0] = m2[0][0];
  m1[0][1] = m2[0][1];
  m1[0][2] = m2[0][2];

  m1[1][0] = m2[1][0];
  m1[1][1] = m2[1][1];
  m1[1][2] = m2[1][2];

  m1[2][0] = m2[2][0];
  m1[2][1] = m2[2][1];
  m1[2][2] = m2[2][2];
}

void copy_m4d_m4(double m1[4][4], const float m2[4][4])
{
  m1[0][0] = m2[0][0];
  m1[0][1] = m2[0][1];
  m1[0][2] = m2[0][2];
  m1[0][3] = m2[0][3];

  m1[1][0] = m2[1][0];
  m1[1][1] = m2[1][1];
  m1[1][2] = m2[1][2];
  m1[1][3] = m2[1][3];

  m1[2][0] = m2[2][0];
  m1[2][1] = m2[2][1];
  m1[2][2] = m2[2][2];
  m1[2][3] = m2[2][3];

  m1[3][0] = m2[3][0];
  m1[3][1] = m2[3][1];
  m1[3][2] = m2[3][2];
  m1[3][3] = m2[3][3];
}

void copy_m3_m3d(float m1[3][3], const double m2[3][3])
{
  /* Keep it stupid simple for better data flow in CPU. */
  m1[0][0] = float(m2[0][0]);
  m1[0][1] = float(m2[0][1]);
  m1[0][2] = float(m2[0][2]);

  m1[1][0] = float(m2[1][0]);
  m1[1][1] = float(m2[1][1]);
  m1[1][2] = float(m2[1][2]);

  m1[2][0] = float(m2[2][0]);
  m1[2][1] = float(m2[2][1]);
  m1[2][2] = float(m2[2][2]);
}

void swap_m4m4(float m1[4][4], float m2[4][4])
{
  float t;
  int i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      t = m1[i][j];
      m1[i][j] = m2[i][j];
      m2[i][j] = t;
    }
  }
}

void shuffle_m4(float R[4][4], const int index[4])
{
  zero_m4(R);
  for (int k = 0; k < 4; k++) {
    if (index[k] >= 0) {
      R[index[k]][k] = 1.0f;
    }
  }
}

/******************************** Arithmetic *********************************/

void mul_m4_m4m4(float R[4][4], const float A[4][4], const float B[4][4])
{
  if (ELEM(R, A, B)) {
    float T[4][4];
    mul_m4_m4m4(T, A, B);
    copy_m4_m4(R, T);
    return;
  }

  /* Matrix product: `R[j][k] = B[j][i] . A[i][k]`. */
#if BLI_HAVE_SSE2
  __m128 A0 = _mm_loadu_ps(A[0]);
  __m128 A1 = _mm_loadu_ps(A[1]);
  __m128 A2 = _mm_loadu_ps(A[2]);
  __m128 A3 = _mm_loadu_ps(A[3]);

  for (int i = 0; i < 4; i++) {
    __m128 B0 = _mm_set1_ps(B[i][0]);
    __m128 B1 = _mm_set1_ps(B[i][1]);
    __m128 B2 = _mm_set1_ps(B[i][2]);
    __m128 B3 = _mm_set1_ps(B[i][3]);

    __m128 sum = _mm_add_ps(_mm_add_ps(_mm_mul_ps(B0, A0), _mm_mul_ps(B1, A1)),
                            _mm_add_ps(_mm_mul_ps(B2, A2), _mm_mul_ps(B3, A3)));

    _mm_storeu_ps(R[i], sum);
  }
#else
  R[0][0] = B[0][0] * A[0][0] + B[0][1] * A[1][0] + B[0][2] * A[2][0] + B[0][3] * A[3][0];
  R[0][1] = B[0][0] * A[0][1] + B[0][1] * A[1][1] + B[0][2] * A[2][1] + B[0][3] * A[3][1];
  R[0][2] = B[0][0] * A[0][2] + B[0][1] * A[1][2] + B[0][2] * A[2][2] + B[0][3] * A[3][2];
  R[0][3] = B[0][0] * A[0][3] + B[0][1] * A[1][3] + B[0][2] * A[2][3] + B[0][3] * A[3][3];

  R[1][0] = B[1][0] * A[0][0] + B[1][1] * A[1][0] + B[1][2] * A[2][0] + B[1][3] * A[3][0];
  R[1][1] = B[1][0] * A[0][1] + B[1][1] * A[1][1] + B[1][2] * A[2][1] + B[1][3] * A[3][1];
  R[1][2] = B[1][0] * A[0][2] + B[1][1] * A[1][2] + B[1][2] * A[2][2] + B[1][3] * A[3][2];
  R[1][3] = B[1][0] * A[0][3] + B[1][1] * A[1][3] + B[1][2] * A[2][3] + B[1][3] * A[3][3];

  R[2][0] = B[2][0] * A[0][0] + B[2][1] * A[1][0] + B[2][2] * A[2][0] + B[2][3] * A[3][0];
  R[2][1] = B[2][0] * A[0][1] + B[2][1] * A[1][1] + B[2][2] * A[2][1] + B[2][3] * A[3][1];
  R[2][2] = B[2][0] * A[0][2] + B[2][1] * A[1][2] + B[2][2] * A[2][2] + B[2][3] * A[3][2];
  R[2][3] = B[2][0] * A[0][3] + B[2][1] * A[1][3] + B[2][2] * A[2][3] + B[2][3] * A[3][3];

  R[3][0] = B[3][0] * A[0][0] + B[3][1] * A[1][0] + B[3][2] * A[2][0] + B[3][3] * A[3][0];
  R[3][1] = B[3][0] * A[0][1] + B[3][1] * A[1][1] + B[3][2] * A[2][1] + B[3][3] * A[3][1];
  R[3][2] = B[3][0] * A[0][2] + B[3][1] * A[1][2] + B[3][2] * A[2][2] + B[3][3] * A[3][2];
  R[3][3] = B[3][0] * A[0][3] + B[3][1] * A[1][3] + B[3][2] * A[2][3] + B[3][3] * A[3][3];
#endif
}

void mul_m4db_m4db_m4fl(double R[4][4], const double A[4][4], const float B[4][4])
{
  if (R == A) {
    double T[4][4];
    mul_m4db_m4db_m4fl(T, A, B);
    copy_m4_m4_db(R, T);
    return;
  }

  /* Matrix product: `R[j][k] = B[j][i] . A[i][k]`. */

  R[0][0] = B[0][0] * A[0][0] + B[0][1] * A[1][0] + B[0][2] * A[2][0] + B[0][3] * A[3][0];
  R[0][1] = B[0][0] * A[0][1] + B[0][1] * A[1][1] + B[0][2] * A[2][1] + B[0][3] * A[3][1];
  R[0][2] = B[0][0] * A[0][2] + B[0][1] * A[1][2] + B[0][2] * A[2][2] + B[0][3] * A[3][2];
  R[0][3] = B[0][0] * A[0][3] + B[0][1] * A[1][3] + B[0][2] * A[2][3] + B[0][3] * A[3][3];

  R[1][0] = B[1][0] * A[0][0] + B[1][1] * A[1][0] + B[1][2] * A[2][0] + B[1][3] * A[3][0];
  R[1][1] = B[1][0] * A[0][1] + B[1][1] * A[1][1] + B[1][2] * A[2][1] + B[1][3] * A[3][1];
  R[1][2] = B[1][0] * A[0][2] + B[1][1] * A[1][2] + B[1][2] * A[2][2] + B[1][3] * A[3][2];
  R[1][3] = B[1][0] * A[0][3] + B[1][1] * A[1][3] + B[1][2] * A[2][3] + B[1][3] * A[3][3];

  R[2][0] = B[2][0] * A[0][0] + B[2][1] * A[1][0] + B[2][2] * A[2][0] + B[2][3] * A[3][0];
  R[2][1] = B[2][0] * A[0][1] + B[2][1] * A[1][1] + B[2][2] * A[2][1] + B[2][3] * A[3][1];
  R[2][2] = B[2][0] * A[0][2] + B[2][1] * A[1][2] + B[2][2] * A[2][2] + B[2][3] * A[3][2];
  R[2][3] = B[2][0] * A[0][3] + B[2][1] * A[1][3] + B[2][2] * A[2][3] + B[2][3] * A[3][3];

  R[3][0] = B[3][0] * A[0][0] + B[3][1] * A[1][0] + B[3][2] * A[2][0] + B[3][3] * A[3][0];
  R[3][1] = B[3][0] * A[0][1] + B[3][1] * A[1][1] + B[3][2] * A[2][1] + B[3][3] * A[3][1];
  R[3][2] = B[3][0] * A[0][2] + B[3][1] * A[1][2] + B[3][2] * A[2][2] + B[3][3] * A[3][2];
  R[3][3] = B[3][0] * A[0][3] + B[3][1] * A[1][3] + B[3][2] * A[2][3] + B[3][3] * A[3][3];
}

void mul_m4_m4_pre(float R[4][4], const float A[4][4])
{
  mul_m4_m4m4(R, A, R);
}

void mul_m4_m4_post(float R[4][4], const float B[4][4])
{
  mul_m4_m4m4(R, R, B);
}

void mul_m3_m3_pre(float R[3][3], const float A[3][3])
{
  mul_m3_m3m3(R, A, R);
}

void mul_m3_m3_post(float R[3][3], const float B[3][3])
{
  mul_m3_m3m3(R, R, B);
}

void mul_m3_m3m3(float R[3][3], const float A[3][3], const float B[3][3])
{
  if (ELEM(R, A, B)) {
    float T[3][3];
    mul_m3_m3m3(T, A, B);
    copy_m3_m3(R, T);
    return;
  }
  R[0][0] = B[0][0] * A[0][0] + B[0][1] * A[1][0] + B[0][2] * A[2][0];
  R[0][1] = B[0][0] * A[0][1] + B[0][1] * A[1][1] + B[0][2] * A[2][1];
  R[0][2] = B[0][0] * A[0][2] + B[0][1] * A[1][2] + B[0][2] * A[2][2];

  R[1][0] = B[1][0] * A[0][0] + B[1][1] * A[1][0] + B[1][2] * A[2][0];
  R[1][1] = B[1][0] * A[0][1] + B[1][1] * A[1][1] + B[1][2] * A[2][1];
  R[1][2] = B[1][0] * A[0][2] + B[1][1] * A[1][2] + B[1][2] * A[2][2];

  R[2][0] = B[2][0] * A[0][0] + B[2][1] * A[1][0] + B[2][2] * A[2][0];
  R[2][1] = B[2][0] * A[0][1] + B[2][1] * A[1][1] + B[2][2] * A[2][1];
  R[2][2] = B[2][0] * A[0][2] + B[2][1] * A[1][2] + B[2][2] * A[2][2];
}

void mul_m4_m4m3(float R[4][4], const float A[4][4], const float B[3][3])
{
  if (R == A) {
    float T[4][4];
    /* The mul_m4_m4m3 only writes to the upper-left 3x3 block, so make it so the rest of the
     * matrix is copied from the input to the output.
     *
     * TODO(sergey): It does sound a bit redundant from the number of copy operations, so there is
     * a potential for optimization. */
    copy_m4_m4(T, A);
    mul_m4_m4m3(T, A, B);
    copy_m4_m4(R, T);
    return;
  }

  R[0][0] = B[0][0] * A[0][0] + B[0][1] * A[1][0] + B[0][2] * A[2][0];
  R[0][1] = B[0][0] * A[0][1] + B[0][1] * A[1][1] + B[0][2] * A[2][1];
  R[0][2] = B[0][0] * A[0][2] + B[0][1] * A[1][2] + B[0][2] * A[2][2];
  R[1][0] = B[1][0] * A[0][0] + B[1][1] * A[1][0] + B[1][2] * A[2][0];
  R[1][1] = B[1][0] * A[0][1] + B[1][1] * A[1][1] + B[1][2] * A[2][1];
  R[1][2] = B[1][0] * A[0][2] + B[1][1] * A[1][2] + B[1][2] * A[2][2];
  R[2][0] = B[2][0] * A[0][0] + B[2][1] * A[1][0] + B[2][2] * A[2][0];
  R[2][1] = B[2][0] * A[0][1] + B[2][1] * A[1][1] + B[2][2] * A[2][1];
  R[2][2] = B[2][0] * A[0][2] + B[2][1] * A[1][2] + B[2][2] * A[2][2];
}

void mul_m3_m3m4(float R[3][3], const float A[3][3], const float B[4][4])
{
  if (R == A) {
    float T[3][3];
    mul_m3_m3m4(T, A, B);
    copy_m3_m3(R, T);
    return;
  }

  /* Matrix product: `R[j][k] = B[j][i] . A[i][k]`. */

  R[0][0] = B[0][0] * A[0][0] + B[0][1] * A[1][0] + B[0][2] * A[2][0];
  R[0][1] = B[0][0] * A[0][1] + B[0][1] * A[1][1] + B[0][2] * A[2][1];
  R[0][2] = B[0][0] * A[0][2] + B[0][1] * A[1][2] + B[0][2] * A[2][2];

  R[1][0] = B[1][0] * A[0][0] + B[1][1] * A[1][0] + B[1][2] * A[2][0];
  R[1][1] = B[1][0] * A[0][1] + B[1][1] * A[1][1] + B[1][2] * A[2][1];
  R[1][2] = B[1][0] * A[0][2] + B[1][1] * A[1][2] + B[1][2] * A[2][2];

  R[2][0] = B[2][0] * A[0][0] + B[2][1] * A[1][0] + B[2][2] * A[2][0];
  R[2][1] = B[2][0] * A[0][1] + B[2][1] * A[1][1] + B[2][2] * A[2][1];
  R[2][2] = B[2][0] * A[0][2] + B[2][1] * A[1][2] + B[2][2] * A[2][2];
}

void mul_m3_m4m3(float R[3][3], const float A[4][4], const float B[3][3])
{
  if (R == B) {
    float T[3][3];
    mul_m3_m4m3(T, A, B);
    copy_m3_m3(R, T);
    return;
  }

  /* Matrix product: `R[j][k] = B[j][i] . A[i][k]`. */

  R[0][0] = B[0][0] * A[0][0] + B[0][1] * A[1][0] + B[0][2] * A[2][0];
  R[0][1] = B[0][0] * A[0][1] + B[0][1] * A[1][1] + B[0][2] * A[2][1];
  R[0][2] = B[0][0] * A[0][2] + B[0][1] * A[1][2] + B[0][2] * A[2][2];

  R[1][0] = B[1][0] * A[0][0] + B[1][1] * A[1][0] + B[1][2] * A[2][0];
  R[1][1] = B[1][0] * A[0][1] + B[1][1] * A[1][1] + B[1][2] * A[2][1];
  R[1][2] = B[1][0] * A[0][2] + B[1][1] * A[1][2] + B[1][2] * A[2][2];

  R[2][0] = B[2][0] * A[0][0] + B[2][1] * A[1][0] + B[2][2] * A[2][0];
  R[2][1] = B[2][0] * A[0][1] + B[2][1] * A[1][1] + B[2][2] * A[2][1];
  R[2][2] = B[2][0] * A[0][2] + B[2][1] * A[1][2] + B[2][2] * A[2][2];
}

void mul_m4_m3m4(float R[4][4], const float A[3][3], const float B[4][4])
{
  if (R == B) {
    float T[4][4];
    /* The mul_m4_m4m3 only writes to the upper-left 3x3 block, so make it so the rest of the
     * matrix is copied from the input to the output.
     *
     * TODO(sergey): It does sound a bit redundant from the number of copy operations, so there is
     * a potential for optimization. */
    copy_m4_m4(T, B);
    mul_m4_m3m4(T, A, B);
    copy_m4_m4(R, T);
    return;
  }

  R[0][0] = B[0][0] * A[0][0] + B[0][1] * A[1][0] + B[0][2] * A[2][0];
  R[0][1] = B[0][0] * A[0][1] + B[0][1] * A[1][1] + B[0][2] * A[2][1];
  R[0][2] = B[0][0] * A[0][2] + B[0][1] * A[1][2] + B[0][2] * A[2][2];
  R[1][0] = B[1][0] * A[0][0] + B[1][1] * A[1][0] + B[1][2] * A[2][0];
  R[1][1] = B[1][0] * A[0][1] + B[1][1] * A[1][1] + B[1][2] * A[2][1];
  R[1][2] = B[1][0] * A[0][2] + B[1][1] * A[1][2] + B[1][2] * A[2][2];
  R[2][0] = B[2][0] * A[0][0] + B[2][1] * A[1][0] + B[2][2] * A[2][0];
  R[2][1] = B[2][0] * A[0][1] + B[2][1] * A[1][1] + B[2][2] * A[2][1];
  R[2][2] = B[2][0] * A[0][2] + B[2][1] * A[1][2] + B[2][2] * A[2][2];
}

void mul_m3_m4m4(float R[3][3], const float A[4][4], const float B[4][4])
{
  R[0][0] = B[0][0] * A[0][0] + B[0][1] * A[1][0] + B[0][2] * A[2][0];
  R[0][1] = B[0][0] * A[0][1] + B[0][1] * A[1][1] + B[0][2] * A[2][1];
  R[0][2] = B[0][0] * A[0][2] + B[0][1] * A[1][2] + B[0][2] * A[2][2];
  R[1][0] = B[1][0] * A[0][0] + B[1][1] * A[1][0] + B[1][2] * A[2][0];
  R[1][1] = B[1][0] * A[0][1] + B[1][1] * A[1][1] + B[1][2] * A[2][1];
  R[1][2] = B[1][0] * A[0][2] + B[1][1] * A[1][2] + B[1][2] * A[2][2];
  R[2][0] = B[2][0] * A[0][0] + B[2][1] * A[1][0] + B[2][2] * A[2][0];
  R[2][1] = B[2][0] * A[0][1] + B[2][1] * A[1][1] + B[2][2] * A[2][1];
  R[2][2] = B[2][0] * A[0][2] + B[2][1] * A[1][2] + B[2][2] * A[2][2];
}

/* -------------------------------------------------------------------- */
/** \name Macro helpers for: mul_m3_series
 * \{ */

void _va_mul_m3_series_3(float r[3][3], const float m1[3][3], const float m2[3][3])
{
  mul_m3_m3m3(r, m1, m2);
}
void _va_mul_m3_series_4(float r[3][3],
                         const float m1[3][3],
                         const float m2[3][3],
                         const float m3[3][3])
{
  float s[3][3];
  mul_m3_m3m3(s, m1, m2);
  mul_m3_m3m3(r, s, m3);
}
void _va_mul_m3_series_5(float r[3][3],
                         const float m1[3][3],
                         const float m2[3][3],
                         const float m3[3][3],
                         const float m4[3][3])
{
  float s[3][3];
  float t[3][3];
  mul_m3_m3m3(s, m1, m2);
  mul_m3_m3m3(t, s, m3);
  mul_m3_m3m3(r, t, m4);
}
void _va_mul_m3_series_6(float r[3][3],
                         const float m1[3][3],
                         const float m2[3][3],
                         const float m3[3][3],
                         const float m4[3][3],
                         const float m5[3][3])
{
  float s[3][3];
  float t[3][3];
  mul_m3_m3m3(s, m1, m2);
  mul_m3_m3m3(t, s, m3);
  mul_m3_m3m3(s, t, m4);
  mul_m3_m3m3(r, s, m5);
}
void _va_mul_m3_series_7(float r[3][3],
                         const float m1[3][3],
                         const float m2[3][3],
                         const float m3[3][3],
                         const float m4[3][3],
                         const float m5[3][3],
                         const float m6[3][3])
{
  float s[3][3];
  float t[3][3];
  mul_m3_m3m3(s, m1, m2);
  mul_m3_m3m3(t, s, m3);
  mul_m3_m3m3(s, t, m4);
  mul_m3_m3m3(t, s, m5);
  mul_m3_m3m3(r, t, m6);
}
void _va_mul_m3_series_8(float r[3][3],
                         const float m1[3][3],
                         const float m2[3][3],
                         const float m3[3][3],
                         const float m4[3][3],
                         const float m5[3][3],
                         const float m6[3][3],
                         const float m7[3][3])
{
  float s[3][3];
  float t[3][3];
  mul_m3_m3m3(s, m1, m2);
  mul_m3_m3m3(t, s, m3);
  mul_m3_m3m3(s, t, m4);
  mul_m3_m3m3(t, s, m5);
  mul_m3_m3m3(s, t, m6);
  mul_m3_m3m3(r, s, m7);
}
void _va_mul_m3_series_9(float r[3][3],
                         const float m1[3][3],
                         const float m2[3][3],
                         const float m3[3][3],
                         const float m4[3][3],
                         const float m5[3][3],
                         const float m6[3][3],
                         const float m7[3][3],
                         const float m8[3][3])
{
  float s[3][3];
  float t[3][3];
  mul_m3_m3m3(s, m1, m2);
  mul_m3_m3m3(t, s, m3);
  mul_m3_m3m3(s, t, m4);
  mul_m3_m3m3(t, s, m5);
  mul_m3_m3m3(s, t, m6);
  mul_m3_m3m3(t, s, m7);
  mul_m3_m3m3(r, t, m8);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Macro helpers for: mul_m4_series
 * \{ */

void _va_mul_m4_series_3(float r[4][4], const float m1[4][4], const float m2[4][4])
{
  mul_m4_m4m4(r, m1, m2);
}
void _va_mul_m4_series_4(float r[4][4],
                         const float m1[4][4],
                         const float m2[4][4],
                         const float m3[4][4])
{
  float s[4][4];
  mul_m4_m4m4(s, m1, m2);
  mul_m4_m4m4(r, s, m3);
}
void _va_mul_m4_series_5(float r[4][4],
                         const float m1[4][4],
                         const float m2[4][4],
                         const float m3[4][4],
                         const float m4[4][4])
{
  float s[4][4];
  float t[4][4];
  mul_m4_m4m4(s, m1, m2);
  mul_m4_m4m4(t, s, m3);
  mul_m4_m4m4(r, t, m4);
}
void _va_mul_m4_series_6(float r[4][4],
                         const float m1[4][4],
                         const float m2[4][4],
                         const float m3[4][4],
                         const float m4[4][4],
                         const float m5[4][4])
{
  float s[4][4];
  float t[4][4];
  mul_m4_m4m4(s, m1, m2);
  mul_m4_m4m4(t, s, m3);
  mul_m4_m4m4(s, t, m4);
  mul_m4_m4m4(r, s, m5);
}
void _va_mul_m4_series_7(float r[4][4],
                         const float m1[4][4],
                         const float m2[4][4],
                         const float m3[4][4],
                         const float m4[4][4],
                         const float m5[4][4],
                         const float m6[4][4])
{
  float s[4][4];
  float t[4][4];
  mul_m4_m4m4(s, m1, m2);
  mul_m4_m4m4(t, s, m3);
  mul_m4_m4m4(s, t, m4);
  mul_m4_m4m4(t, s, m5);
  mul_m4_m4m4(r, t, m6);
}
void _va_mul_m4_series_8(float r[4][4],
                         const float m1[4][4],
                         const float m2[4][4],
                         const float m3[4][4],
                         const float m4[4][4],
                         const float m5[4][4],
                         const float m6[4][4],
                         const float m7[4][4])
{
  float s[4][4];
  float t[4][4];
  mul_m4_m4m4(s, m1, m2);
  mul_m4_m4m4(t, s, m3);
  mul_m4_m4m4(s, t, m4);
  mul_m4_m4m4(t, s, m5);
  mul_m4_m4m4(s, t, m6);
  mul_m4_m4m4(r, s, m7);
}
void _va_mul_m4_series_9(float r[4][4],
                         const float m1[4][4],
                         const float m2[4][4],
                         const float m3[4][4],
                         const float m4[4][4],
                         const float m5[4][4],
                         const float m6[4][4],
                         const float m7[4][4],
                         const float m8[4][4])
{
  float s[4][4];
  float t[4][4];
  mul_m4_m4m4(s, m1, m2);
  mul_m4_m4m4(t, s, m3);
  mul_m4_m4m4(s, t, m4);
  mul_m4_m4m4(t, s, m5);
  mul_m4_m4m4(s, t, m6);
  mul_m4_m4m4(t, s, m7);
  mul_m4_m4m4(r, t, m8);
}

/** \} */

void mul_v2_m3v2(float r[2], const float m[3][3], const float v[2])
{
  float temp[3], warped[3];

  copy_v2_v2(temp, v);
  temp[2] = 1.0f;

  mul_v3_m3v3(warped, m, temp);

  r[0] = warped[0] / warped[2];
  r[1] = warped[1] / warped[2];
}

void mul_m3_v2(const float m[3][3], float r[2])
{
  mul_v2_m3v2(r, m, r);
}

void mul_m4_v3(const float M[4][4], float r[3])
{
  const float x = r[0];
  const float y = r[1];

  r[0] = x * M[0][0] + y * M[1][0] + M[2][0] * r[2] + M[3][0];
  r[1] = x * M[0][1] + y * M[1][1] + M[2][1] * r[2] + M[3][1];
  r[2] = x * M[0][2] + y * M[1][2] + M[2][2] * r[2] + M[3][2];
}

void mul_v3_m4v3(float r[3], const float mat[4][4], const float vec[3])
{
  const float x = vec[0];
  const float y = vec[1];

  r[0] = x * mat[0][0] + y * mat[1][0] + mat[2][0] * vec[2] + mat[3][0];
  r[1] = x * mat[0][1] + y * mat[1][1] + mat[2][1] * vec[2] + mat[3][1];
  r[2] = x * mat[0][2] + y * mat[1][2] + mat[2][2] * vec[2] + mat[3][2];
}

void mul_v3_m4v3_db(double r[3], const double mat[4][4], const double vec[3])
{
  const double x = vec[0];
  const double y = vec[1];

  r[0] = x * mat[0][0] + y * mat[1][0] + mat[2][0] * vec[2] + mat[3][0];
  r[1] = x * mat[0][1] + y * mat[1][1] + mat[2][1] * vec[2] + mat[3][1];
  r[2] = x * mat[0][2] + y * mat[1][2] + mat[2][2] * vec[2] + mat[3][2];
}
void mul_v4_m4v3_db(double r[4], const double mat[4][4], const double vec[3])
{
  const double x = vec[0];
  const double y = vec[1];

  r[0] = x * mat[0][0] + y * mat[1][0] + mat[2][0] * vec[2] + mat[3][0];
  r[1] = x * mat[0][1] + y * mat[1][1] + mat[2][1] * vec[2] + mat[3][1];
  r[2] = x * mat[0][2] + y * mat[1][2] + mat[2][2] * vec[2] + mat[3][2];
  r[3] = x * mat[0][3] + y * mat[1][3] + mat[2][3] * vec[2] + mat[3][3];
}

void mul_v2_m4v3(float r[2], const float mat[4][4], const float vec[3])
{
  const float x = vec[0];

  r[0] = x * mat[0][0] + vec[1] * mat[1][0] + mat[2][0] * vec[2] + mat[3][0];
  r[1] = x * mat[0][1] + vec[1] * mat[1][1] + mat[2][1] * vec[2] + mat[3][1];
}

void mul_v2_m2v2(float r[2], const float mat[2][2], const float vec[2])
{
  const float x = vec[0];

  r[0] = mat[0][0] * x + mat[1][0] * vec[1];
  r[1] = mat[0][1] * x + mat[1][1] * vec[1];
}

void mul_m2_v2(const float mat[2][2], float vec[2])
{
  mul_v2_m2v2(vec, mat, vec);
}

void mul_mat3_m4_v3(const float mat[4][4], float r[3])
{
  const float x = r[0];
  const float y = r[1];

  r[0] = x * mat[0][0] + y * mat[1][0] + mat[2][0] * r[2];
  r[1] = x * mat[0][1] + y * mat[1][1] + mat[2][1] * r[2];
  r[2] = x * mat[0][2] + y * mat[1][2] + mat[2][2] * r[2];
}

void mul_v3_mat3_m4v3(float r[3], const float mat[4][4], const float vec[3])
{
  const float x = vec[0];
  const float y = vec[1];

  r[0] = x * mat[0][0] + y * mat[1][0] + mat[2][0] * vec[2];
  r[1] = x * mat[0][1] + y * mat[1][1] + mat[2][1] * vec[2];
  r[2] = x * mat[0][2] + y * mat[1][2] + mat[2][2] * vec[2];
}

void mul_v3_mat3_m4v3_db(double r[3], const double mat[4][4], const double vec[3])
{
  const double x = vec[0];
  const double y = vec[1];

  r[0] = x * mat[0][0] + y * mat[1][0] + mat[2][0] * vec[2];
  r[1] = x * mat[0][1] + y * mat[1][1] + mat[2][1] * vec[2];
  r[2] = x * mat[0][2] + y * mat[1][2] + mat[2][2] * vec[2];
}

void mul_project_m4_v3(const float mat[4][4], float vec[3])
{
  /* absolute value to not flip the frustum upside down behind the camera */
  const float w = fabsf(mul_project_m4_v3_zfac(mat, vec));
  mul_m4_v3(mat, vec);

  vec[0] /= w;
  vec[1] /= w;
  vec[2] /= w;
}

void mul_v3_project_m4_v3(float r[3], const float mat[4][4], const float vec[3])
{
  const float w = fabsf(mul_project_m4_v3_zfac(mat, vec));
  mul_v3_m4v3(r, mat, vec);

  r[0] /= w;
  r[1] /= w;
  r[2] /= w;
}

void mul_v2_project_m4_v3(float r[2], const float mat[4][4], const float vec[3])
{
  const float w = fabsf(mul_project_m4_v3_zfac(mat, vec));
  mul_v2_m4v3(r, mat, vec);

  r[0] /= w;
  r[1] /= w;
}

void mul_v4_m4v4(float r[4], const float mat[4][4], const float v[4])
{
  const float x = v[0];
  const float y = v[1];
  const float z = v[2];

  r[0] = x * mat[0][0] + y * mat[1][0] + z * mat[2][0] + mat[3][0] * v[3];
  r[1] = x * mat[0][1] + y * mat[1][1] + z * mat[2][1] + mat[3][1] * v[3];
  r[2] = x * mat[0][2] + y * mat[1][2] + z * mat[2][2] + mat[3][2] * v[3];
  r[3] = x * mat[0][3] + y * mat[1][3] + z * mat[2][3] + mat[3][3] * v[3];
}

void mul_m4_v4(const float mat[4][4], float r[4])
{
  mul_v4_m4v4(r, mat, r);
}

void mul_v4_m4v3(float r[4], const float M[4][4], const float v[3])
{
  /* v has implicit w = 1.0f */
  r[0] = v[0] * M[0][0] + v[1] * M[1][0] + M[2][0] * v[2] + M[3][0];
  r[1] = v[0] * M[0][1] + v[1] * M[1][1] + M[2][1] * v[2] + M[3][1];
  r[2] = v[0] * M[0][2] + v[1] * M[1][2] + M[2][2] * v[2] + M[3][2];
  r[3] = v[0] * M[0][3] + v[1] * M[1][3] + M[2][3] * v[2] + M[3][3];
}

void mul_v3_m3v3(float r[3], const float M[3][3], const float a[3])
{
  float t[3];
  copy_v3_v3(t, a);

  r[0] = M[0][0] * t[0] + M[1][0] * t[1] + M[2][0] * t[2];
  r[1] = M[0][1] * t[0] + M[1][1] * t[1] + M[2][1] * t[2];
  r[2] = M[0][2] * t[0] + M[1][2] * t[1] + M[2][2] * t[2];
}

void mul_v3_m3v3_db(double r[3], const double M[3][3], const double a[3])
{
  double t[3];
  copy_v3_v3_db(t, a);

  r[0] = M[0][0] * t[0] + M[1][0] * t[1] + M[2][0] * t[2];
  r[1] = M[0][1] * t[0] + M[1][1] * t[1] + M[2][1] * t[2];
  r[2] = M[0][2] * t[0] + M[1][2] * t[1] + M[2][2] * t[2];
}

void mul_v2_m3v3(float r[2], const float M[3][3], const float a[3])
{
  float t[3];
  copy_v3_v3(t, a);

  r[0] = M[0][0] * t[0] + M[1][0] * t[1] + M[2][0] * t[2];
  r[1] = M[0][1] * t[0] + M[1][1] * t[1] + M[2][1] * t[2];
}

void mul_m3_v3(const float M[3][3], float r[3])
{
  mul_v3_m3v3(r, M, r);
}

void mul_m3_v3_db(const double M[3][3], double r[3])
{
  mul_v3_m3v3_db(r, M, r);
}

void mul_transposed_m3_v3(const float M[3][3], float r[3])
{
  const float x = r[0];
  const float y = r[1];

  r[0] = x * M[0][0] + y * M[0][1] + M[0][2] * r[2];
  r[1] = x * M[1][0] + y * M[1][1] + M[1][2] * r[2];
  r[2] = x * M[2][0] + y * M[2][1] + M[2][2] * r[2];
}

void mul_transposed_mat3_m4_v3(const float M[4][4], float r[3])
{
  const float x = r[0];
  const float y = r[1];

  r[0] = x * M[0][0] + y * M[0][1] + M[0][2] * r[2];
  r[1] = x * M[1][0] + y * M[1][1] + M[1][2] * r[2];
  r[2] = x * M[2][0] + y * M[2][1] + M[2][2] * r[2];
}

void mul_m3_fl(float R[3][3], float f)
{
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      R[i][j] *= f;
    }
  }
}

void mul_m4_fl(float R[4][4], float f)
{
  int i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      R[i][j] *= f;
    }
  }
}

void mul_mat3_m4_fl(float R[4][4], float f)
{
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      R[i][j] *= f;
    }
  }
}

void negate_m3(float R[3][3])
{
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      R[i][j] *= -1.0f;
    }
  }
}

void negate_mat3_m4(float R[4][4])
{
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      R[i][j] *= -1.0f;
    }
  }
}

void negate_m4(float R[4][4])
{
  int i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      R[i][j] *= -1.0f;
    }
  }
}

void add_m3_m3m3(float R[3][3], const float A[3][3], const float B[3][3])
{
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      R[i][j] = A[i][j] + B[i][j];
    }
  }
}

void add_m4_m4m4(float R[4][4], const float A[4][4], const float B[4][4])
{
  int i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      R[i][j] = A[i][j] + B[i][j];
    }
  }
}

void madd_m3_m3m3fl(float R[3][3], const float A[3][3], const float B[3][3], const float f)
{
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      R[i][j] = A[i][j] + B[i][j] * f;
    }
  }
}

void madd_m4_m4m4fl(float R[4][4], const float A[4][4], const float B[4][4], const float f)
{
  int i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      R[i][j] = A[i][j] + B[i][j] * f;
    }
  }
}

void sub_m3_m3m3(float R[3][3], const float A[3][3], const float B[3][3])
{
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      R[i][j] = A[i][j] - B[i][j];
    }
  }
}

float determinant_m3_array(const float m[3][3])
{
  return (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
          m[1][0] * (m[0][1] * m[2][2] - m[0][2] * m[2][1]) +
          m[2][0] * (m[0][1] * m[1][2] - m[0][2] * m[1][1]));
}

float determinant_m4_mat3_array(const float m[4][4])
{
  return (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
          m[1][0] * (m[0][1] * m[2][2] - m[0][2] * m[2][1]) +
          m[2][0] * (m[0][1] * m[1][2] - m[0][2] * m[1][1]));
}

bool invert_m2_m2(float inverse[2][2], const float mat[2][2])
{
  const float det = determinant_m2(mat[0][0], mat[1][0], mat[0][1], mat[1][1]);
  adjoint_m2_m2(inverse, mat);

  bool success = (det != 0.0f);
  if (success) {
    inverse[0][0] /= det;
    inverse[1][0] /= det;
    inverse[0][1] /= det;
    inverse[1][1] /= det;
  }

  return success;
}

bool invert_m3(float mat[3][3])
{
  float mat_tmp[3][3];
  const bool success = invert_m3_m3(mat_tmp, mat);

  copy_m3_m3(mat, mat_tmp);
  return success;
}

bool invert_m3_m3(float inverse[3][3], const float mat[3][3])
{
  float det;
  int a, b;
  bool success;

  /* calc adjoint */
  adjoint_m3_m3(inverse, mat);

  /* then determinant old matrix! */
  det = determinant_m3_array(mat);

  success = (det != 0.0f);

  if (LIKELY(det != 0.0f)) {
    det = 1.0f / det;
    for (a = 0; a < 3; a++) {
      for (b = 0; b < 3; b++) {
        inverse[a][b] *= det;
      }
    }
  }

  return success;
}

bool invert_m4(float mat[4][4])
{
  float mat_tmp[4][4];
  const bool success = invert_m4_m4(mat_tmp, mat);

  copy_m4_m4(mat, mat_tmp);
  return success;
}

bool invert_m4_m4_fallback(float inverse[4][4], const float mat[4][4])
{
#ifndef MATH_STANDALONE
  if (EIG_invert_m4_m4(inverse, mat)) {
    return true;
  }
#endif

  int i, j, k;
  double temp;
  float tempmat[4][4];
  float max;
  int maxj;

  BLI_assert(inverse != mat);

  /* Set inverse to identity */
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      inverse[i][j] = 0;
    }
  }
  for (i = 0; i < 4; i++) {
    inverse[i][i] = 1;
  }

  /* Copy original matrix so we don't mess it up */
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      tempmat[i][j] = mat[i][j];
    }
  }

  for (i = 0; i < 4; i++) {
    /* Look for row with max pivot */
    max = fabsf(tempmat[i][i]);
    maxj = i;
    for (j = i + 1; j < 4; j++) {
      if (fabsf(tempmat[j][i]) > max) {
        max = fabsf(tempmat[j][i]);
        maxj = j;
      }
    }
    /* Swap rows if necessary */
    if (maxj != i) {
      for (k = 0; k < 4; k++) {
        SWAP(float, tempmat[i][k], tempmat[maxj][k]);
        SWAP(float, inverse[i][k], inverse[maxj][k]);
      }
    }

    if (UNLIKELY(tempmat[i][i] == 0.0f)) {
      return false; /* No non-zero pivot */
    }
    temp = double(tempmat[i][i]);
    for (k = 0; k < 4; k++) {
      tempmat[i][k] = float(double(tempmat[i][k]) / temp);
      inverse[i][k] = float(double(inverse[i][k]) / temp);
    }
    for (j = 0; j < 4; j++) {
      if (j != i) {
        temp = tempmat[j][i];
        for (k = 0; k < 4; k++) {
          tempmat[j][k] -= float(double(tempmat[i][k]) * temp);
          inverse[j][k] -= float(double(inverse[i][k]) * temp);
        }
      }
    }
  }
  return true;
}

bool invert_m4_m4(float inverse[4][4], const float mat[4][4])
{
#ifndef MATH_STANDALONE
  /* Use optimized matrix inverse from Eigen, since performance
   * impact of this function is significant in complex rigs. */
  return EIG_invert_m4_m4(inverse, mat);
#else
  return invert_m4_m4_fallback(inverse, mat);
#endif
}

void mul_m4_m4m4_aligned_scale(float R[4][4], const float A[4][4], const float B[4][4])
{
  float loc_a[3], rot_a[3][3], size_a[3];
  float loc_b[3], rot_b[3][3], size_b[3];
  float loc_r[3], rot_r[3][3], size_r[3];

  mat4_to_loc_rot_size(loc_a, rot_a, size_a, A);
  mat4_to_loc_rot_size(loc_b, rot_b, size_b, B);

  mul_v3_m4v3(loc_r, A, loc_b);
  mul_m3_m3m3(rot_r, rot_a, rot_b);
  mul_v3_v3v3(size_r, size_a, size_b);

  loc_rot_size_to_mat4(R, loc_r, rot_r, size_r);
}

void mul_m4_m4m4_split_channels(float R[4][4], const float A[4][4], const float B[4][4])
{
  float loc_a[3], rot_a[3][3], size_a[3];
  float loc_b[3], rot_b[3][3], size_b[3];
  float loc_r[3], rot_r[3][3], size_r[3];

  mat4_to_loc_rot_size(loc_a, rot_a, size_a, A);
  mat4_to_loc_rot_size(loc_b, rot_b, size_b, B);

  add_v3_v3v3(loc_r, loc_a, loc_b);
  mul_m3_m3m3(rot_r, rot_a, rot_b);
  mul_v3_v3v3(size_r, size_a, size_b);

  loc_rot_size_to_mat4(R, loc_r, rot_r, size_r);
}

/****************************** Linear Algebra *******************************/

void transpose_m3(float R[3][3])
{
  float t;

  t = R[0][1];
  R[0][1] = R[1][0];
  R[1][0] = t;
  t = R[0][2];
  R[0][2] = R[2][0];
  R[2][0] = t;
  t = R[1][2];
  R[1][2] = R[2][1];
  R[2][1] = t;
}

void transpose_m3_m3(float R[3][3], const float M[3][3])
{
  BLI_assert(R != M);

  R[0][0] = M[0][0];
  R[0][1] = M[1][0];
  R[0][2] = M[2][0];
  R[1][0] = M[0][1];
  R[1][1] = M[1][1];
  R[1][2] = M[2][1];
  R[2][0] = M[0][2];
  R[2][1] = M[1][2];
  R[2][2] = M[2][2];
}

void transpose_m3_m4(float R[3][3], const float M[4][4])
{
  BLI_assert(&R[0][0] != &M[0][0]);

  R[0][0] = M[0][0];
  R[0][1] = M[1][0];
  R[0][2] = M[2][0];
  R[1][0] = M[0][1];
  R[1][1] = M[1][1];
  R[1][2] = M[2][1];
  R[2][0] = M[0][2];
  R[2][1] = M[1][2];
  R[2][2] = M[2][2];
}

void transpose_m4(float R[4][4])
{
  float t;

  t = R[0][1];
  R[0][1] = R[1][0];
  R[1][0] = t;
  t = R[0][2];
  R[0][2] = R[2][0];
  R[2][0] = t;
  t = R[0][3];
  R[0][3] = R[3][0];
  R[3][0] = t;

  t = R[1][2];
  R[1][2] = R[2][1];
  R[2][1] = t;
  t = R[1][3];
  R[1][3] = R[3][1];
  R[3][1] = t;

  t = R[2][3];
  R[2][3] = R[3][2];
  R[3][2] = t;
}

void transpose_m4_m4(float R[4][4], const float M[4][4])
{
  BLI_assert(R != M);

  R[0][0] = M[0][0];
  R[0][1] = M[1][0];
  R[0][2] = M[2][0];
  R[0][3] = M[3][0];
  R[1][0] = M[0][1];
  R[1][1] = M[1][1];
  R[1][2] = M[2][1];
  R[1][3] = M[3][1];
  R[2][0] = M[0][2];
  R[2][1] = M[1][2];
  R[2][2] = M[2][2];
  R[2][3] = M[3][2];
  R[3][0] = M[0][3];
  R[3][1] = M[1][3];
  R[3][2] = M[2][3];
  R[3][3] = M[3][3];
}

bool compare_m4m4(const float mat1[4][4], const float mat2[4][4], float limit)
{
  if (compare_v4v4(mat1[0], mat2[0], limit)) {
    if (compare_v4v4(mat1[1], mat2[1], limit)) {
      if (compare_v4v4(mat1[2], mat2[2], limit)) {
        if (compare_v4v4(mat1[3], mat2[3], limit)) {
          return true;
        }
      }
    }
  }
  return false;
}

void orthogonalize_m3(float R[3][3], int axis)
{
  float size[3];
  mat3_to_size(size, R);
  normalize_v3(R[axis]);
  switch (axis) {
    case 0:
      if (dot_v3v3(R[0], R[1]) < 1) {
        cross_v3_v3v3(R[2], R[0], R[1]);
        normalize_v3(R[2]);
        cross_v3_v3v3(R[1], R[2], R[0]);
      }
      else if (dot_v3v3(R[0], R[2]) < 1) {
        cross_v3_v3v3(R[1], R[2], R[0]);
        normalize_v3(R[1]);
        cross_v3_v3v3(R[2], R[0], R[1]);
      }
      else {
        float vec[3];

        vec[0] = R[0][1];
        vec[1] = R[0][2];
        vec[2] = R[0][0];

        cross_v3_v3v3(R[2], R[0], vec);
        normalize_v3(R[2]);
        cross_v3_v3v3(R[1], R[2], R[0]);
      }
      break;
    case 1:
      if (dot_v3v3(R[1], R[0]) < 1) {
        cross_v3_v3v3(R[2], R[0], R[1]);
        normalize_v3(R[2]);
        cross_v3_v3v3(R[0], R[1], R[2]);
      }
      else if (dot_v3v3(R[0], R[2]) < 1) {
        cross_v3_v3v3(R[0], R[1], R[2]);
        normalize_v3(R[0]);
        cross_v3_v3v3(R[2], R[0], R[1]);
      }
      else {
        float vec[3];

        vec[0] = R[1][1];
        vec[1] = R[1][2];
        vec[2] = R[1][0];

        cross_v3_v3v3(R[0], R[1], vec);
        normalize_v3(R[0]);
        cross_v3_v3v3(R[2], R[0], R[1]);
      }
      break;
    case 2:
      if (dot_v3v3(R[2], R[0]) < 1) {
        cross_v3_v3v3(R[1], R[2], R[0]);
        normalize_v3(R[1]);
        cross_v3_v3v3(R[0], R[1], R[2]);
      }
      else if (dot_v3v3(R[2], R[1]) < 1) {
        cross_v3_v3v3(R[0], R[1], R[2]);
        normalize_v3(R[0]);
        cross_v3_v3v3(R[1], R[2], R[0]);
      }
      else {
        float vec[3];

        vec[0] = R[2][1];
        vec[1] = R[2][2];
        vec[2] = R[2][0];

        cross_v3_v3v3(R[0], vec, R[2]);
        normalize_v3(R[0]);
        cross_v3_v3v3(R[1], R[2], R[0]);
      }
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
  mul_v3_fl(R[0], size[0]);
  mul_v3_fl(R[1], size[1]);
  mul_v3_fl(R[2], size[2]);
}

void orthogonalize_m4(float R[4][4], int axis)
{
  float size[3];
  mat4_to_size(size, R);
  normalize_v3(R[axis]);
  switch (axis) {
    case 0:
      if (dot_v3v3(R[0], R[1]) < 1) {
        cross_v3_v3v3(R[2], R[0], R[1]);
        normalize_v3(R[2]);
        cross_v3_v3v3(R[1], R[2], R[0]);
      }
      else if (dot_v3v3(R[0], R[2]) < 1) {
        cross_v3_v3v3(R[1], R[2], R[0]);
        normalize_v3(R[1]);
        cross_v3_v3v3(R[2], R[0], R[1]);
      }
      else {
        float vec[3];

        vec[0] = R[0][1];
        vec[1] = R[0][2];
        vec[2] = R[0][0];

        cross_v3_v3v3(R[2], R[0], vec);
        normalize_v3(R[2]);
        cross_v3_v3v3(R[1], R[2], R[0]);
      }
      break;
    case 1:
      if (dot_v3v3(R[1], R[0]) < 1) {
        cross_v3_v3v3(R[2], R[0], R[1]);
        normalize_v3(R[2]);
        cross_v3_v3v3(R[0], R[1], R[2]);
      }
      else if (dot_v3v3(R[0], R[2]) < 1) {
        cross_v3_v3v3(R[0], R[1], R[2]);
        normalize_v3(R[0]);
        cross_v3_v3v3(R[2], R[0], R[1]);
      }
      else {
        float vec[3];

        vec[0] = R[1][1];
        vec[1] = R[1][2];
        vec[2] = R[1][0];

        cross_v3_v3v3(R[0], R[1], vec);
        normalize_v3(R[0]);
        cross_v3_v3v3(R[2], R[0], R[1]);
      }
      break;
    case 2:
      if (dot_v3v3(R[2], R[0]) < 1) {
        cross_v3_v3v3(R[1], R[2], R[0]);
        normalize_v3(R[1]);
        cross_v3_v3v3(R[0], R[1], R[2]);
      }
      else if (dot_v3v3(R[2], R[1]) < 1) {
        cross_v3_v3v3(R[0], R[1], R[2]);
        normalize_v3(R[0]);
        cross_v3_v3v3(R[1], R[2], R[0]);
      }
      else {
        float vec[3];

        vec[0] = R[2][1];
        vec[1] = R[2][2];
        vec[2] = R[2][0];

        cross_v3_v3v3(R[0], vec, R[2]);
        normalize_v3(R[0]);
        cross_v3_v3v3(R[1], R[2], R[0]);
      }
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
  mul_v3_fl(R[0], size[0]);
  mul_v3_fl(R[1], size[1]);
  mul_v3_fl(R[2], size[2]);
}

/** Make an orthonormal basis around v1 in a way that is stable and symmetric. */
static void orthogonalize_stable(float v1[3], float v2[3], float v3[3], bool normalize)
{
  /* Make secondary axis vectors orthogonal to the primary via
   * plane projection, which preserves the determinant. */
  float len_sq_v1 = len_squared_v3(v1);

  if (len_sq_v1 > 0.0f) {
    madd_v3_v3fl(v2, v1, -dot_v3v3(v2, v1) / len_sq_v1);
    madd_v3_v3fl(v3, v1, -dot_v3v3(v3, v1) / len_sq_v1);

    if (normalize) {
      mul_v3_fl(v1, 1.0f / sqrtf(len_sq_v1));
    }
  }

  /* Make secondary axis vectors orthogonal relative to each other. */
  float norm_v2[3], norm_v3[3], tmp[3];
  float length_v2 = normalize_v3_v3(norm_v2, v2);
  float length_v3 = normalize_v3_v3(norm_v3, v3);
  float cos_angle = dot_v3v3(norm_v2, norm_v3);
  float abs_cos_angle = fabsf(cos_angle);

  /* Apply correction if the shear angle is significant, and not degenerate. */
  if (abs_cos_angle > 1e-4f && abs_cos_angle < 1.0f - FLT_EPSILON) {
    /* Adjust v2 by half of the necessary angle correction.
     * Thus the angle change is the same for both axis directions. */
    float angle = acosf(cos_angle);
    float target_angle = angle + (float(M_PI_2) - angle) / 2;

    madd_v3_v3fl(norm_v2, norm_v3, -cos_angle);
    mul_v3_fl(norm_v2, sinf(target_angle) / len_v3(norm_v2));
    madd_v3_v3fl(norm_v2, norm_v3, cosf(target_angle));

    /* Make v3 orthogonal. */
    cross_v3_v3v3(tmp, norm_v2, norm_v3);
    cross_v3_v3v3(norm_v3, tmp, norm_v2);
    normalize_v3(norm_v3);

    /* Re-apply scale, preserving area and proportion. */
    if (!normalize) {
      float scale_fac = sqrtf(sinf(angle));
      mul_v3_v3fl(v2, norm_v2, length_v2 * scale_fac);
      mul_v3_v3fl(v3, norm_v3, length_v3 * scale_fac);
    }
  }

  if (normalize) {
    copy_v3_v3(v2, norm_v2);
    copy_v3_v3(v3, norm_v3);
  }
}

void orthogonalize_m4_stable(float R[4][4], int axis, bool normalize)
{
  switch (axis) {
    case 0:
      orthogonalize_stable(R[0], R[1], R[2], normalize);
      break;
    case 1:
      orthogonalize_stable(R[1], R[0], R[2], normalize);
      break;
    case 2:
      orthogonalize_stable(R[2], R[0], R[1], normalize);
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
}

/* -------------------------------------------------------------------- */
/** \name Orthogonalize Matrix Zeroed Axes
 *
 * Set any zeroed axes to an orthogonal vector in relation to the other axes.
 *
 * Typically used so matrix inversion can be performed.
 *
 * \note If an object has a zero scaled axis, this function can be used to "clean" the matrix
 * to behave as if the scale on that axis was `unit_length`. So it can be inverted
 * or used in matrix multiply without creating degenerate matrices, see: #50103
 * \{ */

/**
 * \return true if any axis needed to be modified.
 */
static bool orthogonalize_m3_zero_axes_impl(float *mat[3], const float unit_length)
{
  enum { X = 1 << 0, Y = 1 << 1, Z = 1 << 2 };
  int flag = 0;
  for (int i = 0; i < 3; i++) {
    flag |= (len_squared_v3(mat[i]) == 0.0f) ? (1 << i) : 0;
  }

  /* Either all or none are zero, either way we can't properly resolve this
   * since we need to fill invalid axes from valid ones. */
  if (ELEM(flag, 0, X | Y | Z)) {
    return false;
  }

  switch (flag) {
    case X | Y: {
      ortho_v3_v3(mat[1], mat[2]);
      ATTR_FALLTHROUGH;
    }
    case X: {
      cross_v3_v3v3(mat[0], mat[1], mat[2]);
      break;
    }

    case Y | Z: {
      ortho_v3_v3(mat[2], mat[0]);
      ATTR_FALLTHROUGH;
    }
    case Y: {
      cross_v3_v3v3(mat[1], mat[0], mat[2]);
      break;
    }

    case Z | X: {
      ortho_v3_v3(mat[0], mat[1]);
      ATTR_FALLTHROUGH;
    }
    case Z: {
      cross_v3_v3v3(mat[2], mat[0], mat[1]);
      break;
    }
    default: {
      BLI_assert_unreachable();
    }
  }

  for (int i = 0; i < 3; i++) {
    if (flag & (1 << i)) {
      if (UNLIKELY(normalize_v3_length(mat[i], unit_length) == 0.0f)) {
        mat[i][i] = unit_length;
      }
    }
  }

  return true;
}

bool orthogonalize_m3_zero_axes(float m[3][3], const float unit_length)
{
  float *unpacked[3] = {m[0], m[1], m[2]};
  return orthogonalize_m3_zero_axes_impl(unpacked, unit_length);
}
bool orthogonalize_m4_zero_axes(float m[4][4], const float unit_length)
{
  float *unpacked[3] = {m[0], m[1], m[2]};
  return orthogonalize_m3_zero_axes_impl(unpacked, unit_length);
}

/** \} */

bool is_orthogonal_m3(const float m[3][3])
{
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < i; j++) {
      if (fabsf(dot_v3v3(m[i], m[j])) > 1e-5f) {
        return false;
      }
    }
  }

  return true;
}

bool is_orthogonal_m4(const float m[4][4])
{
  int i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < i; j++) {
      if (fabsf(dot_v4v4(m[i], m[j])) > 1e-5f) {
        return false;
      }
    }
  }

  return true;
}

bool is_orthonormal_m3(const float m[3][3])
{
  if (is_orthogonal_m3(m)) {
    int i;

    for (i = 0; i < 3; i++) {
      if (fabsf(dot_v3v3(m[i], m[i]) - 1) > 1e-5f) {
        return false;
      }
    }

    return true;
  }

  return false;
}

bool is_orthonormal_m4(const float m[4][4])
{
  if (is_orthogonal_m4(m)) {
    int i;

    for (i = 0; i < 4; i++) {
      if (fabsf(dot_v4v4(m[i], m[i]) - 1) > 1e-5f) {
        return false;
      }
    }

    return true;
  }

  return false;
}

bool is_identity_m4(const float m[4][4])
{
  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 4; col++) {
      if (m[row][col] != (row == col ? 1.0f : 0.0f)) {
        return false;
      }
    }
  }

  return true;
}

bool is_uniform_scaled_m3(const float m[3][3])
{
  const float eps = 1e-7f;
  float t[3][3];
  float l1, l2, l3, l4, l5, l6;

  transpose_m3_m3(t, m);

  l1 = len_squared_v3(m[0]);
  l2 = len_squared_v3(m[1]);
  l3 = len_squared_v3(m[2]);

  l4 = len_squared_v3(t[0]);
  l5 = len_squared_v3(t[1]);
  l6 = len_squared_v3(t[2]);

  if (fabsf(l2 - l1) <= eps && fabsf(l3 - l1) <= eps && fabsf(l4 - l1) <= eps &&
      fabsf(l5 - l1) <= eps && fabsf(l6 - l1) <= eps)
  {
    return true;
  }

  return false;
}

bool is_uniform_scaled_m4(const float m[4][4])
{
  float t[3][3];
  copy_m3_m4(t, m);
  return is_uniform_scaled_m3(t);
}

void normalize_m2_m2(float R[2][2], const float M[2][2])
{
  int i;
  for (i = 0; i < 2; i++) {
    normalize_v2_v2(R[i], M[i]);
  }
}

void normalize_m3(float R[3][3])
{
  int i;
  for (i = 0; i < 3; i++) {
    normalize_v3(R[i]);
  }
}

void normalize_m3_m3(float R[3][3], const float M[3][3])
{
  int i;
  for (i = 0; i < 3; i++) {
    normalize_v3_v3(R[i], M[i]);
  }
}

void normalize_m4_ex(float R[4][4], float r_scale[3])
{
  int i;
  for (i = 0; i < 3; i++) {
    r_scale[i] = normalize_v3(R[i]);
    if (r_scale[i] != 0.0f) {
      R[i][3] /= r_scale[i];
    }
  }
}
void normalize_m4(float R[4][4])
{
  int i;
  for (i = 0; i < 3; i++) {
    float len = normalize_v3(R[i]);
    if (len != 0.0f) {
      R[i][3] /= len;
    }
  }
}

void normalize_m4_m4(float rmat[4][4], const float mat[4][4])
{
  int i;
  for (i = 0; i < 3; i++) {
    float len = normalize_v3_v3(rmat[i], mat[i]);
    rmat[i][3] = (len != 0.0f) ? (mat[i][3] / len) : mat[i][3];
  }
  copy_v4_v4(rmat[3], mat[3]);
}

void adjoint_m2_m2(float R[2][2], const float M[2][2])
{
  const float r00 = M[1][1];
  const float r01 = -M[0][1];
  const float r10 = -M[1][0];
  const float r11 = M[0][0];

  R[0][0] = r00;
  R[0][1] = r01;
  R[1][0] = r10;
  R[1][1] = r11;
}

void adjoint_m3_m3(float R[3][3], const float M[3][3])
{
  BLI_assert(R != M);
  R[0][0] = M[1][1] * M[2][2] - M[1][2] * M[2][1];
  R[0][1] = -M[0][1] * M[2][2] + M[0][2] * M[2][1];
  R[0][2] = M[0][1] * M[1][2] - M[0][2] * M[1][1];

  R[1][0] = -M[1][0] * M[2][2] + M[1][2] * M[2][0];
  R[1][1] = M[0][0] * M[2][2] - M[0][2] * M[2][0];
  R[1][2] = -M[0][0] * M[1][2] + M[0][2] * M[1][0];

  R[2][0] = M[1][0] * M[2][1] - M[1][1] * M[2][0];
  R[2][1] = -M[0][0] * M[2][1] + M[0][1] * M[2][0];
  R[2][2] = M[0][0] * M[1][1] - M[0][1] * M[1][0];
}

void adjoint_m4_m4(float R[4][4], const float M[4][4]) /* out = ADJ(in) */
{
  float a1, a2, a3, a4, b1, b2, b3, b4;
  float c1, c2, c3, c4, d1, d2, d3, d4;

  a1 = M[0][0];
  b1 = M[0][1];
  c1 = M[0][2];
  d1 = M[0][3];

  a2 = M[1][0];
  b2 = M[1][1];
  c2 = M[1][2];
  d2 = M[1][3];

  a3 = M[2][0];
  b3 = M[2][1];
  c3 = M[2][2];
  d3 = M[2][3];

  a4 = M[3][0];
  b4 = M[3][1];
  c4 = M[3][2];
  d4 = M[3][3];

  R[0][0] = determinant_m3(b2, b3, b4, c2, c3, c4, d2, d3, d4);
  R[1][0] = -determinant_m3(a2, a3, a4, c2, c3, c4, d2, d3, d4);
  R[2][0] = determinant_m3(a2, a3, a4, b2, b3, b4, d2, d3, d4);
  R[3][0] = -determinant_m3(a2, a3, a4, b2, b3, b4, c2, c3, c4);

  R[0][1] = -determinant_m3(b1, b3, b4, c1, c3, c4, d1, d3, d4);
  R[1][1] = determinant_m3(a1, a3, a4, c1, c3, c4, d1, d3, d4);
  R[2][1] = -determinant_m3(a1, a3, a4, b1, b3, b4, d1, d3, d4);
  R[3][1] = determinant_m3(a1, a3, a4, b1, b3, b4, c1, c3, c4);

  R[0][2] = determinant_m3(b1, b2, b4, c1, c2, c4, d1, d2, d4);
  R[1][2] = -determinant_m3(a1, a2, a4, c1, c2, c4, d1, d2, d4);
  R[2][2] = determinant_m3(a1, a2, a4, b1, b2, b4, d1, d2, d4);
  R[3][2] = -determinant_m3(a1, a2, a4, b1, b2, b4, c1, c2, c4);

  R[0][3] = -determinant_m3(b1, b2, b3, c1, c2, c3, d1, d2, d3);
  R[1][3] = determinant_m3(a1, a2, a3, c1, c2, c3, d1, d2, d3);
  R[2][3] = -determinant_m3(a1, a2, a3, b1, b2, b3, d1, d2, d3);
  R[3][3] = determinant_m3(a1, a2, a3, b1, b2, b3, c1, c2, c3);
}

float determinant_m2(const float a, const float b, const float c, const float d)
{
  return a * d - b * c;
}

float determinant_m3(
    float a1, float a2, float a3, float b1, float b2, float b3, float c1, float c2, float c3)
{
  float ans;

  ans = (a1 * determinant_m2(b2, b3, c2, c3) - b1 * determinant_m2(a2, a3, c2, c3) +
         c1 * determinant_m2(a2, a3, b2, b3));

  return ans;
}

float determinant_m4(const float m[4][4])
{
  float ans;
  float a1, a2, a3, a4, b1, b2, b3, b4, c1, c2, c3, c4, d1, d2, d3, d4;

  a1 = m[0][0];
  b1 = m[0][1];
  c1 = m[0][2];
  d1 = m[0][3];

  a2 = m[1][0];
  b2 = m[1][1];
  c2 = m[1][2];
  d2 = m[1][3];

  a3 = m[2][0];
  b3 = m[2][1];
  c3 = m[2][2];
  d3 = m[2][3];

  a4 = m[3][0];
  b4 = m[3][1];
  c4 = m[3][2];
  d4 = m[3][3];

  ans = (a1 * determinant_m3(b2, b3, b4, c2, c3, c4, d2, d3, d4) -
         b1 * determinant_m3(a2, a3, a4, c2, c3, c4, d2, d3, d4) +
         c1 * determinant_m3(a2, a3, a4, b2, b3, b4, d2, d3, d4) -
         d1 * determinant_m3(a2, a3, a4, b2, b3, b4, c2, c3, c4));

  return ans;
}

/****************************** Transformations ******************************/

void size_to_mat3(float R[3][3], const float size[3])
{
  R[0][0] = size[0];
  R[0][1] = 0.0f;
  R[0][2] = 0.0f;
  R[1][1] = size[1];
  R[1][0] = 0.0f;
  R[1][2] = 0.0f;
  R[2][2] = size[2];
  R[2][1] = 0.0f;
  R[2][0] = 0.0f;
}

void size_to_mat4(float R[4][4], const float size[3])
{
  R[0][0] = size[0];
  R[0][1] = 0.0f;
  R[0][2] = 0.0f;
  R[0][3] = 0.0f;
  R[1][0] = 0.0f;
  R[1][1] = size[1];
  R[1][2] = 0.0f;
  R[1][3] = 0.0f;
  R[2][0] = 0.0f;
  R[2][1] = 0.0f;
  R[2][2] = size[2];
  R[2][3] = 0.0f;
  R[3][0] = 0.0f;
  R[3][1] = 0.0f;
  R[3][2] = 0.0f;
  R[3][3] = 1.0f;
}

void mat3_to_size(float size[3], const float M[3][3])
{
  size[0] = len_v3(M[0]);
  size[1] = len_v3(M[1]);
  size[2] = len_v3(M[2]);
}

void mat4_to_size(float size[3], const float M[4][4])
{
  size[0] = len_v3(M[0]);
  size[1] = len_v3(M[1]);
  size[2] = len_v3(M[2]);
}

float mat4_to_size_max_axis(const float M[4][4])
{
  return sqrtf(max_fff(len_squared_v3(M[0]), len_squared_v3(M[1]), len_squared_v3(M[2])));
}

void mat4_to_size_fix_shear(float size[3], const float M[4][4])
{
  mat4_to_size(size, M);

  float volume = size[0] * size[1] * size[2];

  if (volume != 0.0f) {
    mul_v3_fl(size, cbrtf(fabsf(mat4_to_volume_scale(M) / volume)));
  }
}

float mat4_to_volume_scale(const float mat[4][4])
{
  return determinant_m4_mat3_array(mat);
}

float mat3_to_scale(const float mat[3][3])
{
  /* unit length vector */
  float unit_vec[3];
  copy_v3_fl(unit_vec, float(M_SQRT1_3));
  mul_m3_v3(mat, unit_vec);
  return len_v3(unit_vec);
}

float mat4_to_scale(const float mat[4][4])
{
  /* unit length vector */
  float unit_vec[3];
  copy_v3_fl(unit_vec, float(M_SQRT1_3));
  mul_mat3_m4_v3(mat, unit_vec);
  return len_v3(unit_vec);
}

void mat3_to_rot_size(float rot[3][3], float size[3], const float mat3[3][3])
{
  /* keep rot as a 3x3 matrix, the caller can convert into a quat or euler */
  size[0] = normalize_v3_v3(rot[0], mat3[0]);
  size[1] = normalize_v3_v3(rot[1], mat3[1]);
  size[2] = normalize_v3_v3(rot[2], mat3[2]);
  if (UNLIKELY(is_negative_m3(rot))) {
    negate_m3(rot);
    negate_v3(size);
  }
}

void mat4_to_loc_rot_size(float loc[3], float rot[3][3], float size[3], const float wmat[4][4])
{
  float mat3[3][3]; /* wmat -> 3x3 */

  copy_m3_m4(mat3, wmat);
  mat3_to_rot_size(rot, size, mat3);

  /* location */
  copy_v3_v3(loc, wmat[3]);
}

void mat4_to_loc_quat(float loc[3], float quat[4], const float wmat[4][4])
{
  float mat3[3][3];
  float mat3_n[3][3]; /* normalized mat3 */

  copy_m3_m4(mat3, wmat);
  normalize_m3_m3(mat3_n, mat3);

  mat3_normalized_to_quat(quat, mat3_n);
  copy_v3_v3(loc, wmat[3]);
}

void mat4_decompose(float loc[3], float quat[4], float size[3], const float wmat[4][4])
{
  float rot[3][3];
  mat4_to_loc_rot_size(loc, rot, size, wmat);
  mat3_normalized_to_quat_fast(quat, rot);
}

/**
 * Right polar decomposition:
 *     M = UP
 *
 * U is the *rotation*-like component, the closest orthogonal matrix to M.
 * P is the *scaling*-like component, defined in U space.
 *
 * See https://en.wikipedia.org/wiki/Polar_decomposition for more.
 */
#ifndef MATH_STANDALONE
void mat3_polar_decompose(const float mat3[3][3], float r_U[3][3], float r_P[3][3])
{
  /* From svd decomposition (M = WSV*), we have:
   *     U = WV*
   *     P = VSV*
   */
  float W[3][3], S[3][3], V[3][3], Vt[3][3];
  float sval[3];

  BLI_svd_m3(mat3, W, sval, V);

  size_to_mat3(S, sval);

  transpose_m3_m3(Vt, V);
  mul_m3_m3m3(r_U, W, Vt);
  mul_m3_series(r_P, V, S, Vt);
}
#endif

void scale_m3_fl(float R[3][3], float scale)
{
  R[0][0] = R[1][1] = R[2][2] = scale;
  R[0][1] = R[0][2] = 0.0;
  R[1][0] = R[1][2] = 0.0;
  R[2][0] = R[2][1] = 0.0;
}

void scale_m4_fl(float R[4][4], float scale)
{
  R[0][0] = R[1][1] = R[2][2] = scale;
  R[3][3] = 1.0;
  R[0][1] = R[0][2] = R[0][3] = 0.0;
  R[1][0] = R[1][2] = R[1][3] = 0.0;
  R[2][0] = R[2][1] = R[2][3] = 0.0;
  R[3][0] = R[3][1] = R[3][2] = 0.0;
}

void translate_m4(float mat[4][4], float Tx, float Ty, float Tz)
{
  mat[3][0] += (Tx * mat[0][0] + Ty * mat[1][0] + Tz * mat[2][0]);
  mat[3][1] += (Tx * mat[0][1] + Ty * mat[1][1] + Tz * mat[2][1]);
  mat[3][2] += (Tx * mat[0][2] + Ty * mat[1][2] + Tz * mat[2][2]);
}

void rotate_m4(float mat[4][4], const char axis, const float angle)
{
  const float angle_cos = cosf(angle);
  const float angle_sin = sinf(angle);

  BLI_assert(axis >= 'X' && axis <= 'Z');

  switch (axis) {
    case 'X':
      for (int col = 0; col < 4; col++) {
        float temp = angle_cos * mat[1][col] + angle_sin * mat[2][col];
        mat[2][col] = -angle_sin * mat[1][col] + angle_cos * mat[2][col];
        mat[1][col] = temp;
      }
      break;

    case 'Y':
      for (int col = 0; col < 4; col++) {
        float temp = angle_cos * mat[0][col] - angle_sin * mat[2][col];
        mat[2][col] = angle_sin * mat[0][col] + angle_cos * mat[2][col];
        mat[0][col] = temp;
      }
      break;

    case 'Z':
      for (int col = 0; col < 4; col++) {
        float temp = angle_cos * mat[0][col] + angle_sin * mat[1][col];
        mat[1][col] = -angle_sin * mat[0][col] + angle_cos * mat[1][col];
        mat[0][col] = temp;
      }
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
}

void rescale_m4(float mat[4][4], const float scale[3])
{
  mul_v3_fl(mat[0], scale[0]);
  mul_v3_fl(mat[1], scale[1]);
  mul_v3_fl(mat[2], scale[2]);
}

void transform_pivot_set_m4(float mat[4][4], const float pivot[3])
{
  float tmat[4][4];

  unit_m4(tmat);

  copy_v3_v3(tmat[3], pivot);
  mul_m4_m4m4(mat, tmat, mat);

  /* invert the matrix */
  negate_v3(tmat[3]);
  mul_m4_m4m4(mat, mat, tmat);
}

void blend_m3_m3m3(float out[3][3],
                   const float dst[3][3],
                   const float src[3][3],
                   const float srcweight)
{
  float srot[3][3], drot[3][3];
  float squat[4], dquat[4], fquat[4];
  float sscale[3], dscale[3], fsize[3];
  float rmat[3][3], smat[3][3];

  mat3_to_rot_size(drot, dscale, dst);
  mat3_to_rot_size(srot, sscale, src);

  mat3_normalized_to_quat_fast(dquat, drot);
  mat3_normalized_to_quat_fast(squat, srot);

  /* do blending */
  interp_qt_qtqt(fquat, dquat, squat, srcweight);
  interp_v3_v3v3(fsize, dscale, sscale, srcweight);

  /* compose new matrix */
  quat_to_mat3(rmat, fquat);
  size_to_mat3(smat, fsize);
  mul_m3_m3m3(out, rmat, smat);
}

void blend_m4_m4m4(float out[4][4],
                   const float dst[4][4],
                   const float src[4][4],
                   const float srcweight)
{
  float sloc[3], dloc[3], floc[3];
  float srot[3][3], drot[3][3];
  float squat[4], dquat[4], fquat[4];
  float sscale[3], dscale[3], fsize[3];

  mat4_to_loc_rot_size(dloc, drot, dscale, dst);
  mat4_to_loc_rot_size(sloc, srot, sscale, src);

  mat3_normalized_to_quat_fast(dquat, drot);
  mat3_normalized_to_quat_fast(squat, srot);

  /* do blending */
  interp_v3_v3v3(floc, dloc, sloc, srcweight);
  interp_qt_qtqt(fquat, dquat, squat, srcweight);
  interp_v3_v3v3(fsize, dscale, sscale, srcweight);

  /* compose new matrix */
  loc_quat_size_to_mat4(out, floc, fquat, fsize);
}

/* for builds without Eigen */
#ifndef MATH_STANDALONE
void interp_m3_m3m3(float R[3][3], const float A[3][3], const float B[3][3], const float t)
{
  /* 'Rotation' component ('U' part of polar decomposition,
   * the closest orthogonal matrix to M3 rot/scale
   * transformation matrix), spherically interpolated. */
  float U_A[3][3], U_B[3][3], U[3][3];
  float quat_A[4], quat_B[4], quat[4];
  /* 'Scaling' component ('P' part of polar decomposition, i.e. scaling in U-defined space),
   * linearly interpolated. */
  float P_A[3][3], P_B[3][3], P[3][3];

  int i;

  mat3_polar_decompose(A, U_A, P_A);
  mat3_polar_decompose(B, U_B, P_B);

  /* Quaternions cannot represent an axis flip. If such a singularity is detected, choose a
   * different decomposition of the matrix that still satisfies A = U_A * P_A but which has a
   * positive determinant and thus no axis flips. This resolves #77154.
   *
   * Note that a flip of two axes is just a rotation of 180 degrees around the third axis, and
   * three flipped axes are just an 180 degree rotation + a single axis flip. It is thus sufficient
   * to solve this problem for single axis flips. */
  if (is_negative_m3(U_A)) {
    mul_m3_fl(U_A, -1.0f);
    mul_m3_fl(P_A, -1.0f);
  }
  if (is_negative_m3(U_B)) {
    mul_m3_fl(U_B, -1.0f);
    mul_m3_fl(P_B, -1.0f);
  }

  mat3_to_quat(quat_A, U_A);
  mat3_to_quat(quat_B, U_B);
  interp_qt_qtqt(quat, quat_A, quat_B, t);
  quat_to_mat3(U, quat);

  for (i = 0; i < 3; i++) {
    interp_v3_v3v3(P[i], P_A[i], P_B[i], t);
  }

  /* And we reconstruct rot/scale matrix from interpolated polar components */
  mul_m3_m3m3(R, U, P);
}

void interp_m4_m4m4(float R[4][4], const float A[4][4], const float B[4][4], const float t)
{
  float A3[3][3], B3[3][3], R3[3][3];

  /* Location component, linearly interpolated. */
  float loc_A[3], loc_B[3], loc[3];

  copy_v3_v3(loc_A, A[3]);
  copy_v3_v3(loc_B, B[3]);
  interp_v3_v3v3(loc, loc_A, loc_B, t);

  copy_m3_m4(A3, A);
  copy_m3_m4(B3, B);

  interp_m3_m3m3(R3, A3, B3, t);

  copy_m4_m3(R, R3);
  copy_v3_v3(R[3], loc);
}
#endif /* MATH_STANDALONE */

bool is_negative_m3(const float mat[3][3])
{
  return determinant_m3_array(mat) < 0.0f;
}

bool is_negative_m4(const float mat[4][4])
{
  /* Don't use #determinant_m4 as only the 3x3 components are needed
   * when the matrix is used as a transformation to represent location/scale/rotation. */
  return determinant_m4_mat3_array(mat) < 0.0f;
}

bool is_zero_m4(const float mat[4][4])
{
  return (is_zero_v4(mat[0]) && is_zero_v4(mat[1]) && is_zero_v4(mat[2]) && is_zero_v4(mat[3]));
}

bool equals_m3m3(const float mat1[3][3], const float mat2[3][3])
{
  return (equals_v3v3(mat1[0], mat2[0]) && equals_v3v3(mat1[1], mat2[1]) &&
          equals_v3v3(mat1[2], mat2[2]));
}

bool equals_m4m4(const float mat1[4][4], const float mat2[4][4])
{
  return (equals_v4v4(mat1[0], mat2[0]) && equals_v4v4(mat1[1], mat2[1]) &&
          equals_v4v4(mat1[2], mat2[2]) && equals_v4v4(mat1[3], mat2[3]));
}

void loc_rot_size_to_mat4(float R[4][4],
                          const float loc[3],
                          const float rot[3][3],
                          const float size[3])
{
  copy_m4_m3(R, rot);
  rescale_m4(R, size);
  copy_v3_v3(R[3], loc);
}

void loc_eul_size_to_mat4(float R[4][4],
                          const float loc[3],
                          const float eul[3],
                          const float size[3])
{
  float rmat[3][3], smat[3][3], tmat[3][3];

  /* initialize new matrix */
  unit_m4(R);

  /* make rotation + scaling part */
  eul_to_mat3(rmat, eul);
  size_to_mat3(smat, size);
  mul_m3_m3m3(tmat, rmat, smat);

  /* Copy rot/scale part to output matrix. */
  copy_m4_m3(R, tmat);

  /* copy location to matrix */
  R[3][0] = loc[0];
  R[3][1] = loc[1];
  R[3][2] = loc[2];
}

void loc_eulO_size_to_mat4(
    float R[4][4], const float loc[3], const float eul[3], const float size[3], const short order)
{
  float rmat[3][3], smat[3][3], tmat[3][3];

  /* Initialize new matrix. */
  unit_m4(R);

  /* Make rotation + scaling part. */
  eulO_to_mat3(rmat, eul, order);
  size_to_mat3(smat, size);
  mul_m3_m3m3(tmat, rmat, smat);

  /* Copy rot/scale part to output matrix. */
  copy_m4_m3(R, tmat);

  /* Copy location to matrix. */
  R[3][0] = loc[0];
  R[3][1] = loc[1];
  R[3][2] = loc[2];
}

void loc_quat_size_to_mat4(float R[4][4],
                           const float loc[3],
                           const float quat[4],
                           const float size[3])
{
  float rmat[3][3], smat[3][3], tmat[3][3];

  /* initialize new matrix */
  unit_m4(R);

  /* make rotation + scaling part */
  quat_to_mat3(rmat, quat);
  size_to_mat3(smat, size);
  mul_m3_m3m3(tmat, rmat, smat);

  /* Copy rot/scale part to output matrix. */
  copy_m4_m3(R, tmat);

  /* copy location to matrix */
  R[3][0] = loc[0];
  R[3][1] = loc[1];
  R[3][2] = loc[2];
}

/*********************************** Other ***********************************/

void print_m3(const char *str, const float m[3][3])
{
  printf("%s\n", str);
  printf("%f %f %f\n", m[0][0], m[1][0], m[2][0]);
  printf("%f %f %f\n", m[0][1], m[1][1], m[2][1]);
  printf("%f %f %f\n", m[0][2], m[1][2], m[2][2]);
  printf("\n");
}

void print_m4(const char *str, const float m[4][4])
{
  printf("%s\n", str);
  printf("%f %f %f %f\n", m[0][0], m[1][0], m[2][0], m[3][0]);
  printf("%f %f %f %f\n", m[0][1], m[1][1], m[2][1], m[3][1]);
  printf("%f %f %f %f\n", m[0][2], m[1][2], m[2][2], m[3][2]);
  printf("%f %f %f %f\n", m[0][3], m[1][3], m[2][3], m[3][3]);
  printf("\n");
}

void svd_m4(float U[4][4], float s[4], float V[4][4], float A_[4][4])
{
  /* NOTE: originally from TNT (template numeric toolkit) matrix library.
   * https://math.nist.gov/tnt */

  float A[4][4];
  float work1[4], work2[4];
  int m = 4;
  int n = 4;
  int maxiter = 200;
  int nu = min_ii(m, n);

  float *work = work1;
  float *e = work2;
  float eps;

  int i = 0, j = 0, k = 0, p, pp, iter;

  /* Reduce A to bidiagonal form, storing the diagonal elements
   * in s and the super-diagonal elements in e. */

  int nct = min_ii(m - 1, n);
  int nrt = max_ii(0, min_ii(n - 2, m));

  copy_m4_m4(A, A_);
  zero_m4(U);
  zero_v4(s);

  for (k = 0; k < max_ii(nct, nrt); k++) {
    if (k < nct) {

      /* Compute the transformation for the k-th column and
       * place the k-th diagonal in s[k].
       * Compute 2-norm of k-th column without under/overflow. */
      s[k] = 0;
      for (i = k; i < m; i++) {
        s[k] = hypotf(s[k], A[i][k]);
      }
      if (s[k] != 0.0f) {
        float invsk;
        if (A[k][k] < 0.0f) {
          s[k] = -s[k];
        }
        invsk = 1.0f / s[k];
        for (i = k; i < m; i++) {
          A[i][k] *= invsk;
        }
        A[k][k] += 1.0f;
      }
      s[k] = -s[k];
    }
    for (j = k + 1; j < n; j++) {
      if ((k < nct) && (s[k] != 0.0f)) {

        /* Apply the transformation. */

        float t = 0;
        for (i = k; i < m; i++) {
          t += A[i][k] * A[i][j];
        }
        t = -t / A[k][k];
        for (i = k; i < m; i++) {
          A[i][j] += t * A[i][k];
        }
      }

      /* Place the k-th row of A into e for the */
      /* subsequent calculation of the row transformation. */

      e[j] = A[k][j];
    }
    if (k < nct) {

      /* Place the transformation in U for subsequent back
       * multiplication. */

      for (i = k; i < m; i++) {
        U[i][k] = A[i][k];
      }
    }
    if (k < nrt) {

      /* Compute the k-th row transformation and place the
       * k-th super-diagonal in e[k].
       * Compute 2-norm without under/overflow. */
      e[k] = 0;
      for (i = k + 1; i < n; i++) {
        e[k] = hypotf(e[k], e[i]);
      }
      if (e[k] != 0.0f) {
        float invek;
        if (e[k + 1] < 0.0f) {
          e[k] = -e[k];
        }
        invek = 1.0f / e[k];
        for (i = k + 1; i < n; i++) {
          e[i] *= invek;
        }
        e[k + 1] += 1.0f;
      }
      e[k] = -e[k];
      if ((k + 1 < m) && (e[k] != 0.0f)) {
        float invek1;

        /* Apply the transformation. */

        for (i = k + 1; i < m; i++) {
          work[i] = 0.0f;
        }
        for (j = k + 1; j < n; j++) {
          for (i = k + 1; i < m; i++) {
            work[i] += e[j] * A[i][j];
          }
        }
        invek1 = 1.0f / e[k + 1];
        for (j = k + 1; j < n; j++) {
          float t = -e[j] * invek1;
          for (i = k + 1; i < m; i++) {
            A[i][j] += t * work[i];
          }
        }
      }

      /* Place the transformation in V for subsequent
       * back multiplication. */

      for (i = k + 1; i < n; i++) {
        V[i][k] = e[i];
      }
    }
  }

  /* Set up the final bidiagonal matrix or order p. */

  p = min_ii(n, m + 1);
  if (nct < n) {
    s[nct] = A[nct][nct];
  }
  if (m < p) {
    s[p - 1] = 0.0f;
  }
  if (nrt + 1 < p) {
    e[nrt] = A[nrt][p - 1];
  }
  e[p - 1] = 0.0f;

  /* If required, generate U. */

  for (j = nct; j < nu; j++) {
    for (i = 0; i < m; i++) {
      U[i][j] = 0.0f;
    }
    U[j][j] = 1.0f;
  }
  for (k = nct - 1; k >= 0; k--) {
    if (s[k] != 0.0f) {
      for (j = k + 1; j < nu; j++) {
        float t = 0;
        for (i = k; i < m; i++) {
          t += U[i][k] * U[i][j];
        }
        t = -t / U[k][k];
        for (i = k; i < m; i++) {
          U[i][j] += t * U[i][k];
        }
      }
      for (i = k; i < m; i++) {
        U[i][k] = -U[i][k];
      }
      U[k][k] = 1.0f + U[k][k];
      for (i = 0; i < k - 1; i++) {
        U[i][k] = 0.0f;
      }
    }
    else {
      for (i = 0; i < m; i++) {
        U[i][k] = 0.0f;
      }
      U[k][k] = 1.0f;
    }
  }

  /* If required, generate V. */

  for (k = n - 1; k >= 0; k--) {
    if ((k < nrt) && (e[k] != 0.0f)) {
      for (j = k + 1; j < nu; j++) {
        float t = 0;
        for (i = k + 1; i < n; i++) {
          t += V[i][k] * V[i][j];
        }
        t = -t / V[k + 1][k];
        for (i = k + 1; i < n; i++) {
          V[i][j] += t * V[i][k];
        }
      }
    }
    for (i = 0; i < n; i++) {
      V[i][k] = 0.0f;
    }
    V[k][k] = 1.0f;
  }

  /* Main iteration loop for the singular values. */

  pp = p - 1;
  iter = 0;
  eps = powf(2.0f, -52.0f);
  while (p > 0) {
    int kase = 0;

    /* Test for maximum iterations to avoid infinite loop */
    if (maxiter == 0) {
      break;
    }
    maxiter--;

    /* This section of the program inspects for
     * negligible elements in the s and e arrays.  On
     * completion the variables kase and k are set as follows.
     *
     * kase = 1: if s(p) and e[k - 1] are negligible and k<p
     * kase = 2: if s(k) is negligible and k<p
     * kase = 3: if e[k - 1] is negligible, k<p, and
     *              s(k), ..., s(p) are not negligible (qr step).
     * kase = 4: if e(p - 1) is negligible (convergence). */

    for (k = p - 2; k >= -1; k--) {
      if (k == -1) {
        break;
      }
      if (fabsf(e[k]) <= eps * (fabsf(s[k]) + fabsf(s[k + 1]))) {
        e[k] = 0.0f;
        break;
      }
    }
    if (k == p - 2) {
      kase = 4;
    }
    else {
      int ks;
      for (ks = p - 1; ks >= k; ks--) {
        float t;
        if (ks == k) {
          break;
        }
        t = (ks != p ? fabsf(e[ks]) : 0.0f) + (ks != k + 1 ? fabsf(e[ks - 1]) : 0.0f);
        if (fabsf(s[ks]) <= eps * t) {
          s[ks] = 0.0f;
          break;
        }
      }
      if (ks == k) {
        kase = 3;
      }
      else if (ks == p - 1) {
        kase = 1;
      }
      else {
        kase = 2;
        k = ks;
      }
    }
    k++;

    /* Perform the task indicated by kase. */

    switch (kase) {

        /* Deflate negligible s(p). */

      case 1: {
        float f = e[p - 2];
        e[p - 2] = 0.0f;
        for (j = p - 2; j >= k; j--) {
          float t = hypotf(s[j], f);
          float invt = 1.0f / t;
          float cs = s[j] * invt;
          float sn = f * invt;
          s[j] = t;
          if (j != k) {
            f = -sn * e[j - 1];
            e[j - 1] = cs * e[j - 1];
          }

          for (i = 0; i < n; i++) {
            t = cs * V[i][j] + sn * V[i][p - 1];
            V[i][p - 1] = -sn * V[i][j] + cs * V[i][p - 1];
            V[i][j] = t;
          }
        }
        break;
      }

        /* Split at negligible s(k). */

      case 2: {
        float f = e[k - 1];
        e[k - 1] = 0.0f;
        for (j = k; j < p; j++) {
          float t = hypotf(s[j], f);
          float invt = 1.0f / t;
          float cs = s[j] * invt;
          float sn = f * invt;
          s[j] = t;
          f = -sn * e[j];
          e[j] = cs * e[j];

          for (i = 0; i < m; i++) {
            t = cs * U[i][j] + sn * U[i][k - 1];
            U[i][k - 1] = -sn * U[i][j] + cs * U[i][k - 1];
            U[i][j] = t;
          }
        }
        break;
      }

        /* Perform one qr step. */

      case 3: {

        /* Calculate the shift. */

        float scale = max_ff(
            max_ff(max_ff(max_ff(fabsf(s[p - 1]), fabsf(s[p - 2])), fabsf(e[p - 2])), fabsf(s[k])),
            fabsf(e[k]));
        float invscale = 1.0f / scale;
        float sp = s[p - 1] * invscale;
        float spm1 = s[p - 2] * invscale;
        float epm1 = e[p - 2] * invscale;
        float sk = s[k] * invscale;
        float ek = e[k] * invscale;
        float b = ((spm1 + sp) * (spm1 - sp) + epm1 * epm1) * 0.5f;
        float c = (sp * epm1) * (sp * epm1);
        float shift = 0.0f;
        float f, g;
        if ((b != 0.0f) || (c != 0.0f)) {
          shift = sqrtf(b * b + c);
          if (b < 0.0f) {
            shift = -shift;
          }
          shift = c / (b + shift);
        }
        f = (sk + sp) * (sk - sp) + shift;
        g = sk * ek;

        /* Chase zeros. */

        for (j = k; j < p - 1; j++) {
          float t = hypotf(f, g);
          /* NOTE(@brecht): division by zero checks added to avoid NaN. */
          float cs = (t == 0.0f) ? 0.0f : f / t;
          float sn = (t == 0.0f) ? 0.0f : g / t;
          if (j != k) {
            e[j - 1] = t;
          }
          f = cs * s[j] + sn * e[j];
          e[j] = cs * e[j] - sn * s[j];
          g = sn * s[j + 1];
          s[j + 1] = cs * s[j + 1];

          for (i = 0; i < n; i++) {
            t = cs * V[i][j] + sn * V[i][j + 1];
            V[i][j + 1] = -sn * V[i][j] + cs * V[i][j + 1];
            V[i][j] = t;
          }

          t = hypotf(f, g);
          /* NOTE(@brecht): division by zero checks added to avoid NaN. */
          cs = (t == 0.0f) ? 0.0f : f / t;
          sn = (t == 0.0f) ? 0.0f : g / t;
          s[j] = t;
          f = cs * e[j] + sn * s[j + 1];
          s[j + 1] = -sn * e[j] + cs * s[j + 1];
          g = sn * e[j + 1];
          e[j + 1] = cs * e[j + 1];
          if (j < m - 1) {
            for (i = 0; i < m; i++) {
              t = cs * U[i][j] + sn * U[i][j + 1];
              U[i][j + 1] = -sn * U[i][j] + cs * U[i][j + 1];
              U[i][j] = t;
            }
          }
        }
        e[p - 2] = f;
        iter = iter + 1;
        break;
      }
        /* Convergence. */

      case 4: {

        /* Make the singular values positive. */

        if (s[k] <= 0.0f) {
          s[k] = (s[k] < 0.0f ? -s[k] : 0.0f);

          for (i = 0; i <= pp; i++) {
            V[i][k] = -V[i][k];
          }
        }

        /* Order the singular values. */

        while (k < pp) {
          float t;
          if (s[k] >= s[k + 1]) {
            break;
          }
          t = s[k];
          s[k] = s[k + 1];
          s[k + 1] = t;
          if (k < n - 1) {
            for (i = 0; i < n; i++) {
              t = V[i][k + 1];
              V[i][k + 1] = V[i][k];
              V[i][k] = t;
            }
          }
          if (k < m - 1) {
            for (i = 0; i < m; i++) {
              t = U[i][k + 1];
              U[i][k + 1] = U[i][k];
              U[i][k] = t;
            }
          }
          k++;
        }
        iter = 0;
        p--;
        break;
      }
    }
  }
}

void pseudoinverse_m4_m4(float inverse[4][4], const float mat[4][4], float epsilon)
{
  /* compute Moore-Penrose pseudo inverse of matrix, singular values
   * below epsilon are ignored for stability (truncated SVD) */
  float A[4][4], V[4][4], W[4], Wm[4][4], U[4][4];
  int i;

  transpose_m4_m4(A, mat);
  svd_m4(V, W, U, A);
  transpose_m4(U);
  transpose_m4(V);

  zero_m4(Wm);
  for (i = 0; i < 4; i++) {
    Wm[i][i] = (W[i] < epsilon) ? 0.0f : 1.0f / W[i];
  }

  transpose_m4(V);

  mul_m4_series(inverse, U, Wm, V);
}

void pseudoinverse_m3_m3(float inverse[3][3], const float mat[3][3], float epsilon)
{
  /* try regular inverse when possible, otherwise fall back to slow svd */
  if (!invert_m3_m3(inverse, mat)) {
    float mat_tmp[4][4], tmpinv[4][4];

    copy_m4_m3(mat_tmp, mat);
    pseudoinverse_m4_m4(tmpinv, mat_tmp, epsilon);
    copy_m3_m4(inverse, tmpinv);
  }
}

bool has_zero_axis_m4(const float matrix[4][4])
{
  return len_squared_v3(matrix[0]) < FLT_EPSILON || len_squared_v3(matrix[1]) < FLT_EPSILON ||
         len_squared_v3(matrix[2]) < FLT_EPSILON;
}

void invert_m4_m4_safe(float inverse[4][4], const float mat[4][4])
{
  if (!invert_m4_m4(inverse, mat)) {
    float mat_tmp[4][4];

    copy_m4_m4(mat_tmp, mat);

    /* Matrix is degenerate (e.g. 0 scale on some axis), ideally we should
     * never be in this situation, but try to invert it anyway with tweak.
     */
    mat_tmp[0][0] += 1e-8f;
    mat_tmp[1][1] += 1e-8f;
    mat_tmp[2][2] += 1e-8f;

    if (!invert_m4_m4(inverse, mat_tmp)) {
      unit_m4(inverse);
    }
  }
}

/* -------------------------------------------------------------------- */
/** \name Invert (Safe Orthographic)
 *
 * Invert the matrix, filling in zeroed axes using the valid ones where possible.
 *
 * Unlike #invert_m4_m4_safe set degenerate axis unit length instead of adding a small value,
 * which has the results in:
 *
 * - Scaling by a large value on the resulting matrix.
 * - Changing axis which aren't degenerate.
 *
 * \note We could support passing in a length value if there is a good use-case
 * where we want to specify the length of the degenerate axes.
 * \{ */

void invert_m4_m4_safe_ortho(float inverse[4][4], const float mat[4][4])
{
  if (UNLIKELY(!invert_m4_m4(inverse, mat))) {
    float mat_tmp[4][4];
    copy_m4_m4(mat_tmp, mat);
    if (UNLIKELY(!(orthogonalize_m4_zero_axes(mat_tmp, 1.0f) && invert_m4_m4(inverse, mat_tmp)))) {
      unit_m4(inverse);
    }
  }
}

void invert_m3_m3_safe_ortho(float inverse[3][3], const float mat[3][3])
{
  if (UNLIKELY(!invert_m3_m3(inverse, mat))) {
    float mat_tmp[3][3];
    copy_m3_m3(mat_tmp, mat);
    if (UNLIKELY(!(orthogonalize_m3_zero_axes(mat_tmp, 1.0f) && invert_m3_m3(inverse, mat_tmp)))) {
      unit_m3(inverse);
    }
  }
}

/** \} */

void BLI_space_transform_from_matrices(SpaceTransform *data,
                                       const float local[4][4],
                                       const float target[4][4])
{
  float itarget[4][4];
  invert_m4_m4(itarget, target);
  mul_m4_m4m4(data->local2target, itarget, local);
  invert_m4_m4(data->target2local, data->local2target);
}

void BLI_space_transform_global_from_matrices(SpaceTransform *data,
                                              const float local[4][4],
                                              const float target[4][4])
{
  float ilocal[4][4];
  invert_m4_m4(ilocal, local);
  mul_m4_m4m4(data->local2target, target, ilocal);
  invert_m4_m4(data->target2local, data->local2target);
}

void BLI_space_transform_apply(const SpaceTransform *data, float co[3])
{
  mul_v3_m4v3(co, ((SpaceTransform *)data)->local2target, co);
}

void BLI_space_transform_invert(const SpaceTransform *data, float co[3])
{
  mul_v3_m4v3(co, ((SpaceTransform *)data)->target2local, co);
}

void BLI_space_transform_apply_normal(const SpaceTransform *data, float no[3])
{
  mul_mat3_m4_v3(((SpaceTransform *)data)->local2target, no);
  normalize_v3(no);
}

void BLI_space_transform_invert_normal(const SpaceTransform *data, float no[3])
{
  mul_mat3_m4_v3(((SpaceTransform *)data)->target2local, no);
  normalize_v3(no);
}
