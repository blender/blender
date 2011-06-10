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

#ifndef EIGEN_SPARSEUTIL_H
#define EIGEN_SPARSEUTIL_H

#ifdef NDEBUG
#define EIGEN_DBG_SPARSE(X)
#else
#define EIGEN_DBG_SPARSE(X) X
#endif

#define EIGEN_SPARSE_INHERIT_ASSIGNMENT_OPERATOR(Derived, Op) \
template<typename OtherDerived> \
EIGEN_STRONG_INLINE Derived& operator Op(const Eigen::SparseMatrixBase<OtherDerived>& other) \
{ \
  return Base::operator Op(other.derived()); \
} \
EIGEN_STRONG_INLINE Derived& operator Op(const Derived& other) \
{ \
  return Base::operator Op(other); \
}

#define EIGEN_SPARSE_INHERIT_SCALAR_ASSIGNMENT_OPERATOR(Derived, Op) \
template<typename Other> \
EIGEN_STRONG_INLINE Derived& operator Op(const Other& scalar) \
{ \
  return Base::operator Op(scalar); \
}

#define EIGEN_SPARSE_INHERIT_ASSIGNMENT_OPERATORS(Derived) \
EIGEN_SPARSE_INHERIT_ASSIGNMENT_OPERATOR(Derived, =) \
EIGEN_SPARSE_INHERIT_ASSIGNMENT_OPERATOR(Derived, +=) \
EIGEN_SPARSE_INHERIT_ASSIGNMENT_OPERATOR(Derived, -=) \
EIGEN_SPARSE_INHERIT_SCALAR_ASSIGNMENT_OPERATOR(Derived, *=) \
EIGEN_SPARSE_INHERIT_SCALAR_ASSIGNMENT_OPERATOR(Derived, /=)

#define _EIGEN_SPARSE_PUBLIC_INTERFACE(Derived, BaseClass) \
  typedef BaseClass Base; \
  typedef typename Eigen::internal::traits<Derived>::Scalar Scalar; \
  typedef typename Eigen::NumTraits<Scalar>::Real RealScalar; \
  typedef typename Eigen::internal::nested<Derived>::type Nested; \
  typedef typename Eigen::internal::traits<Derived>::StorageKind StorageKind; \
  typedef typename Eigen::internal::traits<Derived>::Index Index; \
  enum { RowsAtCompileTime = Eigen::internal::traits<Derived>::RowsAtCompileTime, \
        ColsAtCompileTime = Eigen::internal::traits<Derived>::ColsAtCompileTime, \
        Flags = Eigen::internal::traits<Derived>::Flags, \
        CoeffReadCost = Eigen::internal::traits<Derived>::CoeffReadCost, \
        SizeAtCompileTime = Base::SizeAtCompileTime, \
        IsVectorAtCompileTime = Base::IsVectorAtCompileTime }; \
  using Base::derived; \
  using Base::const_cast_derived;

#define EIGEN_SPARSE_PUBLIC_INTERFACE(Derived) \
  _EIGEN_SPARSE_PUBLIC_INTERFACE(Derived, Eigen::SparseMatrixBase<Derived>)

const int CoherentAccessPattern     = 0x1;
const int InnerRandomAccessPattern  = 0x2 | CoherentAccessPattern;
const int OuterRandomAccessPattern  = 0x4 | CoherentAccessPattern;
const int RandomAccessPattern       = 0x8 | OuterRandomAccessPattern | InnerRandomAccessPattern;

template<typename Derived> class SparseMatrixBase;
template<typename _Scalar, int _Flags = 0, typename _Index = int>  class SparseMatrix;
template<typename _Scalar, int _Flags = 0, typename _Index = int>  class DynamicSparseMatrix;
template<typename _Scalar, int _Flags = 0, typename _Index = int>  class SparseVector;
template<typename _Scalar, int _Flags = 0, typename _Index = int>  class MappedSparseMatrix;

template<typename MatrixType, int Size>           class SparseInnerVectorSet;
template<typename MatrixType, int Mode>           class SparseTriangularView;
template<typename MatrixType, unsigned int UpLo>  class SparseSelfAdjointView;
template<typename Lhs, typename Rhs>              class SparseDiagonalProduct;
template<typename MatrixType> class SparseView;

template<typename Lhs, typename Rhs>        class SparseSparseProduct;
template<typename Lhs, typename Rhs>        class SparseTimeDenseProduct;
template<typename Lhs, typename Rhs>        class DenseTimeSparseProduct;
template<typename Lhs, typename Rhs, bool Transpose> class SparseDenseOuterProduct;

template<typename Lhs, typename Rhs> struct SparseSparseProductReturnType;
template<typename Lhs, typename Rhs, int InnerSize = internal::traits<Lhs>::ColsAtCompileTime> struct DenseSparseProductReturnType;
template<typename Lhs, typename Rhs, int InnerSize = internal::traits<Lhs>::ColsAtCompileTime> struct SparseDenseProductReturnType;

namespace internal {

template<typename T> struct eval<T,Sparse>
{
    typedef typename traits<T>::Scalar _Scalar;
    enum {
          _Flags = traits<T>::Flags
    };

  public:
    typedef SparseMatrix<_Scalar, _Flags> type;
};

template<typename T> struct plain_matrix_type<T,Sparse>
{
  typedef typename traits<T>::Scalar _Scalar;
    enum {
          _Flags = traits<T>::Flags
    };

  public:
    typedef SparseMatrix<_Scalar, _Flags> type;
};

} // end namespace internal

#endif // EIGEN_SPARSEUTIL_H
