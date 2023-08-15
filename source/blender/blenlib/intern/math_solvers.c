/* SPDX-FileCopyrightText: 2015 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_math_base.h"
#include "BLI_math_matrix.h"
#include "BLI_math_solvers.h"
#include "BLI_math_vector.h"
#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_strict_flags.h"

#include "eigen_capi.h"

#include <string.h>

/********************************** Eigen Solvers *********************************/

bool BLI_eigen_solve_selfadjoint_m3(const float m3[3][3],
                                    float r_eigen_values[3],
                                    float r_eigen_vectors[3][3])
{
#ifndef NDEBUG
  /* We must assert given matrix is self-adjoint (i.e. symmetric) */
  if ((m3[0][1] != m3[1][0]) || (m3[0][2] != m3[2][0]) || (m3[1][2] != m3[2][1])) {
    BLI_assert(0);
  }
#endif

  return EIG_self_adjoint_eigen_solve(
      3, (const float *)m3, r_eigen_values, (float *)r_eigen_vectors);
}

void BLI_svd_m3(const float m3[3][3], float r_U[3][3], float r_S[3], float r_V[3][3])
{
  EIG_svd_square_matrix(3, (const float *)m3, (float *)r_U, (float *)r_S, (float *)r_V);
}

/***************************** Simple Solvers ************************************/

bool BLI_tridiagonal_solve(
    const float *a, const float *b, const float *c, const float *d, float *r_x, const int count)
{
  if (count < 1) {
    return false;
  }

  size_t bytes = sizeof(double) * (uint)count;
  double *c1 = (double *)MEM_mallocN(bytes * 2, "tridiagonal_c1d1");
  double *d1 = c1 + count;

  if (!c1) {
    return false;
  }

  int i;
  double c_prev, d_prev, x_prev;

  /* forward pass */

  c1[0] = c_prev = ((double)c[0]) / b[0];
  d1[0] = d_prev = ((double)d[0]) / b[0];

  for (i = 1; i < count; i++) {
    double denum = b[i] - a[i] * c_prev;

    c1[i] = c_prev = c[i] / denum;
    d1[i] = d_prev = (d[i] - a[i] * d_prev) / denum;
  }

  /* back pass */

  x_prev = d_prev;
  r_x[--i] = ((float)x_prev);

  while (--i >= 0) {
    x_prev = d1[i] - c1[i] * x_prev;
    r_x[i] = ((float)x_prev);
  }

  MEM_freeN(c1);

  return isfinite(x_prev);
}

bool BLI_tridiagonal_solve_cyclic(
    const float *a, const float *b, const float *c, const float *d, float *r_x, const int count)
{
  if (count < 1) {
    return false;
  }

  /* Degenerate case not handled correctly by the generic formula. */
  if (count == 1) {
    r_x[0] = d[0] / (a[0] + b[0] + c[0]);

    return isfinite(r_x[0]);
  }

  /* Degenerate case that works but can be simplified. */
  if (count == 2) {
    const float a2[2] = {0, a[1] + c[1]};
    const float c2[2] = {a[0] + c[0], 0};

    return BLI_tridiagonal_solve(a2, b, c2, d, r_x, count);
  }

  /* If not really cyclic, fall back to the simple solver. */
  float a0 = a[0], cN = c[count - 1];

  if (a0 == 0.0f && cN == 0.0f) {
    return BLI_tridiagonal_solve(a, b, c, d, r_x, count);
  }

  size_t bytes = sizeof(float) * (uint)count;
  float *tmp = (float *)MEM_mallocN(bytes * 2, "tridiagonal_ex");
  float *b2 = tmp + count;

  if (!tmp) {
    return false;
  }

  /* Prepare the non-cyclic system; relies on tridiagonal_solve ignoring values. */
  memcpy(b2, b, bytes);
  b2[0] -= a0;
  b2[count - 1] -= cN;

  memset(tmp, 0, bytes);
  tmp[0] = a0;
  tmp[count - 1] = cN;

  /* solve for partial solution and adjustment vector */
  bool success = BLI_tridiagonal_solve(a, b2, c, tmp, tmp, count) &&
                 BLI_tridiagonal_solve(a, b2, c, d, r_x, count);

  /* apply adjustment */
  if (success) {
    float coeff = (r_x[0] + r_x[count - 1]) / (1.0f + tmp[0] + tmp[count - 1]);

    for (int i = 0; i < count; i++) {
      r_x[i] -= coeff * tmp[i];
    }
  }

  MEM_freeN(tmp);

  return success;
}

bool BLI_newton3d_solve(Newton3D_DeltaFunc func_delta,
                        Newton3D_JacobianFunc func_jacobian,
                        Newton3D_CorrectionFunc func_correction,
                        void *userdata,
                        float epsilon,
                        int max_iterations,
                        bool trace,
                        const float x_init[3],
                        float result[3])
{
  float fdelta[3], fdeltav, next_fdeltav;
  float jacobian[3][3], step[3], x[3], x_next[3];

  epsilon *= epsilon;

  copy_v3_v3(x, x_init);

  func_delta(userdata, x, fdelta);
  fdeltav = len_squared_v3(fdelta);

  if (trace) {
    printf("START (%g, %g, %g) %g %g\n", x[0], x[1], x[2], fdeltav, epsilon);
  }

  for (int i = 0; i == 0 || (i < max_iterations && fdeltav > epsilon); i++) {
    /* Newton's method step. */
    func_jacobian(userdata, x, jacobian);

    if (!invert_m3(jacobian)) {
      return false;
    }

    mul_v3_m3v3(step, jacobian, fdelta);
    sub_v3_v3v3(x_next, x, step);

    /* Custom out-of-bounds value correction. */
    if (func_correction) {
      if (trace) {
        printf("%3d * (%g, %g, %g)\n", i, x_next[0], x_next[1], x_next[2]);
      }

      if (!func_correction(userdata, x, step, x_next)) {
        return false;
      }
    }

    func_delta(userdata, x_next, fdelta);
    next_fdeltav = len_squared_v3(fdelta);

    if (trace) {
      printf("%3d ? (%g, %g, %g) %g\n", i, x_next[0], x_next[1], x_next[2], next_fdeltav);
    }

    /* Line search correction. */
    while (next_fdeltav > fdeltav && next_fdeltav > epsilon) {
      float g0 = sqrtf(fdeltav), g1 = sqrtf(next_fdeltav);
      float g01 = -g0 / len_v3(step);
      float det = 2.0f * (g1 - g0 - g01);
      float l = (det == 0.0f) ? 0.1f : -g01 / det;
      CLAMP_MIN(l, 0.1f);

      mul_v3_fl(step, l);
      sub_v3_v3v3(x_next, x, step);

      func_delta(userdata, x_next, fdelta);
      next_fdeltav = len_squared_v3(fdelta);

      if (trace) {
        printf("%3d . (%g, %g, %g) %g\n", i, x_next[0], x_next[1], x_next[2], next_fdeltav);
      }
    }

    copy_v3_v3(x, x_next);
    fdeltav = next_fdeltav;
  }

  bool success = (fdeltav <= epsilon);

  if (trace) {
    printf("%s  (%g, %g, %g) %g\n", success ? "OK  " : "FAIL", x[0], x[1], x[2], fdeltav);
  }

  copy_v3_v3(result, x);
  return success;
}
