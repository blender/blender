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
/** \name Covariance Matrices
 * \{ */

/**
 * \brief Compute the covariance matrix of given set of nD coordinates.
 *
 * \param n: the dimension of the vectors (and hence, of the covariance matrix to compute).
 * \param cos_vn: the nD points to compute covariance from.
 * \param cos_vn_num: the number of nD coordinates in cos_vn.
 * \param center: the center (or mean point) of cos_vn. If NULL,
 * it is assumed cos_vn is already centered.
 * \param use_sample_correction: whether to apply sample correction
 *                              (i.e. get 'sample variance' instead of 'population variance').
 * \return r_covmat the computed covariance matrix.
 */
void BLI_covariance_m_vn_ex(int n,
                            const float *cos_vn,
                            int cos_vn_num,
                            const float *center,
                            bool use_sample_correction,
                            float *r_covmat);
/**
 * \brief Compute the covariance matrix of given set of 3D coordinates.
 *
 * \param cos_v3: the 3D points to compute covariance from.
 * \param cos_v3_num: the number of 3D coordinates in cos_v3.
 * \return r_covmat the computed covariance matrix.
 * \return r_center the computed center (mean) of 3D points (may be NULL).
 */
void BLI_covariance_m3_v3n(const float (*cos_v3)[3],
                           int cos_v3_num,
                           bool use_sample_correction,
                           float r_covmat[3][3],
                           float r_center[3]);

#ifdef BLI_MATH_GCC_WARN_PRAGMA
#  pragma GCC diagnostic pop
#endif

/** \} */

#ifdef __cplusplus
}
#endif
