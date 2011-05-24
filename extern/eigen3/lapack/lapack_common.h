// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2010-2011 Gael Guennebaud <gael.guennebaud@inria.fr>
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

#ifndef EIGEN_LAPACK_COMMON_H
#define EIGEN_LAPACK_COMMON_H

#include "../blas/common.h"

#define EIGEN_LAPACK_FUNC(FUNC,ARGLIST)               \
  extern "C" { int EIGEN_BLAS_FUNC(FUNC) ARGLIST; }   \
  int EIGEN_BLAS_FUNC(FUNC) ARGLIST

typedef Eigen::Map<Eigen::Transpositions<Eigen::Dynamic,Eigen::Dynamic,int> > PivotsType;



#endif // EIGEN_LAPACK_COMMON_H
