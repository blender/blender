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
 * The Original Code is Copyright (C) 2015 by Blender Foundation
 * All rights reserved.
 */

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

/********************************** Eigen Solvers *********************************/

bool BLI_eigen_solve_selfadjoint_m3(const float m3[3][3],
                                    float r_eigen_values[3],
                                    float r_eigen_vectors[3][3]);

void BLI_svd_m3(const float m3[3][3], float r_U[3][3], float r_S[3], float r_V[3][3]);

/***************************** Simple Solvers ************************************/

bool BLI_tridiagonal_solve(
    const float *a, const float *b, const float *c, const float *d, float *r_x, const int count);
bool BLI_tridiagonal_solve_cyclic(
    const float *a, const float *b, const float *c, const float *d, float *r_x, const int count);

/* Generic 3 variable Newton's method solver. */
typedef void (*Newton3D_DeltaFunc)(void *userdata, const float x[3], float r_delta[3]);
typedef void (*Newton3D_JacobianFunc)(void *userdata, const float x[3], float r_jacobian[3][3]);
typedef bool (*Newton3D_CorrectionFunc)(void *userdata,
                                        const float x[3],
                                        float step[3],
                                        float x_next[3]);

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

#ifdef __cplusplus
}
#endif
