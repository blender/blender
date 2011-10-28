// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
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

#ifndef EIGEN_SPARSE_DOT_H
#define EIGEN_SPARSE_DOT_H

template<typename Derived>
template<typename OtherDerived>
typename internal::traits<Derived>::Scalar
SparseMatrixBase<Derived>::dot(const MatrixBase<OtherDerived>& other) const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(OtherDerived)
  EIGEN_STATIC_ASSERT_SAME_VECTOR_SIZE(Derived,OtherDerived)
  EIGEN_STATIC_ASSERT((internal::is_same<Scalar, typename OtherDerived::Scalar>::value),
    YOU_MIXED_DIFFERENT_NUMERIC_TYPES__YOU_NEED_TO_USE_THE_CAST_METHOD_OF_MATRIXBASE_TO_CAST_NUMERIC_TYPES_EXPLICITLY)

  eigen_assert(size() == other.size());
  eigen_assert(other.size()>0 && "you are using a non initialized vector");

  typename Derived::InnerIterator i(derived(),0);
  Scalar res = 0;
  while (i)
  {
    res += internal::conj(i.value()) * other.coeff(i.index());
    ++i;
  }
  return res;
}

template<typename Derived>
template<typename OtherDerived>
typename internal::traits<Derived>::Scalar
SparseMatrixBase<Derived>::dot(const SparseMatrixBase<OtherDerived>& other) const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(OtherDerived)
  EIGEN_STATIC_ASSERT_SAME_VECTOR_SIZE(Derived,OtherDerived)
  EIGEN_STATIC_ASSERT((internal::is_same<Scalar, typename OtherDerived::Scalar>::value),
    YOU_MIXED_DIFFERENT_NUMERIC_TYPES__YOU_NEED_TO_USE_THE_CAST_METHOD_OF_MATRIXBASE_TO_CAST_NUMERIC_TYPES_EXPLICITLY)

  eigen_assert(size() == other.size());

  typename Derived::InnerIterator i(derived(),0);
  typename OtherDerived::InnerIterator j(other.derived(),0);
  Scalar res = 0;
  while (i && j)
  {
    if (i.index()==j.index())
    {
      res += internal::conj(i.value()) * j.value();
      ++i; ++j;
    }
    else if (i.index()<j.index())
      ++i;
    else
      ++j;
  }
  return res;
}

template<typename Derived>
inline typename NumTraits<typename internal::traits<Derived>::Scalar>::Real
SparseMatrixBase<Derived>::squaredNorm() const
{
  return internal::real((*this).cwiseAbs2().sum());
}

template<typename Derived>
inline typename NumTraits<typename internal::traits<Derived>::Scalar>::Real
SparseMatrixBase<Derived>::norm() const
{
  return internal::sqrt(squaredNorm());
}

#endif // EIGEN_SPARSE_DOT_H
