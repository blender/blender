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

#ifndef EIGEN_SPARSE_FUZZY_H
#define EIGEN_SPARSE_FUZZY_H

// template<typename Derived>
// template<typename OtherDerived>
// bool SparseMatrixBase<Derived>::isApprox(
//   const OtherDerived& other,
//   typename NumTraits<Scalar>::Real prec
// ) const
// {
//   const typename ei_nested<Derived,2>::type nested(derived());
//   const typename ei_nested<OtherDerived,2>::type otherNested(other.derived());
//   return    (nested - otherNested).cwise().abs2().sum()
//          <= prec * prec * std::min(nested.cwise().abs2().sum(), otherNested.cwise().abs2().sum());
// }

#endif // EIGEN_SPARSE_FUZZY_H
