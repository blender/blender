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

#ifndef EIGEN_SPARSEREDUX_H
#define EIGEN_SPARSEREDUX_H

template<typename Derived>
typename ei_traits<Derived>::Scalar
SparseMatrixBase<Derived>::sum() const
{
  ei_assert(rows()>0 && cols()>0 && "you are using a non initialized matrix");
  Scalar res = 0;
  for (int j=0; j<outerSize(); ++j)
    for (typename Derived::InnerIterator iter(derived(),j); iter; ++iter)
      res += iter.value();
  return res;
}

#endif // EIGEN_SPARSEREDUX_H
