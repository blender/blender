// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
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

#ifdef EIGEN_EXTERN_INSTANTIATIONS
#undef EIGEN_EXTERN_INSTANTIATIONS
#endif

#include "../../Core"

namespace Eigen
{

#define EIGEN_INSTANTIATE_PRODUCT(TYPE) \
template static void ei_cache_friendly_product<TYPE>( \
  int _rows, int _cols, int depth, \
  bool _lhsRowMajor, const TYPE* _lhs, int _lhsStride, \
  bool _rhsRowMajor, const TYPE* _rhs, int _rhsStride, \
  bool resRowMajor, TYPE* res, int resStride)

EIGEN_INSTANTIATE_PRODUCT(float);
EIGEN_INSTANTIATE_PRODUCT(double);
EIGEN_INSTANTIATE_PRODUCT(int);
EIGEN_INSTANTIATE_PRODUCT(std::complex<float>);
EIGEN_INSTANTIATE_PRODUCT(std::complex<double>);

}
