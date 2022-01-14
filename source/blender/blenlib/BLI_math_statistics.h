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

/* -------------------------------------------------------------------- */
/** \name Covariance Matrices
 * \{ */

/**
 * \brief Compute the covariance matrix of given set of nD coordinates.
 *
 * \param n: the dimension of the vectors (and hence, of the covariance matrix to compute).
 * \param cos_vn: the nD points to compute covariance from.
 * \param nbr_cos_vn: the number of nD coordinates in cos_vn.
 * \param center: the center (or mean point) of cos_vn. If NULL,
 * it is assumed cos_vn is already centered.
 * \param use_sample_correction: whether to apply sample correction
 *                              (i.e. get 'sample variance' instead of 'population variance').
 * \return r_covmat the computed covariance matrix.
 */
void BLI_covariance_m_vn_ex(int n,
                            const float *cos_vn,
                            int nbr_cos_vn,
                            const float *center,
                            bool use_sample_correction,
                            float *r_covmat);
/**
 * \brief Compute the covariance matrix of given set of 3D coordinates.
 *
 * \param cos_v3: the 3D points to compute covariance from.
 * \param nbr_cos_v3: the number of 3D coordinates in cos_v3.
 * \return r_covmat the computed covariance matrix.
 * \return r_center the computed center (mean) of 3D points (may be NULL).
 */
void BLI_covariance_m3_v3n(const float (*cos_v3)[3],
                           int nbr_cos_v3,
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
