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
 * The Original Code is Copyright (C) 2018 Blender Foundation.
 * All rights reserved.
 */

#ifndef __EIGEN3_MATRIX_C_API_CC__
#define __EIGEN3_MATRIX_C_API_CC__

/* Eigen gives annoying huge amount of warnings here, silence them! */
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wlogical-op"
#endif

#ifdef __EIGEN3_MATRIX_C_API_CC__  /* quiet warning */
#endif

#include <Eigen/Core>
#include <Eigen/Dense>

#include "matrix.h"

using Eigen::Map;
using Eigen::Matrix4f;

bool EIG_invert_m4_m4(float inverse[4][4], const float matrix[4][4])
{
	Map<Matrix4f> M = Map<Matrix4f>((float*)matrix);
	Matrix4f R;
	bool invertible = true;
	M.computeInverseWithCheck(R, invertible, 0.0f);
	if (!invertible) {
		R = R.Zero();
	}
	memcpy(inverse, R.data(), sizeof(float)*4*4);
	return invertible;
}

#endif  /* __EIGEN3_MATRIX_C_API_CC__ */
