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

#ifndef EIGEN_SPARSE_DIAGONAL_PRODUCT_H
#define EIGEN_SPARSE_DIAGONAL_PRODUCT_H

// The product of a diagonal matrix with a sparse matrix can be easily
// implemented using expression template.
// We have two consider very different cases:
// 1 - diag * row-major sparse
//     => each inner vector <=> scalar * sparse vector product
//     => so we can reuse CwiseUnaryOp::InnerIterator
// 2 - diag * col-major sparse
//     => each inner vector <=> densevector * sparse vector cwise product
//     => again, we can reuse specialization of CwiseBinaryOp::InnerIterator
//        for that particular case
// The two other cases are symmetric.

namespace internal {

template<typename Lhs, typename Rhs>
struct traits<SparseDiagonalProduct<Lhs, Rhs> >
{
  typedef typename remove_all<Lhs>::type _Lhs;
  typedef typename remove_all<Rhs>::type _Rhs;
  typedef typename _Lhs::Scalar Scalar;
  typedef typename promote_index_type<typename traits<Lhs>::Index,
                                         typename traits<Rhs>::Index>::type Index;
  typedef Sparse StorageKind;
  typedef MatrixXpr XprKind;
  enum {
    RowsAtCompileTime = _Lhs::RowsAtCompileTime,
    ColsAtCompileTime = _Rhs::ColsAtCompileTime,

    MaxRowsAtCompileTime = _Lhs::MaxRowsAtCompileTime,
    MaxColsAtCompileTime = _Rhs::MaxColsAtCompileTime,

    SparseFlags = is_diagonal<_Lhs>::ret ? int(_Rhs::Flags) : int(_Lhs::Flags),
    Flags = (SparseFlags&RowMajorBit),
    CoeffReadCost = Dynamic
  };
};

enum {SDP_IsDiagonal, SDP_IsSparseRowMajor, SDP_IsSparseColMajor};
template<typename Lhs, typename Rhs, typename SparseDiagonalProductType, int RhsMode, int LhsMode>
class sparse_diagonal_product_inner_iterator_selector;

} // end namespace internal

template<typename Lhs, typename Rhs>
class SparseDiagonalProduct
  : public SparseMatrixBase<SparseDiagonalProduct<Lhs,Rhs> >,
    internal::no_assignment_operator
{
    typedef typename Lhs::Nested LhsNested;
    typedef typename Rhs::Nested RhsNested;

    typedef typename internal::remove_all<LhsNested>::type _LhsNested;
    typedef typename internal::remove_all<RhsNested>::type _RhsNested;

    enum {
      LhsMode = internal::is_diagonal<_LhsNested>::ret ? internal::SDP_IsDiagonal
              : (_LhsNested::Flags&RowMajorBit) ? internal::SDP_IsSparseRowMajor : internal::SDP_IsSparseColMajor,
      RhsMode = internal::is_diagonal<_RhsNested>::ret ? internal::SDP_IsDiagonal
              : (_RhsNested::Flags&RowMajorBit) ? internal::SDP_IsSparseRowMajor : internal::SDP_IsSparseColMajor
    };

  public:

    EIGEN_SPARSE_PUBLIC_INTERFACE(SparseDiagonalProduct)

    typedef internal::sparse_diagonal_product_inner_iterator_selector
                <_LhsNested,_RhsNested,SparseDiagonalProduct,LhsMode,RhsMode> InnerIterator;

    EIGEN_STRONG_INLINE SparseDiagonalProduct(const Lhs& lhs, const Rhs& rhs)
      : m_lhs(lhs), m_rhs(rhs)
    {
      eigen_assert(lhs.cols() == rhs.rows() && "invalid sparse matrix * diagonal matrix product");
    }

    EIGEN_STRONG_INLINE Index rows() const { return m_lhs.rows(); }
    EIGEN_STRONG_INLINE Index cols() const { return m_rhs.cols(); }

    EIGEN_STRONG_INLINE const _LhsNested& lhs() const { return m_lhs; }
    EIGEN_STRONG_INLINE const _RhsNested& rhs() const { return m_rhs; }

  protected:
    LhsNested m_lhs;
    RhsNested m_rhs;
};

namespace internal {

template<typename Lhs, typename Rhs, typename SparseDiagonalProductType>
class sparse_diagonal_product_inner_iterator_selector
<Lhs,Rhs,SparseDiagonalProductType,SDP_IsDiagonal,SDP_IsSparseRowMajor>
  : public CwiseUnaryOp<scalar_multiple_op<typename Lhs::Scalar>,const Rhs>::InnerIterator
{
    typedef typename CwiseUnaryOp<scalar_multiple_op<typename Lhs::Scalar>,const Rhs>::InnerIterator Base;
    typedef typename Lhs::Index Index;
  public:
    inline sparse_diagonal_product_inner_iterator_selector(
              const SparseDiagonalProductType& expr, Index outer)
      : Base(expr.rhs()*(expr.lhs().diagonal().coeff(outer)), outer)
    {}
};

template<typename Lhs, typename Rhs, typename SparseDiagonalProductType>
class sparse_diagonal_product_inner_iterator_selector
<Lhs,Rhs,SparseDiagonalProductType,SDP_IsDiagonal,SDP_IsSparseColMajor>
  : public CwiseBinaryOp<
      scalar_product_op<typename Lhs::Scalar>,
      SparseInnerVectorSet<Rhs,1>,
      typename Lhs::DiagonalVectorType>::InnerIterator
{
    typedef typename CwiseBinaryOp<
      scalar_product_op<typename Lhs::Scalar>,
      SparseInnerVectorSet<Rhs,1>,
      typename Lhs::DiagonalVectorType>::InnerIterator Base;
    typedef typename Lhs::Index Index;
  public:
    inline sparse_diagonal_product_inner_iterator_selector(
              const SparseDiagonalProductType& expr, Index outer)
      : Base(expr.rhs().innerVector(outer) .cwiseProduct(expr.lhs().diagonal()), 0)
    {}
};

template<typename Lhs, typename Rhs, typename SparseDiagonalProductType>
class sparse_diagonal_product_inner_iterator_selector
<Lhs,Rhs,SparseDiagonalProductType,SDP_IsSparseColMajor,SDP_IsDiagonal>
  : public CwiseUnaryOp<scalar_multiple_op<typename Rhs::Scalar>,const Lhs>::InnerIterator
{
    typedef typename CwiseUnaryOp<scalar_multiple_op<typename Rhs::Scalar>,const Lhs>::InnerIterator Base;
    typedef typename Lhs::Index Index;
  public:
    inline sparse_diagonal_product_inner_iterator_selector(
              const SparseDiagonalProductType& expr, Index outer)
      : Base(expr.lhs()*expr.rhs().diagonal().coeff(outer), outer)
    {}
};

template<typename Lhs, typename Rhs, typename SparseDiagonalProductType>
class sparse_diagonal_product_inner_iterator_selector
<Lhs,Rhs,SparseDiagonalProductType,SDP_IsSparseRowMajor,SDP_IsDiagonal>
  : public CwiseBinaryOp<
      scalar_product_op<typename Rhs::Scalar>,
      SparseInnerVectorSet<Lhs,1>,
      Transpose<const typename Rhs::DiagonalVectorType> >::InnerIterator
{
    typedef typename CwiseBinaryOp<
      scalar_product_op<typename Rhs::Scalar>,
      SparseInnerVectorSet<Lhs,1>,
      Transpose<const typename Rhs::DiagonalVectorType> >::InnerIterator Base;
    typedef typename Lhs::Index Index;
  public:
    inline sparse_diagonal_product_inner_iterator_selector(
              const SparseDiagonalProductType& expr, Index outer)
      : Base(expr.lhs().innerVector(outer) .cwiseProduct(expr.rhs().diagonal().transpose()), 0)
    {}
};

} // end namespace internal

// SparseMatrixBase functions

template<typename Derived>
template<typename OtherDerived>
const SparseDiagonalProduct<Derived,OtherDerived>
SparseMatrixBase<Derived>::operator*(const DiagonalBase<OtherDerived> &other) const
{
  return SparseDiagonalProduct<Derived,OtherDerived>(this->derived(), other.derived());
}

#endif // EIGEN_SPARSE_DIAGONAL_PRODUCT_H
