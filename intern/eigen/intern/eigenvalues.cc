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

#ifndef __EIGEN3_EIGENVALUES_C_API_CC__
#define __EIGEN3_EIGENVALUES_C_API_CC__

/* Eigen gives annoying huge amount of warnings here, silence them! */
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wlogical-op"
#endif

#include <Eigen/Core>
#include <Eigen/Eigenvalues>

#include "eigenvalues.h"

using Eigen::SelfAdjointEigenSolver;

using Eigen::MatrixXf;
using Eigen::VectorXf;
using Eigen::Map;

using Eigen::Success;

bool EIG_self_adjoint_eigen_solve(const int size, const float *matrix, float *r_eigen_values, float *r_eigen_vectors)
{
	SelfAdjointEigenSolver<MatrixXf> eigen_solver;

	/* Blender and Eigen matrices are both column-major. */
	eigen_solver.compute(Map<MatrixXf>((float *)matrix, size, size));

	if (eigen_solver.info() != Success) {
		return false;
	}

	if (r_eigen_values) {
		Map<VectorXf>(r_eigen_values, size) = eigen_solver.eigenvalues().transpose();
	}

	if (r_eigen_vectors) {
		Map<MatrixXf>(r_eigen_vectors, size, size) = eigen_solver.eigenvectors();
	}

	return true;
}

#endif  /* __EIGEN3_EIGENVALUES_C_API_CC__ */
