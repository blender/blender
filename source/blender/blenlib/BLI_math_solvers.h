/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2015 Blender Foundation. All rights reserved. */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_compiler_attrs.h"
#include "BLI_math_inline.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef BLI_MATH_GCC_WARN_PRAGMA
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wredundant-decls"
#endif

/* -------------------------------------------------------------------- */
/** \name Eigen Solvers
 * \{ */

/**
 * \brief Compute the eigen values and/or vectors of given 3D symmetric (aka adjoint) matrix.
 *
 * \param m3: the 3D symmetric matrix.
 * \return r_eigen_values the computed eigen values (NULL if not needed).
 * \return r_eigen_vectors the computed eigen vectors (NULL if not needed).
 */
bool BLI_eigen_solve_selfadjoint_m3(const float m3[3][3],
                                    float r_eigen_values[3],
                                    float r_eigen_vectors[3][3]);

/**
 * \brief Compute the SVD (Singular Values Decomposition) of given 3D matrix (m3 = USV*).
 *
 * \param m3: the matrix to decompose.
 * \return r_U the computed left singular vector of \a m3 (NULL if not needed).
 * \return r_S the computed singular values of \a m3 (NULL if not needed).
 * \return r_V the computed right singular vector of \a m3 (NULL if not needed).
 */
void BLI_svd_m3(const float m3[3][3], float r_U[3][3], float r_S[3], float r_V[3][3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Simple Solvers
 * \{ */

/**
 * \brief Solve a tridiagonal system of equations:
 *
 * a[i] * r_x[i-1] + b[i] * r_x[i] + c[i] * r_x[i+1] = d[i]
 *
 * Ignores a[0] and c[count-1]. Uses the Thomas algorithm, e.g. see wiki.
 *
 * \param r_x: output vector, may be shared with any of the input ones
 * \return true if success
 */
bool BLI_tridiagonal_solve(
    const float *a, const float *b, const float *c, const float *d, float *r_x, int count);
/**
 * \brief Solve a possibly cyclic tridiagonal system using the Sherman-Morrison formula.
 *
 * \param r_x: output vector, may be shared with any of the input ones
 * \return true if success
 */
bool BLI_tridiagonal_solve_cyclic(
    const float *a, const float *b, const float *c, const float *d, float *r_x, int count);

/**
 * Generic 3 variable Newton's method solver.
 */
typedef void (*Newton3D_DeltaFunc)(void *userdata, const float x[3], float r_delta[3]);
typedef void (*Newton3D_JacobianFunc)(void *userdata, const float x[3], float r_jacobian[3][3]);
typedef bool (*Newton3D_CorrectionFunc)(void *userdata,
                                        const float x[3],
                                        float step[3],
                                        float x_next[3]);

/**
 * \brief Solve a generic f(x) = 0 equation using Newton's method.
 *
 * \param func_delta: Callback computing the value of f(x).
 * \param func_jacobian: Callback computing the Jacobian matrix of the function at x.
 * \param func_correction: Callback for forcing the search into an arbitrary custom domain.
 * May be NULL.
 * \param userdata: Data for the callbacks.
 * \param epsilon: Desired precision.
 * \param max_iterations: Limit on the iterations.
 * \param trace: Enables logging to console.
 * \param x_init: Initial solution vector.
 * \param result: Final result.
 * \return true if success
 */
bool BLI_newton3d_solve(Newton3D_DeltaFunc func_delta,
                        Newton3D_JacobianFunc func_jacobian,
                        Newton3D_CorrectionFunc func_correction,
                        void *userdata,
                        float epsilon,
                        int max_iterations,
                        bool trace,
                        const float x_init[3],
                        float result[3]);

#ifdef BLI_MATH_GCC_WARN_PRAGMA
#  pragma GCC diagnostic pop
#endif

/** \} */

#ifdef __cplusplus
}
#endif
