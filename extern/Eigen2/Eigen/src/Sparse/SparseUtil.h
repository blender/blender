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

#define _EIGEN_SPARSE_GENERIC_PUBLIC_INTERFACE(Derived, BaseClass) \
typedef BaseClass Base; \
typedef typename Eigen::ei_traits<Derived>::Scalar Scalar; \
typedef typename Eigen::NumTraits<Scalar>::Real RealScalar; \
typedef typename Eigen::ei_nested<Derived>::type Nested; \
enum { RowsAtCompileTime = Eigen::ei_traits<Derived>::RowsAtCompileTime, \
       ColsAtCompileTime = Eigen::ei_traits<Derived>::ColsAtCompileTime, \
       Flags = Eigen::ei_traits<Derived>::Flags, \
       CoeffReadCost = Eigen::ei_traits<Derived>::CoeffReadCost, \
       SizeAtCompileTime = Base::SizeAtCompileTime, \
       IsVectorAtCompileTime = Base::IsVectorAtCompileTime };

#define EIGEN_SPARSE_GENERIC_PUBLIC_INTERFACE(Derived) \
_EIGEN_SPARSE_GENERIC_PUBLIC_INTERFACE(Derived, Eigen::SparseMatrixBase<Derived>)

enum SparseBackend {
  DefaultBackend,
  Taucs,
  Cholmod,
  SuperLU,
  UmfPack
};

// solver flags
enum {
  CompleteFactorization       = 0x0000,  // the default
  IncompleteFactorization     = 0x0001,
  MemoryEfficient             = 0x0002,

  // For LLT Cholesky:
  SupernodalMultifrontal      = 0x0010,
  SupernodalLeftLooking       = 0x0020,

  // Ordering methods:
  NaturalOrdering             = 0x0100, // the default
  MinimumDegree_AT_PLUS_A     = 0x0200,
  MinimumDegree_ATA           = 0x0300,
  ColApproxMinimumDegree      = 0x0400,
  Metis                       = 0x0500,
  Scotch                      = 0x0600,
  Chaco                       = 0x0700,
  OrderingMask                = 0x0f00
};

template<typename Derived> class SparseMatrixBase;
template<typename _Scalar, int _Flags = 0> class SparseMatrix;
template<typename _Scalar, int _Flags = 0> class DynamicSparseMatrix;
template<typename _Scalar, int _Flags = 0> class SparseVector;
template<typename _Scalar, int _Flags = 0> class MappedSparseMatrix;

template<typename MatrixType>                            class SparseTranspose;
template<typename MatrixType, int Size>                  class SparseInnerVectorSet;
template<typename Derived>                               class SparseCwise;
template<typename UnaryOp,   typename MatrixType>        class SparseCwiseUnaryOp;
template<typename BinaryOp,  typename Lhs, typename Rhs> class SparseCwiseBinaryOp;
template<typename ExpressionType,
         unsigned int Added, unsigned int Removed>       class SparseFlagged;
template<typename Lhs, typename Rhs>                     class SparseDiagonalProduct;

template<typename Lhs, typename Rhs> struct ei_sparse_product_mode;
template<typename Lhs, typename Rhs, int ProductMode = ei_sparse_product_mode<Lhs,Rhs>::value> struct SparseProductReturnType;

const int CoherentAccessPattern  = 0x1;
const int InnerRandomAccessPattern  = 0x2 | CoherentAccessPattern;
const int OuterRandomAccessPattern  = 0x4 | CoherentAccessPattern;
const int RandomAccessPattern       = 0x8 | OuterRandomAccessPattern | InnerRandomAccessPattern;

// const int AccessPatternNotSupported = 0x0;
// const int AccessPatternSupported    = 0x1;
// 
// template<typename MatrixType, int AccessPattern> struct ei_support_access_pattern
// {
//   enum { ret = (int(ei_traits<MatrixType>::SupportedAccessPatterns) & AccessPattern) == AccessPattern
//              ? AccessPatternSupported
//              : AccessPatternNotSupported
//   };
// };

template<typename T> class ei_eval<T,IsSparse>
{
    typedef typename ei_traits<T>::Scalar _Scalar;
    enum {
          _Flags = ei_traits<T>::Flags
    };

  public:
    typedef SparseMatrix<_Scalar, _Flags> type;
};

#endif // EIGEN_SPARSEUTIL_H
