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

#ifndef EIGEN_SPARSE_CWISE_UNARY_OP_H
#define EIGEN_SPARSE_CWISE_UNARY_OP_H

// template<typename UnaryOp, typename MatrixType>
// struct internal::traits<SparseCwiseUnaryOp<UnaryOp, MatrixType> > : internal::traits<MatrixType>
// {
//   typedef typename internal::result_of<
//                      UnaryOp(typename MatrixType::Scalar)
//                    >::type Scalar;
//   typedef typename MatrixType::Nested MatrixTypeNested;
//   typedef typename internal::remove_reference<MatrixTypeNested>::type _MatrixTypeNested;
//   enum {
//     CoeffReadCost = _MatrixTypeNested::CoeffReadCost + internal::functor_traits<UnaryOp>::Cost
//   };
// };

template<typename UnaryOp, typename MatrixType>
class CwiseUnaryOpImpl<UnaryOp,MatrixType,Sparse>
  : public SparseMatrixBase<CwiseUnaryOp<UnaryOp, MatrixType> >
{
  public:

    class InnerIterator;
//     typedef typename internal::remove_reference<LhsNested>::type _LhsNested;

    typedef CwiseUnaryOp<UnaryOp, MatrixType> Derived;
    EIGEN_SPARSE_PUBLIC_INTERFACE(Derived)
};

template<typename UnaryOp, typename MatrixType>
class CwiseUnaryOpImpl<UnaryOp,MatrixType,Sparse>::InnerIterator
{
    typedef typename CwiseUnaryOpImpl::Scalar Scalar;
    typedef typename internal::traits<Derived>::_XprTypeNested _MatrixTypeNested;
    typedef typename _MatrixTypeNested::InnerIterator MatrixTypeIterator;
    typedef typename MatrixType::Index Index;
  public:

    EIGEN_STRONG_INLINE InnerIterator(const CwiseUnaryOpImpl& unaryOp, Index outer)
      : m_iter(unaryOp.derived().nestedExpression(),outer), m_functor(unaryOp.derived().functor())
    {}

    EIGEN_STRONG_INLINE InnerIterator& operator++()
    { ++m_iter; return *this; }

    EIGEN_STRONG_INLINE Scalar value() const { return m_functor(m_iter.value()); }

    EIGEN_STRONG_INLINE Index index() const { return m_iter.index(); }
    EIGEN_STRONG_INLINE Index row() const { return m_iter.row(); }
    EIGEN_STRONG_INLINE Index col() const { return m_iter.col(); }

    EIGEN_STRONG_INLINE operator bool() const { return m_iter; }

  protected:
    MatrixTypeIterator m_iter;
    const UnaryOp m_functor;
};

template<typename ViewOp, typename MatrixType>
class CwiseUnaryViewImpl<ViewOp,MatrixType,Sparse>
  : public SparseMatrixBase<CwiseUnaryView<ViewOp, MatrixType> >
{
  public:

    class InnerIterator;
//     typedef typename internal::remove_reference<LhsNested>::type _LhsNested;

    typedef CwiseUnaryView<ViewOp, MatrixType> Derived;
    EIGEN_SPARSE_PUBLIC_INTERFACE(Derived)
};

template<typename ViewOp, typename MatrixType>
class CwiseUnaryViewImpl<ViewOp,MatrixType,Sparse>::InnerIterator
{
    typedef typename CwiseUnaryViewImpl::Scalar Scalar;
    typedef typename internal::traits<Derived>::_MatrixTypeNested _MatrixTypeNested;
    typedef typename _MatrixTypeNested::InnerIterator MatrixTypeIterator;
    typedef typename MatrixType::Index Index;
  public:

    EIGEN_STRONG_INLINE InnerIterator(const CwiseUnaryViewImpl& unaryView, Index outer)
      : m_iter(unaryView.derived().nestedExpression(),outer), m_functor(unaryView.derived().functor())
    {}

    EIGEN_STRONG_INLINE InnerIterator& operator++()
    { ++m_iter; return *this; }

    EIGEN_STRONG_INLINE Scalar value() const { return m_functor(m_iter.value()); }
    EIGEN_STRONG_INLINE Scalar& valueRef() { return m_functor(m_iter.valueRef()); }

    EIGEN_STRONG_INLINE Index index() const { return m_iter.index(); }
    EIGEN_STRONG_INLINE Index row() const { return m_iter.row(); }
    EIGEN_STRONG_INLINE Index col() const { return m_iter.col(); }

    EIGEN_STRONG_INLINE operator bool() const { return m_iter; }

  protected:
    MatrixTypeIterator m_iter;
    const ViewOp m_functor;
};

template<typename Derived>
EIGEN_STRONG_INLINE Derived&
SparseMatrixBase<Derived>::operator*=(const Scalar& other)
{
  for (Index j=0; j<outerSize(); ++j)
    for (typename Derived::InnerIterator i(derived(),j); i; ++i)
      i.valueRef() *= other;
  return derived();
}

template<typename Derived>
EIGEN_STRONG_INLINE Derived&
SparseMatrixBase<Derived>::operator/=(const Scalar& other)
{
  for (Index j=0; j<outerSize(); ++j)
    for (typename Derived::InnerIterator i(derived(),j); i; ++i)
      i.valueRef() /= other;
  return derived();
}

#endif // EIGEN_SPARSE_CWISE_UNARY_OP_H
