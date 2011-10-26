// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2009 Gael Guennebaud <gael.guennebaud@inria.fr>
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

#ifndef EIGEN2SUPPORT_H
#define EIGEN2SUPPORT_H

#if (!defined(EIGEN2_SUPPORT)) || (!defined(EIGEN_CORE_H))
#error Eigen2 support must be enabled by defining EIGEN2_SUPPORT before including any Eigen header
#endif

#include "src/Core/util/DisableStupidWarnings.h"

namespace Eigen {

/** \defgroup Eigen2Support_Module Eigen2 support module
  * This module provides a couple of deprecated functions improving the compatibility with Eigen2.
  *
  * To use it, define EIGEN2_SUPPORT before including any Eigen header
  * \code
  * #define EIGEN2_SUPPORT
  * \endcode
  *
  */

#include "src/Eigen2Support/Macros.h"
#include "src/Eigen2Support/Memory.h"
#include "src/Eigen2Support/Meta.h"
#include "src/Eigen2Support/Lazy.h"
#include "src/Eigen2Support/Cwise.h"
#include "src/Eigen2Support/CwiseOperators.h"
#include "src/Eigen2Support/TriangularSolver.h"
#include "src/Eigen2Support/Block.h"
#include "src/Eigen2Support/VectorBlock.h"
#include "src/Eigen2Support/Minor.h"
#include "src/Eigen2Support/MathFunctions.h"


} // namespace Eigen

#include "src/Core/util/ReenableStupidWarnings.h"

// Eigen2 used to include iostream
#include<iostream>

#define USING_PART_OF_NAMESPACE_EIGEN \
EIGEN_USING_MATRIX_TYPEDEFS \
using Eigen::Matrix; \
using Eigen::MatrixBase; \
using Eigen::ei_random; \
using Eigen::ei_real; \
using Eigen::ei_imag; \
using Eigen::ei_conj; \
using Eigen::ei_abs; \
using Eigen::ei_abs2; \
using Eigen::ei_sqrt; \
using Eigen::ei_exp; \
using Eigen::ei_log; \
using Eigen::ei_sin; \
using Eigen::ei_cos;

#endif // EIGEN2SUPPORT_H
