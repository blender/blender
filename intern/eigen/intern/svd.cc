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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Bastien Montagne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __EIGEN3_SVD_C_API_CC__
#define __EIGEN3_SVD_C_API_CC__

/* Eigen gives annoying huge amount of warnings here, silence them! */
#ifdef __GNUC__
#  pragma GCC diagnostic ignored "-Wlogical-op"
#endif

#ifdef __EIGEN3_SVD_C_API_CC__  /* quiet warning */
#endif

#include <Eigen/Core>
#include <Eigen/SVD>

#include "svd.h"

using Eigen::JacobiSVD;

using Eigen::NoQRPreconditioner;

using Eigen::ComputeThinU;
using Eigen::ComputeThinV;

using Eigen::MatrixXf;
using Eigen::VectorXf;
using Eigen::Map;

void EIG_svd_square_matrix(const int size, const float *matrix, float *r_U, float *r_S, float *r_V)
{
	/* Since our matrix is squared, we can use thinU/V. */
	unsigned int flags = (r_U ? ComputeThinU : 0) | (r_V ? ComputeThinV : 0);

	/* Blender and Eigen matrices are both column-major. */
	JacobiSVD<MatrixXf, NoQRPreconditioner> svd(Map<MatrixXf>((float *)matrix, size, size), flags);

	if (r_U) {
		Map<MatrixXf>(r_U, size, size) = svd.matrixU();
	}

	if (r_S) {
		Map<VectorXf>(r_S, size) = svd.singularValues();
	}

	if (r_V) {
		Map<MatrixXf>(r_V, size, size) = svd.matrixV();
	}
}

#endif  /* __EIGEN3_SVD_C_API_CC__ */
