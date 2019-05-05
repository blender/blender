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

/** \file
 * \ingroup bli
 */

#include <assert.h>
#include "BLI_math.h"

#include "BLI_strict_flags.h"

#include "eigen_capi.h"

/********************************* Init **************************************/

void zero_m2(float m[2][2])
{
  memset(m, 0, sizeof(float[2][2]));
}

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

  /*  Reevan's Bugfix */
  m1[0][3] = 0.0f;
  m1[1][3] = 0.0f;
  m1[2][3] = 0.0f;

  m1[3][0] = 0.0f;
  m1[3][1] = 0.0f;
  m1[3][2] = 0.0f;
  m1[3][3] = 1.0f;
}

void copy_m3_m3d(float R[3][3], const double A[3][3])
{
  /* Keep it stupid simple for better data flow in CPU. */
  R[0][0] = (float)A[0][0];
  R[0][1] = (float)A[0][1];
  R[0][2] = (float)A[0][2];

  R[1][0] = (float)A[1][0];
  R[1][1] = (float)A[1][1];
  R[1][2] = (float)A[1][2];

  R[2][0] = (float)A[2][0];
  R[2][1] = (float)A[2][1];
  R[2][2] = (float)A[2][2];
}

void swap_m3m3(float m1[3][3], float m2[3][3])
{
  float t;
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      t = m1[i][j];
      m1[i][j] = m2[i][j];
      m2[i][j] = t;
    }
  }
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

/******************************** Arithmetic *********************************/

void mul_m4_m4m4(float R[4][4], const float A[4][4], const float B[4][4])
{
  if (A == R) {
    mul_m4_m4_post(R, B);
  }
  else if (B == R) {
    mul_m4_m4_pre(R, A);
  }
  else {
    mul_m4_m4m4_uniq(R, A, B);
  }
}

void mul_m4_m4m4_uniq(float R[4][4], const float A[4][4], const float B[4][4])
{
  BLI_assert(R != A && R != B);

  /* matrix product: R[j][k] = A[j][i] . B[i][k] */
#ifdef __SSE2__
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

void mul_m4_m4_pre(float R[4][4], const float A[4][4])
{
  BLI_assert(A != R);
  float B[4][4];
  copy_m4_m4(B, R);
  mul_m4_m4m4_uniq(R, A, B);
}

void mul_m4_m4_post(float R[4][4], const float B[4][4])
{
  BLI_assert(B != R);
  float A[4][4];
  copy_m4_m4(A, R);
  mul_m4_m4m4_uniq(R, A, B);
}

void mul_m3_m3m3(float R[3][3], const float A[3][3], const float B[3][3])
{
  if (A == R) {
    mul_m3_m3_post(R, B);
  }
  else if (B == R) {
    mul_m3_m3_pre(R, A);
  }
  else {
    mul_m3_m3m3_uniq(R, A, B);
  }
}

void mul_m3_m3_pre(float R[3][3], const float A[3][3])
{
  BLI_assert(A != R);
  float B[3][3];
  copy_m3_m3(B, R);
  mul_m3_m3m3_uniq(R, A, B);
}

void mul_m3_m3_post(float R[3][3], const float B[3][3])
{
  BLI_assert(B != R);
  float A[3][3];
  copy_m3_m3(A, R);
  mul_m3_m3m3_uniq(R, A, B);
}

void mul_m3_m3m3_uniq(float R[3][3], const float A[3][3], const float B[3][3])
{
  BLI_assert(R != A && R != B);

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

void mul_m4_m4m3(float m1[4][4], const float m3_[4][4], const float m2_[3][3])
{
  float m2[3][3], m3[4][4];

  /* copy so it works when m1 is the same pointer as m2 or m3 */
  /* TODO: avoid copying when matrices are different */
  copy_m3_m3(m2, m2_);
  copy_m4_m4(m3, m3_);

  m1[0][0] = m2[0][0] * m3[0][0] + m2[0][1] * m3[1][0] + m2[0][2] * m3[2][0];
  m1[0][1] = m2[0][0] * m3[0][1] + m2[0][1] * m3[1][1] + m2[0][2] * m3[2][1];
  m1[0][2] = m2[0][0] * m3[0][2] + m2[0][1] * m3[1][2] + m2[0][2] * m3[2][2];
  m1[1][0] = m2[1][0] * m3[0][0] + m2[1][1] * m3[1][0] + m2[1][2] * m3[2][0];
  m1[1][1] = m2[1][0] * m3[0][1] + m2[1][1] * m3[1][1] + m2[1][2] * m3[2][1];
  m1[1][2] = m2[1][0] * m3[0][2] + m2[1][1] * m3[1][2] + m2[1][2] * m3[2][2];
  m1[2][0] = m2[2][0] * m3[0][0] + m2[2][1] * m3[1][0] + m2[2][2] * m3[2][0];
  m1[2][1] = m2[2][0] * m3[0][1] + m2[2][1] * m3[1][1] + m2[2][2] * m3[2][1];
  m1[2][2] = m2[2][0] * m3[0][2] + m2[2][1] * m3[1][2] + m2[2][2] * m3[2][2];
}

/* m1 = m2 * m3, ignore the elements on the 4th row/column of m2 */
void mul_m3_m3m4(float m1[3][3], const float m3_[3][3], const float m2_[4][4])
{
  float m2[4][4], m3[3][3];

  /* copy so it works when m1 is the same pointer as m2 or m3 */
  /* TODO: avoid copying when matrices are different */
  copy_m4_m4(m2, m2_);
  copy_m3_m3(m3, m3_);

  /* m1[i][j] = m2[i][k] * m3[k][j] */
  m1[0][0] = m2[0][0] * m3[0][0] + m2[0][1] * m3[1][0] + m2[0][2] * m3[2][0];
  m1[0][1] = m2[0][0] * m3[0][1] + m2[0][1] * m3[1][1] + m2[0][2] * m3[2][1];
  m1[0][2] = m2[0][0] * m3[0][2] + m2[0][1] * m3[1][2] + m2[0][2] * m3[2][2];

  m1[1][0] = m2[1][0] * m3[0][0] + m2[1][1] * m3[1][0] + m2[1][2] * m3[2][0];
  m1[1][1] = m2[1][0] * m3[0][1] + m2[1][1] * m3[1][1] + m2[1][2] * m3[2][1];
  m1[1][2] = m2[1][0] * m3[0][2] + m2[1][1] * m3[1][2] + m2[1][2] * m3[2][2];

  m1[2][0] = m2[2][0] * m3[0][0] + m2[2][1] * m3[1][0] + m2[2][2] * m3[2][0];
  m1[2][1] = m2[2][0] * m3[0][1] + m2[2][1] * m3[1][1] + m2[2][2] * m3[2][1];
  m1[2][2] = m2[2][0] * m3[0][2] + m2[2][1] * m3[1][2] + m2[2][2] * m3[2][2];
}

/* m1 = m2 * m3, ignore the elements on the 4th row/column of m3 */
void mul_m3_m4m3(float m1[3][3], const float m3_[4][4], const float m2_[3][3])
{
  float m2[3][3], m3[4][4];

  /* copy so it works when m1 is the same pointer as m2 or m3 */
  /* TODO: avoid copying when matrices are different */
  copy_m3_m3(m2, m2_);
  copy_m4_m4(m3, m3_);

  /* m1[i][j] = m2[i][k] * m3[k][j] */
  m1[0][0] = m2[0][0] * m3[0][0] + m2[0][1] * m3[1][0] + m2[0][2] * m3[2][0];
  m1[0][1] = m2[0][0] * m3[0][1] + m2[0][1] * m3[1][1] + m2[0][2] * m3[2][1];
  m1[0][2] = m2[0][0] * m3[0][2] + m2[0][1] * m3[1][2] + m2[0][2] * m3[2][2];

  m1[1][0] = m2[1][0] * m3[0][0] + m2[1][1] * m3[1][0] + m2[1][2] * m3[2][0];
  m1[1][1] = m2[1][0] * m3[0][1] + m2[1][1] * m3[1][1] + m2[1][2] * m3[2][1];
  m1[1][2] = m2[1][0] * m3[0][2] + m2[1][1] * m3[1][2] + m2[1][2] * m3[2][2];

  m1[2][0] = m2[2][0] * m3[0][0] + m2[2][1] * m3[1][0] + m2[2][2] * m3[2][0];
  m1[2][1] = m2[2][0] * m3[0][1] + m2[2][1] * m3[1][1] + m2[2][2] * m3[2][1];
  m1[2][2] = m2[2][0] * m3[0][2] + m2[2][1] * m3[1][2] + m2[2][2] * m3[2][2];
}

void mul_m4_m3m4(float m1[4][4], const float m3_[3][3], const float m2_[4][4])
{
  float m2[4][4], m3[3][3];

  /* copy so it works when m1 is the same pointer as m2 or m3 */
  /* TODO: avoid copying when matrices are different */
  copy_m4_m4(m2, m2_);
  copy_m3_m3(m3, m3_);

  m1[0][0] = m2[0][0] * m3[0][0] + m2[0][1] * m3[1][0] + m2[0][2] * m3[2][0];
  m1[0][1] = m2[0][0] * m3[0][1] + m2[0][1] * m3[1][1] + m2[0][2] * m3[2][1];
  m1[0][2] = m2[0][0] * m3[0][2] + m2[0][1] * m3[1][2] + m2[0][2] * m3[2][2];
  m1[1][0] = m2[1][0] * m3[0][0] + m2[1][1] * m3[1][0] + m2[1][2] * m3[2][0];
  m1[1][1] = m2[1][0] * m3[0][1] + m2[1][1] * m3[1][1] + m2[1][2] * m3[2][1];
  m1[1][2] = m2[1][0] * m3[0][2] + m2[1][1] * m3[1][2] + m2[1][2] * m3[2][2];
  m1[2][0] = m2[2][0] * m3[0][0] + m2[2][1] * m3[1][0] + m2[2][2] * m3[2][0];
  m1[2][1] = m2[2][0] * m3[0][1] + m2[2][1] * m3[1][1] + m2[2][2] * m3[2][1];
  m1[2][2] = m2[2][0] * m3[0][2] + m2[2][1] * m3[1][2] + m2[2][2] * m3[2][2];
}

void mul_m3_m4m4(float m1[3][3], const float m3[4][4], const float m2[4][4])
{
  m1[0][0] = m2[0][0] * m3[0][0] + m2[0][1] * m3[1][0] + m2[0][2] * m3[2][0];
  m1[0][1] = m2[0][0] * m3[0][1] + m2[0][1] * m3[1][1] + m2[0][2] * m3[2][1];
  m1[0][2] = m2[0][0] * m3[0][2] + m2[0][1] * m3[1][2] + m2[0][2] * m3[2][2];
  m1[1][0] = m2[1][0] * m3[0][0] + m2[1][1] * m3[1][0] + m2[1][2] * m3[2][0];
  m1[1][1] = m2[1][0] * m3[0][1] + m2[1][1] * m3[1][1] + m2[1][2] * m3[2][1];
  m1[1][2] = m2[1][0] * m3[0][2] + m2[1][1] * m3[1][2] + m2[1][2] * m3[2][2];
  m1[2][0] = m2[2][0] * m3[0][0] + m2[2][1] * m3[1][0] + m2[2][2] * m3[2][0];
  m1[2][1] = m2[2][0] * m3[0][1] + m2[2][1] * m3[1][1] + m2[2][2] * m3[2][1];
  m1[2][2] = m2[2][0] * m3[0][2] + m2[2][1] * m3[1][2] + m2[2][2] * m3[2][2];
}

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
  mul_m3_m3m3(r, m1, m2);
  mul_m3_m3m3(r, r, m3);
}
void _va_mul_m3_series_5(float r[3][3],
                         const float m1[3][3],
                         const float m2[3][3],
                         const float m3[3][3],
                         const float m4[3][3])
{
  mul_m3_m3m3(r, m1, m2);
  mul_m3_m3m3(r, r, m3);
  mul_m3_m3m3(r, r, m4);
}
void _va_mul_m3_series_6(float r[3][3],
                         const float m1[3][3],
                         const float m2[3][3],
                         const float m3[3][3],
                         const float m4[3][3],
                         const float m5[3][3])
{
  mul_m3_m3m3(r, m1, m2);
  mul_m3_m3m3(r, r, m3);
  mul_m3_m3m3(r, r, m4);
  mul_m3_m3m3(r, r, m5);
}
void _va_mul_m3_series_7(float r[3][3],
                         const float m1[3][3],
                         const float m2[3][3],
                         const float m3[3][3],
                         const float m4[3][3],
                         const float m5[3][3],
                         const float m6[3][3])
{
  mul_m3_m3m3(r, m1, m2);
  mul_m3_m3m3(r, r, m3);
  mul_m3_m3m3(r, r, m4);
  mul_m3_m3m3(r, r, m5);
  mul_m3_m3m3(r, r, m6);
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
  mul_m3_m3m3(r, m1, m2);
  mul_m3_m3m3(r, r, m3);
  mul_m3_m3m3(r, r, m4);
  mul_m3_m3m3(r, r, m5);
  mul_m3_m3m3(r, r, m6);
  mul_m3_m3m3(r, r, m7);
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
  mul_m3_m3m3(r, m1, m2);
  mul_m3_m3m3(r, r, m3);
  mul_m3_m3m3(r, r, m4);
  mul_m3_m3m3(r, r, m5);
  mul_m3_m3m3(r, r, m6);
  mul_m3_m3m3(r, r, m7);
  mul_m3_m3m3(r, r, m8);
}
/** \} */

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
  mul_m4_m4m4(r, m1, m2);
  mul_m4_m4m4(r, r, m3);
}
void _va_mul_m4_series_5(float r[4][4],
                         const float m1[4][4],
                         const float m2[4][4],
                         const float m3[4][4],
                         const float m4[4][4])
{
  mul_m4_m4m4(r, m1, m2);
  mul_m4_m4m4(r, r, m3);
  mul_m4_m4m4(r, r, m4);
}
void _va_mul_m4_series_6(float r[4][4],
                         const float m1[4][4],
                         const float m2[4][4],
                         const float m3[4][4],
                         const float m4[4][4],
                         const float m5[4][4])
{
  mul_m4_m4m4(r, m1, m2);
  mul_m4_m4m4(r, r, m3);
  mul_m4_m4m4(r, r, m4);
  mul_m4_m4m4(r, r, m5);
}
void _va_mul_m4_series_7(float r[4][4],
                         const float m1[4][4],
                         const float m2[4][4],
                         const float m3[4][4],
                         const float m4[4][4],
                         const float m5[4][4],
                         const float m6[4][4])
{
  mul_m4_m4m4(r, m1, m2);
  mul_m4_m4m4(r, r, m3);
  mul_m4_m4m4(r, r, m4);
  mul_m4_m4m4(r, r, m5);
  mul_m4_m4m4(r, r, m6);
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
  mul_m4_m4m4(r, m1, m2);
  mul_m4_m4m4(r, r, m3);
  mul_m4_m4m4(r, r, m4);
  mul_m4_m4m4(r, r, m5);
  mul_m4_m4m4(r, r, m6);
  mul_m4_m4m4(r, r, m7);
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
  mul_m4_m4m4(r, m1, m2);
  mul_m4_m4m4(r, r, m3);
  mul_m4_m4m4(r, r, m4);
  mul_m4_m4m4(r, r, m5);
  mul_m4_m4m4(r, r, m6);
  mul_m4_m4m4(r, r, m7);
  mul_m4_m4m4(r, r, m8);
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

void mul_m4_v3(const float mat[4][4], float vec[3])
{
  const float x = vec[0];
  const float y = vec[1];

  vec[0] = x * mat[0][0] + y * mat[1][0] + mat[2][0] * vec[2] + mat[3][0];
  vec[1] = x * mat[0][1] + y * mat[1][1] + mat[2][1] * vec[2] + mat[3][1];
  vec[2] = x * mat[0][2] + y * mat[1][2] + mat[2][2] * vec[2] + mat[3][2];
}

void mul_v3_m4v3(float r[3], const float mat[4][4], const float vec[3])
{
  const float x = vec[0];
  const float y = vec[1];

  r[0] = x * mat[0][0] + y * mat[1][0] + mat[2][0] * vec[2] + mat[3][0];
  r[1] = x * mat[0][1] + y * mat[1][1] + mat[2][1] * vec[2] + mat[3][1];
  r[2] = x * mat[0][2] + y * mat[1][2] + mat[2][2] * vec[2] + mat[3][2];
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

void mul_m2v2(const float mat[2][2], float vec[2])
{
  mul_v2_m2v2(vec, mat, vec);
}

/* same as mul_m4_v3() but doesnt apply translation component */
void mul_mat3_m4_v3(const float mat[4][4], float vec[3])
{
  const float x = vec[0];
  const float y = vec[1];

  vec[0] = x * mat[0][0] + y * mat[1][0] + mat[2][0] * vec[2];
  vec[1] = x * mat[0][1] + y * mat[1][1] + mat[2][1] * vec[2];
  vec[2] = x * mat[0][2] + y * mat[1][2] + mat[2][2] * vec[2];
}

void mul_v3_mat3_m4v3(float r[3], const float mat[4][4], const float vec[3])
{
  const float x = vec[0];
  const float y = vec[1];

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

void mul_v4d_m4v4d(double r[4], const float mat[4][4], const double v[4])
{
  const double x = v[0];
  const double y = v[1];
  const double z = v[2];

  r[0] = x * (double)mat[0][0] + y * (double)mat[1][0] + z * (double)mat[2][0] +
         (double)mat[3][0] * v[3];
  r[1] = x * (double)mat[0][1] + y * (double)mat[1][1] + z * (double)mat[2][1] +
         (double)mat[3][1] * v[3];
  r[2] = x * (double)mat[0][2] + y * (double)mat[1][2] + z * (double)mat[2][2] +
         (double)mat[3][2] * v[3];
  r[3] = x * (double)mat[0][3] + y * (double)mat[1][3] + z * (double)mat[2][3] +
         (double)mat[3][3] * v[3];
}

void mul_m4_v4d(const float mat[4][4], double r[4])
{
  mul_v4d_m4v4d(r, mat, r);
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
  mul_v3_m3v3(r, M, (const float[3]){UNPACK3(r)});
}

void mul_m3_v3_db(const double M[3][3], double r[3])
{
  mul_v3_m3v3_db(r, M, (const double[3]){UNPACK3(r)});
}

void mul_transposed_m3_v3(const float mat[3][3], float vec[3])
{
  const float x = vec[0];
  const float y = vec[1];

  vec[0] = x * mat[0][0] + y * mat[0][1] + mat[0][2] * vec[2];
  vec[1] = x * mat[1][0] + y * mat[1][1] + mat[1][2] * vec[2];
  vec[2] = x * mat[2][0] + y * mat[2][1] + mat[2][2] * vec[2];
}

void mul_transposed_mat3_m4_v3(const float mat[4][4], float vec[3])
{
  const float x = vec[0];
  const float y = vec[1];

  vec[0] = x * mat[0][0] + y * mat[0][1] + mat[0][2] * vec[2];
  vec[1] = x * mat[1][0] + y * mat[1][1] + mat[1][2] * vec[2];
  vec[2] = x * mat[2][0] + y * mat[2][1] + mat[2][2] * vec[2];
}

void mul_m3_fl(float m[3][3], float f)
{
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      m[i][j] *= f;
    }
  }
}

void mul_m4_fl(float m[4][4], float f)
{
  int i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      m[i][j] *= f;
    }
  }
}

void mul_mat3_m4_fl(float m[4][4], float f)
{
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      m[i][j] *= f;
    }
  }
}

void negate_m3(float m[3][3])
{
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      m[i][j] *= -1.0f;
    }
  }
}

void negate_mat3_m4(float m[4][4])
{
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      m[i][j] *= -1.0f;
    }
  }
}

void negate_m4(float m[4][4])
{
  int i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      m[i][j] *= -1.0f;
    }
  }
}

void mul_m3_v3_double(const float mat[3][3], double vec[3])
{
  const double x = vec[0];
  const double y = vec[1];

  vec[0] = x * (double)mat[0][0] + y * (double)mat[1][0] + (double)mat[2][0] * vec[2];
  vec[1] = x * (double)mat[0][1] + y * (double)mat[1][1] + (double)mat[2][1] * vec[2];
  vec[2] = x * (double)mat[0][2] + y * (double)mat[1][2] + (double)mat[2][2] * vec[2];
}

void add_m3_m3m3(float m1[3][3], const float m2[3][3], const float m3[3][3])
{
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      m1[i][j] = m2[i][j] + m3[i][j];
    }
  }
}

void add_m4_m4m4(float m1[4][4], const float m2[4][4], const float m3[4][4])
{
  int i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      m1[i][j] = m2[i][j] + m3[i][j];
    }
  }
}

void madd_m3_m3m3fl(float m1[3][3], const float m2[3][3], const float m3[3][3], const float f)
{
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      m1[i][j] = m2[i][j] + m3[i][j] * f;
    }
  }
}

void madd_m4_m4m4fl(float m1[4][4], const float m2[4][4], const float m3[4][4], const float f)
{
  int i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      m1[i][j] = m2[i][j] + m3[i][j] * f;
    }
  }
}

void sub_m3_m3m3(float m1[3][3], const float m2[3][3], const float m3[3][3])
{
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      m1[i][j] = m2[i][j] - m3[i][j];
    }
  }
}

void sub_m4_m4m4(float m1[4][4], const float m2[4][4], const float m3[4][4])
{
  int i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      m1[i][j] = m2[i][j] - m3[i][j];
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

bool invert_m3_ex(float m[3][3], const float epsilon)
{
  float tmp[3][3];
  const bool success = invert_m3_m3_ex(tmp, m, epsilon);

  copy_m3_m3(m, tmp);
  return success;
}

bool invert_m3_m3_ex(float m1[3][3], const float m2[3][3], const float epsilon)
{
  float det;
  int a, b;
  bool success;

  BLI_assert(epsilon >= 0.0f);

  /* calc adjoint */
  adjoint_m3_m3(m1, m2);

  /* then determinant old matrix! */
  det = determinant_m3_array(m2);

  success = (fabsf(det) > epsilon);

  if (LIKELY(det != 0.0f)) {
    det = 1.0f / det;
    for (a = 0; a < 3; a++) {
      for (b = 0; b < 3; b++) {
        m1[a][b] *= det;
      }
    }
  }
  return success;
}

bool invert_m3(float m[3][3])
{
  float tmp[3][3];
  const bool success = invert_m3_m3(tmp, m);

  copy_m3_m3(m, tmp);
  return success;
}

bool invert_m3_m3(float m1[3][3], const float m2[3][3])
{
  float det;
  int a, b;
  bool success;

  /* calc adjoint */
  adjoint_m3_m3(m1, m2);

  /* then determinant old matrix! */
  det = determinant_m3_array(m2);

  success = (det != 0.0f);

  if (LIKELY(det != 0.0f)) {
    det = 1.0f / det;
    for (a = 0; a < 3; a++) {
      for (b = 0; b < 3; b++) {
        m1[a][b] *= det;
      }
    }
  }

  return success;
}

bool invert_m4(float m[4][4])
{
  float tmp[4][4];
  const bool success = invert_m4_m4(tmp, m);

  copy_m4_m4(m, tmp);
  return success;
}

bool invert_m4_m4(float inverse[4][4], const float mat[4][4])
{
  /* Use optimized matrix inverse from Eigen, since performance
   * impact of this function is significant in complex rigs. */
  return EIG_invert_m4_m4(inverse, mat);
}

/****************************** Linear Algebra *******************************/

void transpose_m3(float mat[3][3])
{
  float t;

  t = mat[0][1];
  mat[0][1] = mat[1][0];
  mat[1][0] = t;
  t = mat[0][2];
  mat[0][2] = mat[2][0];
  mat[2][0] = t;
  t = mat[1][2];
  mat[1][2] = mat[2][1];
  mat[2][1] = t;
}

void transpose_m3_m3(float rmat[3][3], const float mat[3][3])
{
  BLI_assert(rmat != mat);

  rmat[0][0] = mat[0][0];
  rmat[0][1] = mat[1][0];
  rmat[0][2] = mat[2][0];
  rmat[1][0] = mat[0][1];
  rmat[1][1] = mat[1][1];
  rmat[1][2] = mat[2][1];
  rmat[2][0] = mat[0][2];
  rmat[2][1] = mat[1][2];
  rmat[2][2] = mat[2][2];
}

/* seems obscure but in-fact a common operation */
void transpose_m3_m4(float rmat[3][3], const float mat[4][4])
{
  BLI_assert(&rmat[0][0] != &mat[0][0]);

  rmat[0][0] = mat[0][0];
  rmat[0][1] = mat[1][0];
  rmat[0][2] = mat[2][0];
  rmat[1][0] = mat[0][1];
  rmat[1][1] = mat[1][1];
  rmat[1][2] = mat[2][1];
  rmat[2][0] = mat[0][2];
  rmat[2][1] = mat[1][2];
  rmat[2][2] = mat[2][2];
}

void transpose_m4(float mat[4][4])
{
  float t;

  t = mat[0][1];
  mat[0][1] = mat[1][0];
  mat[1][0] = t;
  t = mat[0][2];
  mat[0][2] = mat[2][0];
  mat[2][0] = t;
  t = mat[0][3];
  mat[0][3] = mat[3][0];
  mat[3][0] = t;

  t = mat[1][2];
  mat[1][2] = mat[2][1];
  mat[2][1] = t;
  t = mat[1][3];
  mat[1][3] = mat[3][1];
  mat[3][1] = t;

  t = mat[2][3];
  mat[2][3] = mat[3][2];
  mat[3][2] = t;
}

void transpose_m4_m4(float rmat[4][4], const float mat[4][4])
{
  BLI_assert(rmat != mat);

  rmat[0][0] = mat[0][0];
  rmat[0][1] = mat[1][0];
  rmat[0][2] = mat[2][0];
  rmat[0][3] = mat[3][0];
  rmat[1][0] = mat[0][1];
  rmat[1][1] = mat[1][1];
  rmat[1][2] = mat[2][1];
  rmat[1][3] = mat[3][1];
  rmat[2][0] = mat[0][2];
  rmat[2][1] = mat[1][2];
  rmat[2][2] = mat[2][2];
  rmat[2][3] = mat[3][2];
  rmat[3][0] = mat[0][3];
  rmat[3][1] = mat[1][3];
  rmat[3][2] = mat[2][3];
  rmat[3][3] = mat[3][3];
}

/* TODO: return bool */
int compare_m4m4(const float mat1[4][4], const float mat2[4][4], float limit)
{
  if (compare_v4v4(mat1[0], mat2[0], limit)) {
    if (compare_v4v4(mat1[1], mat2[1], limit)) {
      if (compare_v4v4(mat1[2], mat2[2], limit)) {
        if (compare_v4v4(mat1[3], mat2[3], limit)) {
          return 1;
        }
      }
    }
  }
  return 0;
}

/**
 * Make an orthonormal matrix around the selected axis of the given matrix.
 *
 * \param axis: Axis to build the orthonormal basis around.
 */
void orthogonalize_m3(float mat[3][3], int axis)
{
  float size[3];
  mat3_to_size(size, mat);
  normalize_v3(mat[axis]);
  switch (axis) {
    case 0:
      if (dot_v3v3(mat[0], mat[1]) < 1) {
        cross_v3_v3v3(mat[2], mat[0], mat[1]);
        normalize_v3(mat[2]);
        cross_v3_v3v3(mat[1], mat[2], mat[0]);
      }
      else if (dot_v3v3(mat[0], mat[2]) < 1) {
        cross_v3_v3v3(mat[1], mat[2], mat[0]);
        normalize_v3(mat[1]);
        cross_v3_v3v3(mat[2], mat[0], mat[1]);
      }
      else {
        float vec[3];

        vec[0] = mat[0][1];
        vec[1] = mat[0][2];
        vec[2] = mat[0][0];

        cross_v3_v3v3(mat[2], mat[0], vec);
        normalize_v3(mat[2]);
        cross_v3_v3v3(mat[1], mat[2], mat[0]);
      }
      break;
    case 1:
      if (dot_v3v3(mat[1], mat[0]) < 1) {
        cross_v3_v3v3(mat[2], mat[0], mat[1]);
        normalize_v3(mat[2]);
        cross_v3_v3v3(mat[0], mat[1], mat[2]);
      }
      else if (dot_v3v3(mat[0], mat[2]) < 1) {
        cross_v3_v3v3(mat[0], mat[1], mat[2]);
        normalize_v3(mat[0]);
        cross_v3_v3v3(mat[2], mat[0], mat[1]);
      }
      else {
        float vec[3];

        vec[0] = mat[1][1];
        vec[1] = mat[1][2];
        vec[2] = mat[1][0];

        cross_v3_v3v3(mat[0], mat[1], vec);
        normalize_v3(mat[0]);
        cross_v3_v3v3(mat[2], mat[0], mat[1]);
      }
      break;
    case 2:
      if (dot_v3v3(mat[2], mat[0]) < 1) {
        cross_v3_v3v3(mat[1], mat[2], mat[0]);
        normalize_v3(mat[1]);
        cross_v3_v3v3(mat[0], mat[1], mat[2]);
      }
      else if (dot_v3v3(mat[2], mat[1]) < 1) {
        cross_v3_v3v3(mat[0], mat[1], mat[2]);
        normalize_v3(mat[0]);
        cross_v3_v3v3(mat[1], mat[2], mat[0]);
      }
      else {
        float vec[3];

        vec[0] = mat[2][1];
        vec[1] = mat[2][2];
        vec[2] = mat[2][0];

        cross_v3_v3v3(mat[0], vec, mat[2]);
        normalize_v3(mat[0]);
        cross_v3_v3v3(mat[1], mat[2], mat[0]);
      }
      break;
    default:
      BLI_assert(0);
      break;
  }
  mul_v3_fl(mat[0], size[0]);
  mul_v3_fl(mat[1], size[1]);
  mul_v3_fl(mat[2], size[2]);
}

/**
 * Make an orthonormal matrix around the selected axis of the given matrix.
 *
 * \param axis: Axis to build the orthonormal basis around.
 */
void orthogonalize_m4(float mat[4][4], int axis)
{
  float size[3];
  mat4_to_size(size, mat);
  normalize_v3(mat[axis]);
  switch (axis) {
    case 0:
      if (dot_v3v3(mat[0], mat[1]) < 1) {
        cross_v3_v3v3(mat[2], mat[0], mat[1]);
        normalize_v3(mat[2]);
        cross_v3_v3v3(mat[1], mat[2], mat[0]);
      }
      else if (dot_v3v3(mat[0], mat[2]) < 1) {
        cross_v3_v3v3(mat[1], mat[2], mat[0]);
        normalize_v3(mat[1]);
        cross_v3_v3v3(mat[2], mat[0], mat[1]);
      }
      else {
        float vec[3];

        vec[0] = mat[0][1];
        vec[1] = mat[0][2];
        vec[2] = mat[0][0];

        cross_v3_v3v3(mat[2], mat[0], vec);
        normalize_v3(mat[2]);
        cross_v3_v3v3(mat[1], mat[2], mat[0]);
      }
      break;
    case 1:
      if (dot_v3v3(mat[1], mat[0]) < 1) {
        cross_v3_v3v3(mat[2], mat[0], mat[1]);
        normalize_v3(mat[2]);
        cross_v3_v3v3(mat[0], mat[1], mat[2]);
      }
      else if (dot_v3v3(mat[0], mat[2]) < 1) {
        cross_v3_v3v3(mat[0], mat[1], mat[2]);
        normalize_v3(mat[0]);
        cross_v3_v3v3(mat[2], mat[0], mat[1]);
      }
      else {
        float vec[3];

        vec[0] = mat[1][1];
        vec[1] = mat[1][2];
        vec[2] = mat[1][0];

        cross_v3_v3v3(mat[0], mat[1], vec);
        normalize_v3(mat[0]);
        cross_v3_v3v3(mat[2], mat[0], mat[1]);
      }
      break;
    case 2:
      if (dot_v3v3(mat[2], mat[0]) < 1) {
        cross_v3_v3v3(mat[1], mat[2], mat[0]);
        normalize_v3(mat[1]);
        cross_v3_v3v3(mat[0], mat[1], mat[2]);
      }
      else if (dot_v3v3(mat[2], mat[1]) < 1) {
        cross_v3_v3v3(mat[0], mat[1], mat[2]);
        normalize_v3(mat[0]);
        cross_v3_v3v3(mat[1], mat[2], mat[0]);
      }
      else {
        float vec[3];

        vec[0] = mat[2][1];
        vec[1] = mat[2][2];
        vec[2] = mat[2][0];

        cross_v3_v3v3(mat[0], vec, mat[2]);
        normalize_v3(mat[0]);
        cross_v3_v3v3(mat[1], mat[2], mat[0]);
      }
      break;
    default:
      BLI_assert(0);
      break;
  }
  mul_v3_fl(mat[0], size[0]);
  mul_v3_fl(mat[1], size[1]);
  mul_v3_fl(mat[2], size[2]);
}

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

    for (i = 0; i < 4; i++)
      if (fabsf(dot_v4v4(m[i], m[i]) - 1) > 1e-5f) {
        return false;
      }

    return true;
  }

  return false;
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
      fabsf(l5 - l1) <= eps && fabsf(l6 - l1) <= eps) {
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

void normalize_m3_ex(float mat[3][3], float r_scale[3])
{
  int i;
  for (i = 0; i < 3; i++) {
    r_scale[i] = normalize_v3(mat[i]);
  }
}
void normalize_m3(float mat[3][3])
{
  int i;
  for (i = 0; i < 3; i++) {
    normalize_v3(mat[i]);
  }
}

void normalize_m3_m3_ex(float rmat[3][3], const float mat[3][3], float r_scale[3])
{
  int i;
  for (i = 0; i < 3; i++) {
    r_scale[i] = normalize_v3_v3(rmat[i], mat[i]);
  }
}
void normalize_m3_m3(float rmat[3][3], const float mat[3][3])
{
  int i;
  for (i = 0; i < 3; i++) {
    normalize_v3_v3(rmat[i], mat[i]);
  }
}

void normalize_m4_ex(float mat[4][4], float r_scale[3])
{
  int i;
  for (i = 0; i < 3; i++) {
    r_scale[i] = normalize_v3(mat[i]);
    if (r_scale[i] != 0.0f) {
      mat[i][3] /= r_scale[i];
    }
  }
}
void normalize_m4(float mat[4][4])
{
  int i;
  for (i = 0; i < 3; i++) {
    float len = normalize_v3(mat[i]);
    if (len != 0.0f) {
      mat[i][3] /= len;
    }
  }
}

void normalize_m4_m4_ex(float rmat[4][4], const float mat[4][4], float r_scale[3])
{
  int i;
  for (i = 0; i < 3; i++) {
    r_scale[i] = normalize_v3_v3(rmat[i], mat[i]);
    rmat[i][3] = (r_scale[i] != 0.0f) ? (mat[i][3] / r_scale[i]) : mat[i][3];
  }
  copy_v4_v4(rmat[3], mat[3]);
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

void adjoint_m2_m2(float m1[2][2], const float m[2][2])
{
  BLI_assert(m1 != m);
  m1[0][0] = m[1][1];
  m1[0][1] = -m[0][1];
  m1[1][0] = -m[1][0];
  m1[1][1] = m[0][0];
}

void adjoint_m3_m3(float m1[3][3], const float m[3][3])
{
  BLI_assert(m1 != m);
  m1[0][0] = m[1][1] * m[2][2] - m[1][2] * m[2][1];
  m1[0][1] = -m[0][1] * m[2][2] + m[0][2] * m[2][1];
  m1[0][2] = m[0][1] * m[1][2] - m[0][2] * m[1][1];

  m1[1][0] = -m[1][0] * m[2][2] + m[1][2] * m[2][0];
  m1[1][1] = m[0][0] * m[2][2] - m[0][2] * m[2][0];
  m1[1][2] = -m[0][0] * m[1][2] + m[0][2] * m[1][0];

  m1[2][0] = m[1][0] * m[2][1] - m[1][1] * m[2][0];
  m1[2][1] = -m[0][0] * m[2][1] + m[0][1] * m[2][0];
  m1[2][2] = m[0][0] * m[1][1] - m[0][1] * m[1][0];
}

void adjoint_m4_m4(float out[4][4], const float in[4][4]) /* out = ADJ(in) */
{
  float a1, a2, a3, a4, b1, b2, b3, b4;
  float c1, c2, c3, c4, d1, d2, d3, d4;

  a1 = in[0][0];
  b1 = in[0][1];
  c1 = in[0][2];
  d1 = in[0][3];

  a2 = in[1][0];
  b2 = in[1][1];
  c2 = in[1][2];
  d2 = in[1][3];

  a3 = in[2][0];
  b3 = in[2][1];
  c3 = in[2][2];
  d3 = in[2][3];

  a4 = in[3][0];
  b4 = in[3][1];
  c4 = in[3][2];
  d4 = in[3][3];

  out[0][0] = determinant_m3(b2, b3, b4, c2, c3, c4, d2, d3, d4);
  out[1][0] = -determinant_m3(a2, a3, a4, c2, c3, c4, d2, d3, d4);
  out[2][0] = determinant_m3(a2, a3, a4, b2, b3, b4, d2, d3, d4);
  out[3][0] = -determinant_m3(a2, a3, a4, b2, b3, b4, c2, c3, c4);

  out[0][1] = -determinant_m3(b1, b3, b4, c1, c3, c4, d1, d3, d4);
  out[1][1] = determinant_m3(a1, a3, a4, c1, c3, c4, d1, d3, d4);
  out[2][1] = -determinant_m3(a1, a3, a4, b1, b3, b4, d1, d3, d4);
  out[3][1] = determinant_m3(a1, a3, a4, b1, b3, b4, c1, c3, c4);

  out[0][2] = determinant_m3(b1, b2, b4, c1, c2, c4, d1, d2, d4);
  out[1][2] = -determinant_m3(a1, a2, a4, c1, c2, c4, d1, d2, d4);
  out[2][2] = determinant_m3(a1, a2, a4, b1, b2, b4, d1, d2, d4);
  out[3][2] = -determinant_m3(a1, a2, a4, b1, b2, b4, c1, c2, c4);

  out[0][3] = -determinant_m3(b1, b2, b3, c1, c2, c3, d1, d2, d3);
  out[1][3] = determinant_m3(a1, a2, a3, c1, c2, c3, d1, d2, d3);
  out[2][3] = -determinant_m3(a1, a2, a3, b1, b2, b3, d1, d2, d3);
  out[3][3] = determinant_m3(a1, a2, a3, b1, b2, b3, c1, c2, c3);
}

float determinant_m2(float a, float b, float c, float d)
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

void size_to_mat3(float mat[3][3], const float size[3])
{
  mat[0][0] = size[0];
  mat[0][1] = 0.0f;
  mat[0][2] = 0.0f;
  mat[1][1] = size[1];
  mat[1][0] = 0.0f;
  mat[1][2] = 0.0f;
  mat[2][2] = size[2];
  mat[2][1] = 0.0f;
  mat[2][0] = 0.0f;
}

void size_to_mat4(float mat[4][4], const float size[3])
{
  mat[0][0] = size[0];
  mat[0][1] = 0.0f;
  mat[0][2] = 0.0f;
  mat[0][3] = 0.0f;
  mat[1][0] = 0.0f;
  mat[1][1] = size[1];
  mat[1][2] = 0.0f;
  mat[1][3] = 0.0f;
  mat[2][0] = 0.0f;
  mat[2][1] = 0.0f;
  mat[2][2] = size[2];
  mat[2][3] = 0.0f;
  mat[3][0] = 0.0f;
  mat[3][1] = 0.0f;
  mat[3][2] = 0.0f;
  mat[3][3] = 1.0f;
}

void mat3_to_size(float size[3], const float mat[3][3])
{
  size[0] = len_v3(mat[0]);
  size[1] = len_v3(mat[1]);
  size[2] = len_v3(mat[2]);
}

void mat4_to_size(float size[3], const float mat[4][4])
{
  size[0] = len_v3(mat[0]);
  size[1] = len_v3(mat[1]);
  size[2] = len_v3(mat[2]);
}

/* This computes the overall volume scale factor of a transformation matrix.
 * For an orthogonal matrix, it is the product of all three scale values.
 * Returns a negative value if the transform is flipped by negative scale. */
float mat3_to_volume_scale(const float mat[3][3])
{
  return determinant_m3_array(mat);
}

float mat4_to_volume_scale(const float mat[4][4])
{
  return determinant_m4_mat3_array(mat);
}

/* this gets the average scale of a matrix, only use when your scaling
 * data that has no idea of scale axis, examples are bone-envelope-radius
 * and curve radius */
float mat3_to_scale(const float mat[3][3])
{
  /* unit length vector */
  float unit_vec[3];
  copy_v3_fl(unit_vec, (float)M_SQRT1_3);
  mul_m3_v3(mat, unit_vec);
  return len_v3(unit_vec);
}

float mat4_to_scale(const float mat[4][4])
{
  /* unit length vector */
  float unit_vec[3];
  copy_v3_fl(unit_vec, (float)M_SQRT1_3);
  mul_mat3_m4_v3(mat, unit_vec);
  return len_v3(unit_vec);
}

/** Return 2D scale (in XY plane) of given mat4. */
float mat4_to_xy_scale(const float M[4][4])
{
  /* unit length vector in xy plane */
  float unit_vec[3] = {(float)M_SQRT1_2, (float)M_SQRT1_2, 0.0f};
  mul_mat3_m4_v3(M, unit_vec);
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

  /* so scale doesn't interfere with rotation [#24291] */
  /* note: this is a workaround for negative matrix not working for rotation conversion, FIXME */
  if (is_negative_m3(mat3)) {
    negate_m3(mat3_n);
  }

  mat3_normalized_to_quat(quat, mat3_n);
  copy_v3_v3(loc, wmat[3]);
}

void mat4_decompose(float loc[3], float quat[4], float size[3], const float wmat[4][4])
{
  float rot[3][3];
  mat4_to_loc_rot_size(loc, rot, size, wmat);
  mat3_normalized_to_quat(quat, rot);
}

/**
 * Right polar decomposition:
 *     M = UP
 *
 * U is the 'rotation'-like component, the closest orthogonal matrix to M.
 * P is the 'scaling'-like component, defined in U space.
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

void scale_m3_fl(float m[3][3], float scale)
{
  m[0][0] = m[1][1] = m[2][2] = scale;
  m[0][1] = m[0][2] = 0.0;
  m[1][0] = m[1][2] = 0.0;
  m[2][0] = m[2][1] = 0.0;
}

void scale_m4_fl(float m[4][4], float scale)
{
  m[0][0] = m[1][1] = m[2][2] = scale;
  m[3][3] = 1.0;
  m[0][1] = m[0][2] = m[0][3] = 0.0;
  m[1][0] = m[1][2] = m[1][3] = 0.0;
  m[2][0] = m[2][1] = m[2][3] = 0.0;
  m[3][0] = m[3][1] = m[3][2] = 0.0;
}

void translate_m4(float mat[4][4], float Tx, float Ty, float Tz)
{
  mat[3][0] += (Tx * mat[0][0] + Ty * mat[1][0] + Tz * mat[2][0]);
  mat[3][1] += (Tx * mat[0][1] + Ty * mat[1][1] + Tz * mat[2][1]);
  mat[3][2] += (Tx * mat[0][2] + Ty * mat[1][2] + Tz * mat[2][2]);
}

/* TODO: enum for axis? */
/**
 * Rotate a matrix in-place.
 *
 * \note To create a new rotation matrix see:
 * #axis_angle_to_mat4_single, #axis_angle_to_mat3_single, #angle_to_mat2
 * (axis & angle args are compatible).
 */
void rotate_m4(float mat[4][4], const char axis, const float angle)
{
  const float angle_cos = cosf(angle);
  const float angle_sin = sinf(angle);

  assert(axis >= 'X' && axis <= 'Z');

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
      BLI_assert(0);
      break;
  }
}

/**
 * Scale or rotate around a pivot point,
 * a convenience function to avoid having to do inline.
 *
 * Since its common to make a scale/rotation matrix that pivots around an arbitrary point.
 *
 * Typical use case is to make 3x3 matrix, copy to 4x4, then pass to this function.
 */
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

  mat3_normalized_to_quat(dquat, drot);
  mat3_normalized_to_quat(squat, srot);

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

  mat3_normalized_to_quat(dquat, drot);
  mat3_normalized_to_quat(squat, srot);

  /* do blending */
  interp_v3_v3v3(floc, dloc, sloc, srcweight);
  interp_qt_qtqt(fquat, dquat, squat, srcweight);
  interp_v3_v3v3(fsize, dscale, sscale, srcweight);

  /* compose new matrix */
  loc_quat_size_to_mat4(out, floc, fquat, fsize);
}

/* for builds without Eigen */
#ifndef MATH_STANDALONE
/**
 * A polar-decomposition-based interpolation between matrix A and matrix B.
 *
 * \note This code is about five times slower as the 'naive' interpolation done by #blend_m3_m3m3
 * (it typically remains below 2 usec on an average i74700,
 * while #blend_m3_m3m3 remains below 0.4 usec).
 * However, it gives expected results even with non-uniformly scaled matrices,
 * see T46418 for an example.
 *
 * Based on "Matrix Animation and Polar Decomposition", by Ken Shoemake & Tom Duff
 *
 * \param R: Resulting interpolated matrix.
 * \param A: Input matrix which is totally effective with `t = 0.0`.
 * \param B: Input matrix which is totally effective with `t = 1.0`.
 * \param t: Interpolation factor.
 */
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

/**
 * Complete transform matrix interpolation,
 * based on polar-decomposition-based interpolation from #interp_m3_m3m3.
 *
 * \param R: Resulting interpolated matrix.
 * \param A: Input matrix which is totally effective with `t = 0.0`.
 * \param B: Input matrix which is totally effective with `t = 1.0`.
 * \param t: Interpolation factor.
 */
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
  float vec[3];
  cross_v3_v3v3(vec, mat[0], mat[1]);
  return (dot_v3v3(vec, mat[2]) < 0.0f);
}

bool is_negative_m4(const float mat[4][4])
{
  float vec[3];
  cross_v3_v3v3(vec, mat[0], mat[1]);
  return (dot_v3v3(vec, mat[2]) < 0.0f);
}

bool is_zero_m3(const float mat[3][3])
{
  return (is_zero_v3(mat[0]) && is_zero_v3(mat[1]) && is_zero_v3(mat[2]));
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

/* make a 4x4 matrix out of 3 transform components */
/* matrices are made in the order: scale * rot * loc */
/* TODO: need to have a version that allows for rotation order... */

void loc_eul_size_to_mat4(float mat[4][4],
                          const float loc[3],
                          const float eul[3],
                          const float size[3])
{
  float rmat[3][3], smat[3][3], tmat[3][3];

  /* initialize new matrix */
  unit_m4(mat);

  /* make rotation + scaling part */
  eul_to_mat3(rmat, eul);
  size_to_mat3(smat, size);
  mul_m3_m3m3(tmat, rmat, smat);

  /* copy rot/scale part to output matrix*/
  copy_m4_m3(mat, tmat);

  /* copy location to matrix */
  mat[3][0] = loc[0];
  mat[3][1] = loc[1];
  mat[3][2] = loc[2];
}

/* make a 4x4 matrix out of 3 transform components */

/* matrices are made in the order: scale * rot * loc */
void loc_eulO_size_to_mat4(float mat[4][4],
                           const float loc[3],
                           const float eul[3],
                           const float size[3],
                           const short rotOrder)
{
  float rmat[3][3], smat[3][3], tmat[3][3];

  /* initialize new matrix */
  unit_m4(mat);

  /* make rotation + scaling part */
  eulO_to_mat3(rmat, eul, rotOrder);
  size_to_mat3(smat, size);
  mul_m3_m3m3(tmat, rmat, smat);

  /* copy rot/scale part to output matrix*/
  copy_m4_m3(mat, tmat);

  /* copy location to matrix */
  mat[3][0] = loc[0];
  mat[3][1] = loc[1];
  mat[3][2] = loc[2];
}

/* make a 4x4 matrix out of 3 transform components */

/* matrices are made in the order: scale * rot * loc */
void loc_quat_size_to_mat4(float mat[4][4],
                           const float loc[3],
                           const float quat[4],
                           const float size[3])
{
  float rmat[3][3], smat[3][3], tmat[3][3];

  /* initialize new matrix */
  unit_m4(mat);

  /* make rotation + scaling part */
  quat_to_mat3(rmat, quat);
  size_to_mat3(smat, size);
  mul_m3_m3m3(tmat, rmat, smat);

  /* copy rot/scale part to output matrix*/
  copy_m4_m3(mat, tmat);

  /* copy location to matrix */
  mat[3][0] = loc[0];
  mat[3][1] = loc[1];
  mat[3][2] = loc[2];
}

void loc_axisangle_size_to_mat4(float mat[4][4],
                                const float loc[3],
                                const float axis[3],
                                const float angle,
                                const float size[3])
{
  float q[4];
  axis_angle_to_quat(q, axis, angle);
  loc_quat_size_to_mat4(mat, loc, q, size);
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

/*********************************** SVD ************************************
 * from TNT matrix library
 *
 * Compute the Single Value Decomposition of an arbitrary matrix A
 * That is compute the 3 matrices U,W,V with U column orthogonal (m,n)
 * ,W a diagonal matrix and V an orthogonal square matrix s.t.
 * A = U.W.Vt. From this decomposition it is trivial to compute the
 * (pseudo-inverse) of A as Ainv = V.Winv.tranpose(U).
 */

void svd_m4(float U[4][4], float s[4], float V[4][4], float A_[4][4])
{
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
      if ((k + 1 < m) & (e[k] != 0.0f)) {
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
    if ((k < nrt) & (e[k] != 0.0f)) {
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
        t = (ks != p ? fabsf(e[ks]) : 0.f) + (ks != k + 1 ? fabsf(e[ks - 1]) : 0.0f);
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
          /* division by zero checks added to avoid NaN (brecht) */
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
          /* division by zero checks added to avoid NaN (brecht) */
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

void pseudoinverse_m4_m4(float Ainv[4][4], const float A_[4][4], float epsilon)
{
  /* compute Moore-Penrose pseudo inverse of matrix, singular values
   * below epsilon are ignored for stability (truncated SVD) */
  float A[4][4], V[4][4], W[4], Wm[4][4], U[4][4];
  int i;

  transpose_m4_m4(A, A_);
  svd_m4(V, W, U, A);
  transpose_m4(U);
  transpose_m4(V);

  zero_m4(Wm);
  for (i = 0; i < 4; i++) {
    Wm[i][i] = (W[i] < epsilon) ? 0.0f : 1.0f / W[i];
  }

  transpose_m4(V);

  mul_m4_series(Ainv, U, Wm, V);
}

void pseudoinverse_m3_m3(float Ainv[3][3], const float A[3][3], float epsilon)
{
  /* try regular inverse when possible, otherwise fall back to slow svd */
  if (!invert_m3_m3(Ainv, A)) {
    float tmp[4][4], tmpinv[4][4];

    copy_m4_m3(tmp, A);
    pseudoinverse_m4_m4(tmpinv, tmp, epsilon);
    copy_m3_m4(Ainv, tmpinv);
  }
}

bool has_zero_axis_m4(const float matrix[4][4])
{
  return len_squared_v3(matrix[0]) < FLT_EPSILON || len_squared_v3(matrix[1]) < FLT_EPSILON ||
         len_squared_v3(matrix[2]) < FLT_EPSILON;
}

void invert_m4_m4_safe(float Ainv[4][4], const float A[4][4])
{
  if (!invert_m4_m4(Ainv, A)) {
    float Atemp[4][4];

    copy_m4_m4(Atemp, A);

    /* Matrix is degenerate (e.g. 0 scale on some axis), ideally we should
     * never be in this situation, but try to invert it anyway with tweak.
     */
    Atemp[0][0] += 1e-8f;
    Atemp[1][1] += 1e-8f;
    Atemp[2][2] += 1e-8f;

    if (!invert_m4_m4(Ainv, Atemp)) {
      unit_m4(Ainv);
    }
  }
}

/**
 * #SpaceTransform struct encapsulates all needed data to convert between two coordinate spaces
 * (where conversion can be represented by a matrix multiplication).
 *
 * A SpaceTransform is initialized using:
 * - #BLI_SPACE_TRANSFORM_SETUP(&data,  ob1, ob2)
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
void BLI_space_transform_from_matrices(SpaceTransform *data,
                                       const float local[4][4],
                                       const float target[4][4])
{
  float itarget[4][4];
  invert_m4_m4(itarget, target);
  mul_m4_m4m4(data->local2target, itarget, local);
  invert_m4_m4(data->target2local, data->local2target);
}

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
