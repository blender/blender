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

/** \file blender/blenlib/intern/math_solvers.c
 *  \ingroup bli
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLI_strict_flags.h"

#include "eigen3_capi.h"

/********************************** Eigen Solvers *********************************/

/**
 * \brief Compute the eigen values and/or vectors of given 3D symmetric (aka adjoint) matrix.
 *
 * \param m3 the 3D symmetric matrix.
 * \return r_eigen_values the computed eigen values (NULL if not needed).
 * \return r_eigen_vectors the computed eigen vectors (NULL if not needed).
 */
bool BLI_eigen_solve_selfadjoint_m3(const float m3[3][3], float r_eigen_values[3], float r_eigen_vectors[3][3])
{
#ifndef NDEBUG
	/* We must assert given matrix is self-adjoint (i.e. symmetric) */
	if ((m3[0][1] != m3[1][0]) ||
	    (m3[0][2] != m3[2][0]) ||
	    (m3[1][2] != m3[2][1]))
	{
		BLI_assert(0);
	}
#endif

	return EG3_self_adjoint_eigen_solve(3, (const float *)m3, r_eigen_values, (float *)r_eigen_vectors);
}
