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

#ifndef EIGEN_SPARSE_CWISE_BINARY_OP_H
#define EIGEN_SPARSE_CWISE_BINARY_OP_H

// Here we have to handle 3 cases:
//  1 - sparse op dense
//  2 - dense op sparse
//  3 - sparse op sparse
// We also need to implement a 4th iterator for:
//  4 - dense op dense
// Finally, we also need to distinguish between the product and other operations :
//                configuration      returned mode
//  1 - sparse op dense    product      sparse
//                         generic      dense
//  2 - dense op sparse    product      sparse
//                         generic      dense
//  3 - sparse op sparse   product      sparse
//                         generic      sparse
//  4 - dense op dense     product      dense
//                         generic      dense

template<typename BinaryOp, typename Lhs, typename Rhs>
struct ei_traits<SparseCwiseBinaryOp<BinaryOp, Lhs, Rhs> >
{
  typedef typename ei_result_of<
                     BinaryOp(
                       typename Lhs::Scalar,
                       typename Rhs::Scalar
                     )
                   >::type Scalar;
  typedef typename Lhs::Nested LhsNested;
  typedef typename Rhs::Nested RhsNested;
  typedef typename ei_unref<LhsNested>::type _LhsNested;
  typedef typename ei_unref<RhsNested>::type _RhsNested;
  enum {
    LhsCoeffReadCost = _LhsNested::CoeffReadCost,
    RhsCoeffReadCost = _RhsNested::CoeffReadCost,
    LhsFlags = _LhsNested::Flags,
    RhsFlags = _RhsNested::Flags,
    RowsAtCompileTime = Lhs::RowsAtCompileTime,
    ColsAtCompileTime = Lhs::ColsAtCompileTime,
    MaxRowsAtCompileTime = Lhs::MaxRowsAtCompileTime,
    MaxColsAtCompileTime = Lhs::MaxColsAtCompileTime,
    Flags = (int(LhsFlags) | int(RhsFlags)) & HereditaryBits,
    CoeffReadCost = LhsCoeffReadCost + RhsCoeffReadCost + ei_functor_traits<BinaryOp>::Cost
  };
};

template<typename BinaryOp, typename Lhs, typename Rhs>
class SparseCwiseBinaryOp : ei_no_assignment_operator,
  public SparseMatrixBase<SparseCwiseBinaryOp<BinaryOp, Lhs, Rhs> >
{
  public:

    class InnerIterator;

    EIGEN_SPARSE_GENERIC_PUBLIC_INTERFACE(SparseCwiseBinaryOp)
    typedef typename ei_traits<SparseCwiseBinaryOp>::LhsNested LhsNested;
    typedef typename ei_traits<SparseCwiseBinaryOp>::RhsNested RhsNested;
    typedef typename ei_unref<LhsNested>::type _LhsNested;
    typedef typename ei_unref<RhsNested>::type _RhsNested;

    EIGEN_STRONG_INLINE SparseCwiseBinaryOp(const Lhs& lhs, const Rhs& rhs, const BinaryOp& func = BinaryOp())
      : m_lhs(lhs), m_rhs(rhs), m_functor(func)
    {
      EIGEN_STATIC_ASSERT((ei_functor_allows_mixing_real_and_complex<BinaryOp>::ret
                           ? int(ei_is_same_type<typename Lhs::RealScalar, typename Rhs::RealScalar>::ret)
                           : int(ei_is_same_type<typename Lhs::Scalar, typename Rhs::Scalar>::ret)),
        YOU_MIXED_DIFFERENT_NUMERIC_TYPES__YOU_NEED_TO_USE_THE_CAST_METHOD_OF_MATRIXBASE_TO_CAST_NUMERIC_TYPES_EXPLICITLY)
      // require the sizes to match
      EIGEN_STATIC_ASSERT_SAME_MATRIX_SIZE(Lhs, Rhs)
      ei_assert(lhs.rows() == rhs.rows() && lhs.cols() == rhs.cols());
    }

    EIGEN_STRONG_INLINE int rows() const { return m_lhs.rows(); }
    EIGEN_STRONG_INLINE int cols() const { return m_lhs.cols(); }

    EIGEN_STRONG_INLINE const _LhsNested& lhs() const { return m_lhs; }
    EIGEN_STRONG_INLINE const _RhsNested& rhs() const { return m_rhs; }
    EIGEN_STRONG_INLINE const BinaryOp& functor() const { return m_functor; }

  protected:
    const LhsNested m_lhs;
    const RhsNested m_rhs;
    const BinaryOp m_functor;
};

template<typename BinaryOp, typename Lhs, typename Rhs, typename Derived,
  int _LhsStorageMode = int(Lhs::Flags) & SparseBit,
  int _RhsStorageMode = int(Rhs::Flags) & SparseBit>
class ei_sparse_cwise_binary_op_inner_iterator_selector;

template<typename BinaryOp, typename Lhs, typename Rhs>
class SparseCwiseBinaryOp<BinaryOp,Lhs,Rhs>::InnerIterator
  : public ei_sparse_cwise_binary_op_inner_iterator_selector<BinaryOp,Lhs,Rhs, typename SparseCwiseBinaryOp<BinaryOp,Lhs,Rhs>::InnerIterator>
{
  public:
    typedef ei_sparse_cwise_binary_op_inner_iterator_selector<
      BinaryOp,Lhs,Rhs, InnerIterator> Base;

    EIGEN_STRONG_INLINE InnerIterator(const SparseCwiseBinaryOp& binOp, int outer)
      : Base(binOp,outer)
    {}
};

/***************************************************************************
* Implementation of inner-iterators
***************************************************************************/

// template<typename T> struct ei_is_scalar_product { enum { ret = false }; };
// template<typename T> struct ei_is_scalar_product<ei_scalar_product_op<T> > { enum { ret = true }; };

// helper class


// sparse - sparse  (generic)
template<typename BinaryOp, typename Lhs, typename Rhs, typename Derived>
class ei_sparse_cwise_binary_op_inner_iterator_selector<BinaryOp, Lhs, Rhs, Derived, IsSparse, IsSparse>
{
    typedef SparseCwiseBinaryOp<BinaryOp, Lhs, Rhs> CwiseBinaryXpr;
    typedef typename ei_traits<CwiseBinaryXpr>::Scalar Scalar;
    typedef typename ei_traits<CwiseBinaryXpr>::_LhsNested _LhsNested;
    typedef typename ei_traits<CwiseBinaryXpr>::_RhsNested _RhsNested;
    typedef typename _LhsNested::InnerIterator LhsIterator;
    typedef typename _RhsNested::InnerIterator RhsIterator;
  public:

    EIGEN_STRONG_INLINE ei_sparse_cwise_binary_op_inner_iterator_selector(const CwiseBinaryXpr& xpr, int outer)
      : m_lhsIter(xpr.lhs(),outer), m_rhsIter(xpr.rhs(),outer), m_functor(xpr.functor())
    {
      this->operator++();
    }

    EIGEN_STRONG_INLINE Derived& operator++()
    {
      if (m_lhsIter && m_rhsIter && (m_lhsIter.index() == m_rhsIter.index()))
      {
        m_id = m_lhsIter.index();
        m_value = m_functor(m_lhsIter.value(), m_rhsIter.value());
        ++m_lhsIter;
        ++m_rhsIter;
      }
      else if (m_lhsIter && (!m_rhsIter || (m_lhsIter.index() < m_rhsIter.index())))
      {
        m_id = m_lhsIter.index();
        m_value = m_functor(m_lhsIter.value(), Scalar(0));
        ++m_lhsIter;
      }
      else if (m_rhsIter && (!m_lhsIter || (m_lhsIter.index() > m_rhsIter.index())))
      {
        m_id = m_rhsIter.index();
        m_value = m_functor(Scalar(0), m_rhsIter.value());
        ++m_rhsIter;
      }
      else
      {
        m_id = -1;
      }
      return *static_cast<Derived*>(this);
    }

    EIGEN_STRONG_INLINE Scalar value() const { return m_value; }

    EIGEN_STRONG_INLINE int index() const { return m_id; }
    EIGEN_STRONG_INLINE int row() const { return m_lhsIter.row(); }
    EIGEN_STRONG_INLINE int col() const { return m_lhsIter.col(); }

    EIGEN_STRONG_INLINE operator bool() const { return m_id>=0; }

  protected:
    LhsIterator m_lhsIter;
    RhsIterator m_rhsIter;
    const BinaryOp& m_functor;
    Scalar m_value;
    int m_id;
};

// sparse - sparse  (product)
template<typename T, typename Lhs, typename Rhs, typename Derived>
class ei_sparse_cwise_binary_op_inner_iterator_selector<ei_scalar_product_op<T>, Lhs, Rhs, Derived, IsSparse, IsSparse>
{
    typedef ei_scalar_product_op<T> BinaryFunc;
    typedef SparseCwiseBinaryOp<BinaryFunc, Lhs, Rhs> CwiseBinaryXpr;
    typedef typename CwiseBinaryXpr::Scalar Scalar;
    typedef typename ei_traits<CwiseBinaryXpr>::_LhsNested _LhsNested;
    typedef typename _LhsNested::InnerIterator LhsIterator;
    typedef typename ei_traits<CwiseBinaryXpr>::_RhsNested _RhsNested;
    typedef typename _RhsNested::InnerIterator RhsIterator;
  public:

    EIGEN_STRONG_INLINE ei_sparse_cwise_binary_op_inner_iterator_selector(const CwiseBinaryXpr& xpr, int outer)
      : m_lhsIter(xpr.lhs(),outer), m_rhsIter(xpr.rhs(),outer), m_functor(xpr.functor())
    {
      while (m_lhsIter && m_rhsIter && (m_lhsIter.index() != m_rhsIter.index()))
      {
        if (m_lhsIter.index() < m_rhsIter.index())
          ++m_lhsIter;
        else
          ++m_rhsIter;
      }
    }

    EIGEN_STRONG_INLINE Derived& operator++()
    {
      ++m_lhsIter;
      ++m_rhsIter;
      while (m_lhsIter && m_rhsIter && (m_lhsIter.index() != m_rhsIter.index()))
      {
        if (m_lhsIter.index() < m_rhsIter.index())
          ++m_lhsIter;
        else
          ++m_rhsIter;
      }
      return *static_cast<Derived*>(this);
    }

    EIGEN_STRONG_INLINE Scalar value() const { return m_functor(m_lhsIter.value(), m_rhsIter.value()); }

    EIGEN_STRONG_INLINE int index() const { return m_lhsIter.index(); }
    EIGEN_STRONG_INLINE int row() const { return m_lhsIter.row(); }
    EIGEN_STRONG_INLINE int col() const { return m_lhsIter.col(); }

    EIGEN_STRONG_INLINE operator bool() const { return (m_lhsIter && m_rhsIter); }

  protected:
    LhsIterator m_lhsIter;
    RhsIterator m_rhsIter;
    const BinaryFunc& m_functor;
};

// sparse - dense  (product)
template<typename T, typename Lhs, typename Rhs, typename Derived>
class ei_sparse_cwise_binary_op_inner_iterator_selector<ei_scalar_product_op<T>, Lhs, Rhs, Derived, IsSparse, IsDense>
{
    typedef ei_scalar_product_op<T> BinaryFunc;
    typedef SparseCwiseBinaryOp<BinaryFunc, Lhs, Rhs> CwiseBinaryXpr;
    typedef typename CwiseBinaryXpr::Scalar Scalar;
    typedef typename ei_traits<CwiseBinaryXpr>::_LhsNested _LhsNested;
    typedef typename _LhsNested::InnerIterator LhsIterator;
    enum { IsRowMajor = (int(Lhs::Flags)&RowMajorBit)==RowMajorBit };
  public:

    EIGEN_STRONG_INLINE ei_sparse_cwise_binary_op_inner_iterator_selector(const CwiseBinaryXpr& xpr, int outer)
      : m_xpr(xpr), m_lhsIter(xpr.lhs(),outer), m_functor(xpr.functor()), m_outer(outer)
    {}

    EIGEN_STRONG_INLINE Derived& operator++()
    {
      ++m_lhsIter;
      return *static_cast<Derived*>(this);
    }

    EIGEN_STRONG_INLINE Scalar value() const
    { return m_functor(m_lhsIter.value(),
                       m_xpr.rhs().coeff(IsRowMajor?m_outer:m_lhsIter.index(),IsRowMajor?m_lhsIter.index():m_outer)); }

    EIGEN_STRONG_INLINE int index() const { return m_lhsIter.index(); }
    EIGEN_STRONG_INLINE int row() const { return m_lhsIter.row(); }
    EIGEN_STRONG_INLINE int col() const { return m_lhsIter.col(); }

    EIGEN_STRONG_INLINE operator bool() const { return m_lhsIter; }

  protected:
    const CwiseBinaryXpr& m_xpr;
    LhsIterator m_lhsIter;
    const BinaryFunc& m_functor;
    const int m_outer;
};

// sparse - dense  (product)
template<typename T, typename Lhs, typename Rhs, typename Derived>
class ei_sparse_cwise_binary_op_inner_iterator_selector<ei_scalar_product_op<T>, Lhs, Rhs, Derived, IsDense, IsSparse>
{
    typedef ei_scalar_product_op<T> BinaryFunc;
    typedef SparseCwiseBinaryOp<BinaryFunc, Lhs, Rhs> CwiseBinaryXpr;
    typedef typename CwiseBinaryXpr::Scalar Scalar;
    typedef typename ei_traits<CwiseBinaryXpr>::_RhsNested _RhsNested;
    typedef typename _RhsNested::InnerIterator RhsIterator;
    enum { IsRowMajor = (int(Rhs::Flags)&RowMajorBit)==RowMajorBit };
  public:

    EIGEN_STRONG_INLINE ei_sparse_cwise_binary_op_inner_iterator_selector(const CwiseBinaryXpr& xpr, int outer)
      : m_xpr(xpr), m_rhsIter(xpr.rhs(),outer), m_functor(xpr.functor()), m_outer(outer)
    {}

    EIGEN_STRONG_INLINE Derived& operator++()
    {
      ++m_rhsIter;
      return *static_cast<Derived*>(this);
    }

    EIGEN_STRONG_INLINE Scalar value() const
    { return m_functor(m_xpr.lhs().coeff(IsRowMajor?m_outer:m_rhsIter.index(),IsRowMajor?m_rhsIter.index():m_outer), m_rhsIter.value()); }

    EIGEN_STRONG_INLINE int index() const { return m_rhsIter.index(); }
    EIGEN_STRONG_INLINE int row() const { return m_rhsIter.row(); }
    EIGEN_STRONG_INLINE int col() const { return m_rhsIter.col(); }

    EIGEN_STRONG_INLINE operator bool() const { return m_rhsIter; }

  protected:
    const CwiseBinaryXpr& m_xpr;
    RhsIterator m_rhsIter;
    const BinaryFunc& m_functor;
    const int m_outer;
};


template<typename Derived>
template<typename OtherDerived>
EIGEN_STRONG_INLINE const SparseCwiseBinaryOp<ei_scalar_difference_op<typename ei_traits<Derived>::Scalar>,
                                 Derived, OtherDerived>
SparseMatrixBase<Derived>::operator-(const SparseMatrixBase<OtherDerived> &other) const
{
  return SparseCwiseBinaryOp<ei_scalar_difference_op<Scalar>,
                       Derived, OtherDerived>(derived(), other.derived());
}

template<typename Derived>
template<typename OtherDerived>
EIGEN_STRONG_INLINE Derived &
SparseMatrixBase<Derived>::operator-=(const SparseMatrixBase<OtherDerived> &other)
{
  return *this = derived() - other.derived();
}

template<typename Derived>
template<typename OtherDerived>
EIGEN_STRONG_INLINE const SparseCwiseBinaryOp<ei_scalar_sum_op<typename ei_traits<Derived>::Scalar>, Derived, OtherDerived>
SparseMatrixBase<Derived>::operator+(const SparseMatrixBase<OtherDerived> &other) const
{
  return SparseCwiseBinaryOp<ei_scalar_sum_op<Scalar>, Derived, OtherDerived>(derived(), other.derived());
}

template<typename Derived>
template<typename OtherDerived>
EIGEN_STRONG_INLINE Derived &
SparseMatrixBase<Derived>::operator+=(const SparseMatrixBase<OtherDerived>& other)
{
  return *this = derived() + other.derived();
}

template<typename ExpressionType>
template<typename OtherDerived>
EIGEN_STRONG_INLINE const EIGEN_SPARSE_CWISE_PRODUCT_RETURN_TYPE
SparseCwise<ExpressionType>::operator*(const SparseMatrixBase<OtherDerived> &other) const
{
  return EIGEN_SPARSE_CWISE_PRODUCT_RETURN_TYPE(_expression(), other.derived());
}

template<typename ExpressionType>
template<typename OtherDerived>
EIGEN_STRONG_INLINE const EIGEN_SPARSE_CWISE_PRODUCT_RETURN_TYPE
SparseCwise<ExpressionType>::operator*(const MatrixBase<OtherDerived> &other) const
{
  return EIGEN_SPARSE_CWISE_PRODUCT_RETURN_TYPE(_expression(), other.derived());
}

// template<typename ExpressionType>
// template<typename OtherDerived>
// EIGEN_STRONG_INLINE const EIGEN_SPARSE_CWISE_BINOP_RETURN_TYPE(ei_scalar_quotient_op)
// SparseCwise<ExpressionType>::operator/(const SparseMatrixBase<OtherDerived> &other) const
// {
//   return EIGEN_SPARSE_CWISE_BINOP_RETURN_TYPE(ei_scalar_quotient_op)(_expression(), other.derived());
// }
//
// template<typename ExpressionType>
// template<typename OtherDerived>
// EIGEN_STRONG_INLINE const EIGEN_SPARSE_CWISE_BINOP_RETURN_TYPE(ei_scalar_quotient_op)
// SparseCwise<ExpressionType>::operator/(const MatrixBase<OtherDerived> &other) const
// {
//   return EIGEN_SPARSE_CWISE_BINOP_RETURN_TYPE(ei_scalar_quotient_op)(_expression(), other.derived());
// }

template<typename ExpressionType>
template<typename OtherDerived>
inline ExpressionType& SparseCwise<ExpressionType>::operator*=(const SparseMatrixBase<OtherDerived> &other)
{
  return m_matrix.const_cast_derived() = _expression() * other.derived();
}

// template<typename ExpressionType>
// template<typename OtherDerived>
// inline ExpressionType& SparseCwise<ExpressionType>::operator/=(const SparseMatrixBase<OtherDerived> &other)
// {
//   return m_matrix.const_cast_derived() = *this / other;
// }

template<typename ExpressionType>
template<typename OtherDerived>
EIGEN_STRONG_INLINE const EIGEN_SPARSE_CWISE_BINOP_RETURN_TYPE(ei_scalar_min_op)
SparseCwise<ExpressionType>::min(const SparseMatrixBase<OtherDerived> &other) const
{
  return EIGEN_SPARSE_CWISE_BINOP_RETURN_TYPE(ei_scalar_min_op)(_expression(), other.derived());
}

template<typename ExpressionType>
template<typename OtherDerived>
EIGEN_STRONG_INLINE const EIGEN_SPARSE_CWISE_BINOP_RETURN_TYPE(ei_scalar_max_op)
SparseCwise<ExpressionType>::max(const SparseMatrixBase<OtherDerived> &other) const
{
  return EIGEN_SPARSE_CWISE_BINOP_RETURN_TYPE(ei_scalar_max_op)(_expression(), other.derived());
}

// template<typename Derived>
// template<typename CustomBinaryOp, typename OtherDerived>
// EIGEN_STRONG_INLINE const CwiseBinaryOp<CustomBinaryOp, Derived, OtherDerived>
// SparseMatrixBase<Derived>::binaryExpr(const SparseMatrixBase<OtherDerived> &other, const CustomBinaryOp& func) const
// {
//   return CwiseBinaryOp<CustomBinaryOp, Derived, OtherDerived>(derived(), other.derived(), func);
// }

#endif // EIGEN_SPARSE_CWISE_BINARY_OP_H
