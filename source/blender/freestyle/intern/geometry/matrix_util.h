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
 * The Original Code is:
 *   GXML/Graphite: Geometry and Graphics Programming Library + Utilities
 *   Copyright (C) 2000 Bruno Levy
 *   Contact: Bruno Levy
 *      levy@loria.fr
 *      ISA Project
 *      LORIA, INRIA Lorraine,
 *      Campus Scientifique, BP 239
 *      54506 VANDOEUVRE LES NANCY CEDEX
 *      FRANCE
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __MATRIX_UTIL__
#define __MATRIX_UTIL__

/** \file blender/freestyle/intern/geometry/matrix_util.h
 *  \ingroup freestyle
 *  \author Bruno Levy
 */

#include "../system/FreestyleConfig.h"

namespace Freestyle {

namespace OGF {

namespace MatrixUtil {

	/**
	 * computes the eigen values and eigen vectors of a semi definite symmetric matrix
	 *
	 * @param  matrix is stored in column symmetric storage, i.e.
	 *     matrix = { m11, m12, m22, m13, m23, m33, m14, m24, m34, m44 ... }
	 *     size = n(n+1)/2
	 *
	 * @param eigen_vectors (return) = { v1, v2, v3, ..., vn } 
	 *   where vk = vk0, vk1, ..., vkn
	 *     size = n^2, must be allocated by caller
	 *
	 * @param eigen_values  (return) are in decreasing order
	 *     size = n,   must be allocated by caller
	 */
	LIB_GEOMETRY_EXPORT 
	void semi_definite_symmetric_eigen(const double *mat, int n, double *eigen_vec, double *eigen_val);

}  // MatrixUtil namespace

}  // OGF namespace

} /* namespace Freestyle */

#endif  // __MATRIX_UTIL__
