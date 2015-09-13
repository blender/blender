/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is Copyright (C) 2015 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 * */

/** \file blender/blenlib/intern/math_statistics.c
 *  \ingroup bli
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLI_strict_flags.h"

/********************************** Covariance Matrices *********************************/

/**
 * \brief Compute the covariance matrix of given set of nD coordinates.
 *
 * \param n the dimension of the vectors (and hence, of the covariance matrix to compute).
 * \param cos_vn the nD points to compute covariance from.
 * \param nbr_cos_vn the number of nD coordinates in cos_vn.
 * \param center the center (or mean point) of cos_vn. If NULL, it is assumed cos_vn is already centered.
 * \param use_sample_correction whether to apply sample correction
 *                              (i.e. get 'sample varince' instead of 'population variance').
 * \return r_covmat the computed covariance matrix.
 */
void BLI_covariance_m_vn_ex(
        const int n, const float *cos_vn, const int nbr_cos_vn, const float *center, const bool use_sample_correction,
        float *r_covmat)
{
	int i, j, k;

	/* Note about that division: see https://en.wikipedia.org/wiki/Bessel%27s_correction.
	 * In a nutshell, it must be 1 / (n - 1) for 'sample data', and 1 / n for 'population data'...
	 */
	const float covfac = 1.0f / (float)(use_sample_correction ? nbr_cos_vn - 1 : nbr_cos_vn);

	memset(r_covmat, 0, sizeof(*r_covmat) * (size_t)(n * n));

#pragma omp parallel for default(shared) private(i, j, k) schedule(static) if ((nbr_cos_vn * n) >= 10000)
	for (i = 0; i < n; i++) {
		for (j = i; j < n; j++) {
			r_covmat[i * n + j] = 0.0f;
			if (center) {
				for (k = 0; k < nbr_cos_vn; k++) {
					r_covmat[i * n + j] += (cos_vn[k * n + i] - center[i]) * (cos_vn[k * n + j] - center[j]);
				}
			}
			else {
				for (k = 0; k < nbr_cos_vn; k++) {
					r_covmat[i * n + j] += cos_vn[k * n + i] * cos_vn[k * n + j];
				}
			}
			r_covmat[i * n + j] *= covfac;
		}
	}
	/* Covariance matrices are always symetrical, so we can compute only one half of it (as done above),
	 * and copy it to the other half! */
	for (i = 1; i < n; i++) {
		for (j = 0; j < i; j++) {
			r_covmat[i * n + j] = r_covmat[j * n + i];
		}
	}
}

/**
 * \brief Compute the covariance matrix of given set of 3D coordinates.
 *
 * \param cos_v3 the 3D points to compute covariance from.
 * \param nbr_cos_v3 the number of 3D coordinates in cos_v3.
 * \return r_covmat the computed covariance matrix.
 * \return r_center the computed center (mean) of 3D points (may be NULL).
 */
void BLI_covariance_m3_v3n(
        const float (*cos_v3)[3], const int nbr_cos_v3, const bool use_sample_correction,
        float r_covmat[3][3], float r_center[3])
{
	float center[3];
	const float mean_fac = 1.0f / (float)nbr_cos_v3;
	int i;

	zero_v3(center);
	for (i = 0; i < nbr_cos_v3; i++) {
		/* Applying mean_fac here rather than once at the end reduce compute errors... */
		madd_v3_v3fl(center, cos_v3[i], mean_fac);
	}

	if (r_center) {
		copy_v3_v3(r_center, center);
	}

	BLI_covariance_m_vn_ex(3, (const float *)cos_v3, nbr_cos_v3, center, use_sample_correction, (float *)r_covmat);
}
