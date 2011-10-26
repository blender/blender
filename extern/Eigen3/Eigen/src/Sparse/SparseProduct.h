// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
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

#ifndef EIGEN_SPARSEPRODUCT_H
#define EIGEN_SPARSEPRODUCT_H

template<typename Lhs, typename Rhs>
struct SparseSparseProductReturnType
{
  typedef typename internal::traits<Lhs>::Scalar Scalar;
  enum {
    LhsRowMajor = internal::traits<Lhs>::Flags & RowMajorBit,
    RhsRowMajor = internal::traits<Rhs>::Flags & RowMajorBit,
    TransposeRhs = (!LhsRowMajor) && RhsRowMajor,
    TransposeLhs = LhsRowMajor && (!RhsRowMajor)
  };

  typedef typename internal::conditional<TransposeLhs,
    SparseMatrix<Scalar,0>,
    const typename internal::nested<Lhs,Rhs::RowsAtCompileTime>::type>::type LhsNested;

  typedef typename internal::conditional<TransposeRhs,
    SparseMatrix<Scalar,0>,
    const typename internal::nested<Rhs,Lhs::RowsAtCompileTime>::type>::type RhsNested;

  typedef SparseSparseProduct<LhsNested, RhsNested> Type;
};

namespace internal {
template<typename LhsNested, typename RhsNested>
struct traits<SparseSparseProduct<LhsNested, RhsNested> >
{
  typedef MatrixXpr XprKind;
  // clean the nested types:
  typedef typename remove_all<LhsNested>::type _LhsNested;
  typedef typename remove_all<RhsNested>::type _RhsNested;
  typedef typename _LhsNested::Scalar Scalar;
  typedef typename promote_index_type<typename traits<_LhsNested>::Index,
                                         typename traits<_RhsNested>::Index>::type Index;

  enum {
    LhsCoeffReadCost = _LhsNested::CoeffReadCost,
    RhsCoeffReadCost = _RhsNested::CoeffReadCost,
    LhsFlags = _LhsNested::Flags,
    RhsFlags = _RhsNested::Flags,

    RowsAtCompileTime    = _LhsNested::RowsAtCompileTime,
    ColsAtCompileTime    = _RhsNested::ColsAtCompileTime,
    MaxRowsAtCompileTime = _LhsNested::MaxRowsAtCompileTime,
    MaxColsAtCompileTime = _RhsNested::MaxColsAtCompileTime,

    InnerSize = EIGEN_SIZE_MIN_PREFER_FIXED(_LhsNested::ColsAtCompileTime, _RhsNested::RowsAtCompileTime),

    EvalToRowMajor = (RhsFlags & LhsFlags & RowMajorBit),

    RemovedBits = ~(EvalToRowMajor ? 0 : RowMajorBit),

    Flags = (int(LhsFlags | RhsFlags) & HereditaryBits & RemovedBits)
          | EvalBeforeAssigningBit
          | EvalBeforeNestingBit,

    CoeffReadCost = Dynamic
  };

  typedef Sparse StorageKind;
};

} // end namespace internal

template<typename LhsNested, typename RhsNested>
class SparseSparseProduct : internal::no_assignment_operator,
  public SparseMatrixBase<SparseSparseProduct<LhsNested, RhsNested> >
{
  public:

    typedef SparseMatrixBase<SparseSparseProduct> Base;
    EIGEN_DENSE_PUBLIC_INTERFACE(SparseSparseProduct)

  private:

    typedef typename internal::traits<SparseSparseProduct>::_LhsNested _LhsNested;
    typedef typename internal::traits<SparseSparseProduct>::_RhsNested _RhsNested;

  public:

    template<typename Lhs, typename Rhs>
    EIGEN_STRONG_INLINE SparseSparseProduct(const Lhs& lhs, const Rhs& rhs)
      : m_lhs(lhs), m_rhs(rhs)
    {
      eigen_assert(lhs.cols() == rhs.rows());

      enum {
        ProductIsValid = _LhsNested::ColsAtCompileTime==Dynamic
                      || _RhsNested::RowsAtCompileTime==Dynamic
                      || int(_LhsNested::ColsAtCompileTime)==int(_RhsNested::RowsAtCompileTime),
        AreVectors = _LhsNested::IsVectorAtCompileTime && _RhsNested::IsVectorAtCompileTime,
        SameSizes = EIGEN_PREDICATE_SAME_MATRIX_SIZE(_LhsNested,_RhsNested)
      };
      // note to the lost user:
      //    * for a dot product use: v1.dot(v2)
      //    * for a coeff-wise product use: v1.cwise()*v2
      EIGEN_STATIC_ASSERT(ProductIsValid || !(AreVectors && SameSizes),
        INVALID_VECTOR_VECTOR_PRODUCT__IF_YOU_WANTED_A_DOT_OR_COEFF_WISE_PRODUCT_YOU_MUST_USE_THE_EXPLICIT_FUNCTIONS)
      EIGEN_STATIC_ASSERT(ProductIsValid || !(SameSizes && !AreVectors),
        INVALID_MATRIX_PRODUCT__IF_YOU_WANTED_A_COEFF_WISE_PRODUCT_YOU_MUST_USE_THE_EXPLICIT_FUNCTION)
      EIGEN_STATIC_ASSERT(ProductIsValid || SameSizes, INVALID_MATRIX_PRODUCT)
    }

    EIGEN_STRONG_INLINE Index rows() const { return m_lhs.rows(); }
    EIGEN_STRONG_INLINE Index cols() const { return m_rhs.cols(); }

    EIGEN_STRONG_INLINE const _LhsNested& lhs() const { return m_lhs; }
    EIGEN_STRONG_INLINE const _RhsNested& rhs() const { return m_rhs; }

  protected:
    LhsNested m_lhs;
    RhsNested m_rhs;
};

#endif // EIGEN_SPARSEPRODUCT_H
