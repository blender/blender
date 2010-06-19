// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2009 Gael Guennebaud <g.gael@free.fr>
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

// the product a diagonal matrix with a sparse matrix can be easily
// implemented using expression template. We have two very different cases:
// 1 - diag * row-major sparse
//     => each inner vector <=> scalar * sparse vector product
//     => so we can reuse CwiseUnaryOp::InnerIterator
// 2 - diag * col-major sparse
//     => each inner vector <=> densevector * sparse vector cwise product
//     => again, we can reuse specialization of CwiseBinaryOp::InnerIterator
//        for that particular case
// The two other cases are symmetric.

template<typename Lhs, typename Rhs>
struct ei_traits<SparseDiagonalProduct<Lhs, Rhs> > : ei_traits<SparseProduct<Lhs, Rhs, DiagonalProduct> >
{
  typedef typename ei_cleantype<Lhs>::type _Lhs;
  typedef typename ei_cleantype<Rhs>::type _Rhs;
  enum {
    SparseFlags = ((int(_Lhs::Flags)&Diagonal)==Diagonal) ? int(_Rhs::Flags) : int(_Lhs::Flags),
    Flags = SparseBit | (SparseFlags&RowMajorBit)
  };
};

enum {SDP_IsDiagonal, SDP_IsSparseRowMajor, SDP_IsSparseColMajor};
template<typename Lhs, typename Rhs, typename SparseDiagonalProductType, int RhsMode, int LhsMode> 
class ei_sparse_diagonal_product_inner_iterator_selector;

template<typename LhsNested, typename RhsNested>
class SparseDiagonalProduct : public SparseMatrixBase<SparseDiagonalProduct<LhsNested,RhsNested> >, ei_no_assignment_operator
{
    typedef typename ei_traits<SparseDiagonalProduct>::_LhsNested _LhsNested;
    typedef typename ei_traits<SparseDiagonalProduct>::_RhsNested _RhsNested;
    
    enum {
      LhsMode = (_LhsNested::Flags&Diagonal)==Diagonal ? SDP_IsDiagonal
              : (_LhsNested::Flags&RowMajorBit) ? SDP_IsSparseRowMajor : SDP_IsSparseColMajor,
      RhsMode = (_RhsNested::Flags&Diagonal)==Diagonal ? SDP_IsDiagonal
              : (_RhsNested::Flags&RowMajorBit) ? SDP_IsSparseRowMajor : SDP_IsSparseColMajor
    };

  public:

    EIGEN_SPARSE_GENERIC_PUBLIC_INTERFACE(SparseDiagonalProduct)
    
    typedef ei_sparse_diagonal_product_inner_iterator_selector
                <_LhsNested,_RhsNested,SparseDiagonalProduct,LhsMode,RhsMode> InnerIterator;

    template<typename Lhs, typename Rhs>
    EIGEN_STRONG_INLINE SparseDiagonalProduct(const Lhs& lhs, const Rhs& rhs)
      : m_lhs(lhs), m_rhs(rhs)
    {
      ei_assert(lhs.cols() == rhs.rows() && "invalid sparse matrix * diagonal matrix product");
    }

    EIGEN_STRONG_INLINE int rows() const { return m_lhs.rows(); }
    EIGEN_STRONG_INLINE int cols() const { return m_rhs.cols(); }

    EIGEN_STRONG_INLINE const _LhsNested& lhs() const { return m_lhs; }
    EIGEN_STRONG_INLINE const _RhsNested& rhs() const { return m_rhs; }

  protected:
    LhsNested m_lhs;
    RhsNested m_rhs;
};


template<typename Lhs, typename Rhs, typename SparseDiagonalProductType>
class ei_sparse_diagonal_product_inner_iterator_selector
<Lhs,Rhs,SparseDiagonalProductType,SDP_IsDiagonal,SDP_IsSparseRowMajor>
  : public SparseCwiseUnaryOp<ei_scalar_multiple_op<typename Lhs::Scalar>,Rhs>::InnerIterator
{
    typedef typename SparseCwiseUnaryOp<ei_scalar_multiple_op<typename Lhs::Scalar>,Rhs>::InnerIterator Base;
  public:
    inline ei_sparse_diagonal_product_inner_iterator_selector(
              const SparseDiagonalProductType& expr, int outer)
      : Base(expr.rhs()*(expr.lhs().diagonal().coeff(outer)), outer)
    {}
};

template<typename Lhs, typename Rhs, typename SparseDiagonalProductType>
class ei_sparse_diagonal_product_inner_iterator_selector
<Lhs,Rhs,SparseDiagonalProductType,SDP_IsDiagonal,SDP_IsSparseColMajor>
  : public SparseCwiseBinaryOp<
      ei_scalar_product_op<typename Lhs::Scalar>,
      SparseInnerVectorSet<Rhs,1>,
      typename Lhs::_CoeffsVectorType>::InnerIterator
{
    typedef typename SparseCwiseBinaryOp<
      ei_scalar_product_op<typename Lhs::Scalar>,
      SparseInnerVectorSet<Rhs,1>,
      typename Lhs::_CoeffsVectorType>::InnerIterator Base;
  public:
    inline ei_sparse_diagonal_product_inner_iterator_selector(
              const SparseDiagonalProductType& expr, int outer)
      : Base(expr.rhs().innerVector(outer) .cwise()* expr.lhs().diagonal(), 0)
    {}
  private:
    ei_sparse_diagonal_product_inner_iterator_selector& operator=(const ei_sparse_diagonal_product_inner_iterator_selector&);
};

template<typename Lhs, typename Rhs, typename SparseDiagonalProductType>
class ei_sparse_diagonal_product_inner_iterator_selector
<Lhs,Rhs,SparseDiagonalProductType,SDP_IsSparseColMajor,SDP_IsDiagonal>
  : public SparseCwiseUnaryOp<ei_scalar_multiple_op<typename Rhs::Scalar>,Lhs>::InnerIterator
{
    typedef typename SparseCwiseUnaryOp<ei_scalar_multiple_op<typename Rhs::Scalar>,Lhs>::InnerIterator Base;
  public:
    inline ei_sparse_diagonal_product_inner_iterator_selector(
              const SparseDiagonalProductType& expr, int outer)
      : Base(expr.lhs()*expr.rhs().diagonal().coeff(outer), outer)
    {}
};

template<typename Lhs, typename Rhs, typename SparseDiagonalProductType>
class ei_sparse_diagonal_product_inner_iterator_selector
<Lhs,Rhs,SparseDiagonalProductType,SDP_IsSparseRowMajor,SDP_IsDiagonal>
  : public SparseCwiseBinaryOp<
      ei_scalar_product_op<typename Rhs::Scalar>,
      SparseInnerVectorSet<Lhs,1>,
      NestByValue<Transpose<typename Rhs::_CoeffsVectorType> > >::InnerIterator
{
    typedef typename SparseCwiseBinaryOp<
      ei_scalar_product_op<typename Rhs::Scalar>,
      SparseInnerVectorSet<Lhs,1>,
      NestByValue<Transpose<typename Rhs::_CoeffsVectorType> > >::InnerIterator Base;
  public:
    inline ei_sparse_diagonal_product_inner_iterator_selector(
              const SparseDiagonalProductType& expr, int outer)
      : Base(expr.lhs().innerVector(outer) .cwise()* expr.rhs().diagonal().transpose().nestByValue(), 0)
    {}
};

#endif // EIGEN_SPARSE_DIAGONAL_PRODUCT_H
