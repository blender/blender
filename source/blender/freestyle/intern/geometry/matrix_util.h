/*
 *  GXML/Graphite: Geometry and Graphics Programming Library + Utilities
 *  Copyright (C) 2000 Bruno Levy
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  If you modify this software, you should include a notice giving the
 *  name of the person performing the modification, the date of modification,
 *  and the reason for such modification.
 *
 *  Contact: Bruno Levy
 *
 *     levy@loria.fr
 *
 *     ISA Project
 *     LORIA, INRIA Lorraine, 
 *     Campus Scientifique, BP 239
 *     54506 VANDOEUVRE LES NANCY CEDEX 
 *     FRANCE
 *
 *  Note that the GNU General Public License does not permit incorporating
 *  the Software into proprietary programs. 
 */

#ifndef __MATRIX_UTIL__
#define __MATRIX_UTIL__

# include "../system/FreestyleConfig.h"

namespace OGF {

    namespace MatrixUtil {

        /**
         * computes the eigen values and eigen vectors   
         * of a semi definite symmetric matrix
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
        void semi_definite_symmetric_eigen(
            const double *mat, int n, double *eigen_vec, double *eigen_val
        ) ;
    
    }
}

#endif
