/* SPDX-FileCopyrightText: 2015 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_eigen
 */

#ifndef __EIGEN3_EIGENVALUES_C_API_H__
#define __EIGEN3_EIGENVALUES_C_API_H__

#ifdef __cplusplus
extern "C" {
#endif

bool EIG_self_adjoint_eigen_solve(const int size,
                                  const float *matrix,
                                  float *r_eigen_values,
                                  float *r_eigen_vectors);

#ifdef __cplusplus
}
#endif

#endif /* __EIGEN3_EIGENVALUES_C_API_H__ */
