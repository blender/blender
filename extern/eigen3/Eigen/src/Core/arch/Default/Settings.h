// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
//
// Eigen is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// Alternatively, you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// Eigen is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License or the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License and a copy of the GNU General Public License along with
// Eigen. If not, see <http://www.gnu.org/licenses/>.


/* All the parameters defined in this file can be specialized in the
 * architecture specific files, and/or by the user.
 * More to come... */

#ifndef EIGEN_DEFAULT_SETTINGS_H
#define EIGEN_DEFAULT_SETTINGS_H

/** Defines the maximal loop size to enable meta unrolling of loops.
  * Note that the value here is expressed in Eigen's own notion of "number of FLOPS",
  * it does not correspond to the number of iterations or the number of instructions
  */
#ifndef EIGEN_UNROLLING_LIMIT
#define EIGEN_UNROLLING_LIMIT 100
#endif

/** Defines the threshold between a "small" and a "large" matrix.
  * This threshold is mainly used to select the proper product implementation.
  */
#ifndef EIGEN_CACHEFRIENDLY_PRODUCT_THRESHOLD
#define EIGEN_CACHEFRIENDLY_PRODUCT_THRESHOLD 8
#endif

/** Defines the maximal width of the blocks used in the triangular product and solver
  * for vectors (level 2 blas xTRMV and xTRSV). The default is 8.
  */
#ifndef EIGEN_TUNE_TRIANGULAR_PANEL_WIDTH
#define EIGEN_TUNE_TRIANGULAR_PANEL_WIDTH 8
#endif


/** Defines the default number of registers available for that architecture.
  * Currently it must be 8 or 16. Other values will fail.
  */
#ifndef EIGEN_ARCH_DEFAULT_NUMBER_OF_REGISTERS
#define EIGEN_ARCH_DEFAULT_NUMBER_OF_REGISTERS 8
#endif

#endif // EIGEN_DEFAULT_SETTINGS_H
