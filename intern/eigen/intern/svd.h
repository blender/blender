/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2015 Blender Foundation */

/** \file
 * \ingroup intern_eigen
 */

#ifndef __EIGEN3_SVD_C_API_H__
#define __EIGEN3_SVD_C_API_H__

#ifdef __cplusplus
extern "C" {
#endif

void EIG_svd_square_matrix(
    const int size, const float *matrix, float *r_U, float *r_S, float *r_V);

#ifdef __cplusplus
}
#endif

#endif /* __EIGEN3_SVD_C_API_H__ */
