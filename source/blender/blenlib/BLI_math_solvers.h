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
 * The Original Code is Copyright (C) 2015 by Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 * */

#ifndef __BLI_MATH_SOLVERS_H__
#define __BLI_MATH_SOLVERS_H__

/** \file BLI_math_solvers.h
 *  \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_compiler_attrs.h"
#include "BLI_math_inline.h"

#ifdef BLI_MATH_GCC_WARN_PRAGMA
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wredundant-decls"
#endif

/********************************** Eigen Solvers *********************************/

bool BLI_eigen_solve_selfadjoint_m3(const float m3[3][3], float r_eigen_values[3], float r_eigen_vectors[3][3]);

void BLI_svd_m3(const float m3[3][3], float r_U[3][3], float r_S[], float r_V[3][3]);

/***************************** Simple Solvers ************************************/

bool BLI_tridiagonal_solve(const float *a, const float *b, const float *c, const float *d, float *r_x, const int count);
bool BLI_tridiagonal_solve_cyclic(const float *a, const float *b, const float *c, const float *d, float *r_x, const int count);

/**************************** Inline Definitions ******************************/
#if 0  /* None so far. */
#  if BLI_MATH_DO_INLINE
#    include "intern/math_geom_inline.c"
#  endif
#endif

#ifdef BLI_MATH_GCC_WARN_PRAGMA
#  pragma GCC diagnostic pop
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BLI_MATH_SOLVERS_H__ */
