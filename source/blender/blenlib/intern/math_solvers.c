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

#include "eigen_capi.h"

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

	return EIG_self_adjoint_eigen_solve(3, (const float *)m3, r_eigen_values, (float *)r_eigen_vectors);
}

/**
 * \brief Compute the SVD (Singular Values Decomposition) of given 3D  matrix (m3 = USV*).
 *
 * \param m3 the matrix to decompose.
 * \return r_U the computed left singular vector of \a m3 (NULL if not needed).
 * \return r_S the computed singular values of \a m3 (NULL if not needed).
 * \return r_V the computed right singular vector of \a m3 (NULL if not needed).
 */
void BLI_svd_m3(const float m3[3][3], float r_U[3][3], float r_S[3], float r_V[3][3])
{
	EIG_svd_square_matrix(3, (const float *)m3, (float *)r_U, (float *)r_S, (float *)r_V);
}

/***************************** Simple Solvers ************************************/

/**
 * \brief Solve a tridiagonal system of equations:
 *
 * a[i] * r_x[i-1] + b[i] * r_x[i] + c[i] * r_x[i+1] = d[i]
 *
 * Ignores a[0] and c[count-1]. Uses the Thomas algorithm, e.g. see wiki.
 *
 * \param r_x output vector, may be shared with any of the input ones
 * \return true if success
 */
bool BLI_tridiagonal_solve(const float *a, const float *b, const float *c, const float *d, float *r_x, const int count)
{
	if (count < 1)
		return false;

	size_t bytes = sizeof(double) * (unsigned)count;
	double *c1 = (double *)MEM_mallocN(bytes * 2, "tridiagonal_c1d1");
	double *d1 = c1 + count;

	if (!c1)
		return false;

	int i;
	double c_prev, d_prev, x_prev;

	/* forward pass */

	c1[0] = c_prev = ((double)c[0]) / b[0];
	d1[0] = d_prev = ((double)d[0]) / b[0];

	for (i = 1; i < count; i++) {
		double denum = b[i] - a[i] * c_prev;

		c1[i] = c_prev = c[i] / denum;
		d1[i] = d_prev = (d[i] - a[i] * d_prev) / denum;
	}

	/* back pass */

	x_prev = d_prev;
	r_x[--i] = ((float)x_prev);

	while (--i >= 0) {
		x_prev = d1[i] - c1[i] * x_prev;
		r_x[i] = ((float)x_prev);
	}

	MEM_freeN(c1);

	return isfinite(x_prev);
}

/**
 * \brief Solve a possibly cyclic tridiagonal system using the Sherman-Morrison formula.
 *
 * \param r_x output vector, may be shared with any of the input ones
 * \return true if success
 */
bool BLI_tridiagonal_solve_cyclic(const float *a, const float *b, const float *c, const float *d, float *r_x, const int count)
{
	if (count < 1)
		return false;

	float a0 = a[0], cN = c[count - 1];

	/* if not really cyclic, fall back to the simple solver */
	if (a0 == 0.0f && cN == 0.0f) {
		return BLI_tridiagonal_solve(a, b, c, d, r_x, count);
	}

	size_t bytes = sizeof(float) * (unsigned)count;
	float *tmp = (float *)MEM_mallocN(bytes * 2, "tridiagonal_ex");
	float *b2 = tmp + count;

	if (!tmp)
		return false;

	/* prepare the noncyclic system; relies on tridiagonal_solve ignoring values */
	memcpy(b2, b, bytes);
	b2[0] -= a0;
	b2[count - 1] -= cN;

	memset(tmp, 0, bytes);
	tmp[0] = a0;
	tmp[count - 1] = cN;

	/* solve for partial solution and adjustment vector */
	bool success =
		BLI_tridiagonal_solve(a, b2, c, tmp, tmp, count) &&
		BLI_tridiagonal_solve(a, b2, c, d, r_x, count);

	/* apply adjustment */
	if (success) {
		float coeff = (r_x[0] + r_x[count - 1]) / (1.0f + tmp[0] + tmp[count - 1]);

		for (int i = 0; i < count; i++) {
			r_x[i] -= coeff * tmp[i];
		}
	}

	MEM_freeN(tmp);

	return success;
}

