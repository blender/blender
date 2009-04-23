// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
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

#ifndef EIGEN_PARTIAL_REDUX_H
#define EIGEN_PARTIAL_REDUX_H

/** \array_module \ingroup Array
  *
  * \class PartialReduxExpr
  *
  * \brief Generic expression of a partially reduxed matrix
  *
  * \param MatrixType the type of the matrix we are applying the redux operation
  * \param MemberOp type of the member functor
  * \param Direction indicates the direction of the redux (Vertical or Horizontal)
  *
  * This class represents an expression of a partial redux operator of a matrix.
  * It is the return type of PartialRedux functions,
  * and most of the time this is the only way it is used.
  *
  * \sa class PartialRedux
  */

template< typename MatrixType, typename MemberOp, int Direction>
class PartialReduxExpr;

template<typename MatrixType, typename MemberOp, int Direction>
struct ei_traits<PartialReduxExpr<MatrixType, MemberOp, Direction> >
{
  typedef typename MemberOp::result_type Scalar;
  typedef typename MatrixType::Scalar InputScalar;
  typedef typename ei_nested<MatrixType>::type MatrixTypeNested;
  typedef typename ei_cleantype<MatrixTypeNested>::type _MatrixTypeNested;
  enum {
    RowsAtCompileTime = Direction==Vertical   ? 1 : MatrixType::RowsAtCompileTime,
    ColsAtCompileTime = Direction==Horizontal ? 1 : MatrixType::ColsAtCompileTime,
    MaxRowsAtCompileTime = Direction==Vertical   ? 1 : MatrixType::MaxRowsAtCompileTime,
    MaxColsAtCompileTime = Direction==Horizontal ? 1 : MatrixType::MaxColsAtCompileTime,
    Flags = (unsigned int)_MatrixTypeNested::Flags & HereditaryBits,
    TraversalSize = Direction==Vertical ? RowsAtCompileTime : ColsAtCompileTime
  };
  typedef typename MemberOp::template Cost<InputScalar,int(TraversalSize)> CostOpType;
  enum {
    CoeffReadCost = TraversalSize * ei_traits<_MatrixTypeNested>::CoeffReadCost + int(CostOpType::value)
  };
};

template< typename MatrixType, typename MemberOp, int Direction>
class PartialReduxExpr : ei_no_assignment_operator,
  public MatrixBase<PartialReduxExpr<MatrixType, MemberOp, Direction> >
{
  public:

    EIGEN_GENERIC_PUBLIC_INTERFACE(PartialReduxExpr)
    typedef typename ei_traits<PartialReduxExpr>::MatrixTypeNested MatrixTypeNested;
    typedef typename ei_traits<PartialReduxExpr>::_MatrixTypeNested _MatrixTypeNested;

    PartialReduxExpr(const MatrixType& mat, const MemberOp& func = MemberOp())
      : m_matrix(mat), m_functor(func) {}

    int rows() const { return (Direction==Vertical   ? 1 : m_matrix.rows()); }
    int cols() const { return (Direction==Horizontal ? 1 : m_matrix.cols()); }

    const Scalar coeff(int i, int j) const
    {
      if (Direction==Vertical)
        return m_functor(m_matrix.col(j));
      else
        return m_functor(m_matrix.row(i));
    }

  protected:
    const MatrixTypeNested m_matrix;
    const MemberOp m_functor;
};

#define EIGEN_MEMBER_FUNCTOR(MEMBER,COST)                           \
  template <typename ResultType>                                    \
  struct ei_member_##MEMBER EIGEN_EMPTY_STRUCT {                    \
    typedef ResultType result_type;                                 \
    template<typename Scalar, int Size> struct Cost                 \
    { enum { value = COST }; };                                     \
    template<typename Derived>                                      \
    inline ResultType operator()(const MatrixBase<Derived>& mat) const     \
    { return mat.MEMBER(); }                                        \
  }

EIGEN_MEMBER_FUNCTOR(squaredNorm, Size * NumTraits<Scalar>::MulCost + (Size-1)*NumTraits<Scalar>::AddCost);
EIGEN_MEMBER_FUNCTOR(norm, (Size+5) * NumTraits<Scalar>::MulCost + (Size-1)*NumTraits<Scalar>::AddCost);
EIGEN_MEMBER_FUNCTOR(sum, (Size-1)*NumTraits<Scalar>::AddCost);
EIGEN_MEMBER_FUNCTOR(minCoeff, (Size-1)*NumTraits<Scalar>::AddCost);
EIGEN_MEMBER_FUNCTOR(maxCoeff, (Size-1)*NumTraits<Scalar>::AddCost);
EIGEN_MEMBER_FUNCTOR(all, (Size-1)*NumTraits<Scalar>::AddCost);
EIGEN_MEMBER_FUNCTOR(any, (Size-1)*NumTraits<Scalar>::AddCost);
EIGEN_MEMBER_FUNCTOR(count, (Size-1)*NumTraits<Scalar>::AddCost);

/** \internal */
template <typename BinaryOp, typename Scalar>
struct ei_member_redux {
  typedef typename ei_result_of<
                     BinaryOp(Scalar)
                   >::type  result_type;
  template<typename _Scalar, int Size> struct Cost
  { enum { value = (Size-1) * ei_functor_traits<BinaryOp>::Cost }; };
  ei_member_redux(const BinaryOp func) : m_functor(func) {}
  template<typename Derived>
  inline result_type operator()(const MatrixBase<Derived>& mat) const
  { return mat.redux(m_functor); }
  const BinaryOp m_functor;
};

/** \array_module \ingroup Array
  *
  * \class PartialRedux
  *
  * \brief Pseudo expression providing partial reduction operations
  *
  * \param ExpressionType the type of the object on which to do partial reductions
  * \param Direction indicates the direction of the redux (Vertical or Horizontal)
  *
  * This class represents a pseudo expression with partial reduction features.
  * It is the return type of MatrixBase::colwise() and MatrixBase::rowwise()
  * and most of the time this is the only way it is used.
  *
  * Example: \include MatrixBase_colwise.cpp
  * Output: \verbinclude MatrixBase_colwise.out
  *
  * \sa MatrixBase::colwise(), MatrixBase::rowwise(), class PartialReduxExpr
  */
template<typename ExpressionType, int Direction> class PartialRedux
{
  public:

    typedef typename ei_traits<ExpressionType>::Scalar Scalar;
    typedef typename ei_meta_if<ei_must_nest_by_value<ExpressionType>::ret,
        ExpressionType, const ExpressionType&>::ret ExpressionTypeNested;

    template<template<typename _Scalar> class Functor> struct ReturnType
    {
      typedef PartialReduxExpr<ExpressionType,
                               Functor<typename ei_traits<ExpressionType>::Scalar>,
                               Direction
                              > Type;
    };

    template<typename BinaryOp> struct ReduxReturnType
    {
      typedef PartialReduxExpr<ExpressionType,
                               ei_member_redux<BinaryOp,typename ei_traits<ExpressionType>::Scalar>,
                               Direction
                              > Type;
    };

    typedef typename ExpressionType::PlainMatrixType CrossReturnType;
    
    inline PartialRedux(const ExpressionType& matrix) : m_matrix(matrix) {}

    /** \internal */
    inline const ExpressionType& _expression() const { return m_matrix; }

    template<typename BinaryOp>
    const typename ReduxReturnType<BinaryOp>::Type
    redux(const BinaryOp& func = BinaryOp()) const;

    /** \returns a row (or column) vector expression of the smallest coefficient
      * of each column (or row) of the referenced expression.
      *
      * Example: \include PartialRedux_minCoeff.cpp
      * Output: \verbinclude PartialRedux_minCoeff.out
      *
      * \sa MatrixBase::minCoeff() */
    const typename ReturnType<ei_member_minCoeff>::Type minCoeff() const
    { return _expression(); }

    /** \returns a row (or column) vector expression of the largest coefficient
      * of each column (or row) of the referenced expression.
      *
      * Example: \include PartialRedux_maxCoeff.cpp
      * Output: \verbinclude PartialRedux_maxCoeff.out
      *
      * \sa MatrixBase::maxCoeff() */
    const typename ReturnType<ei_member_maxCoeff>::Type maxCoeff() const
    { return _expression(); }

    /** \returns a row (or column) vector expression of the squared norm
      * of each column (or row) of the referenced expression.
      *
      * Example: \include PartialRedux_squaredNorm.cpp
      * Output: \verbinclude PartialRedux_squaredNorm.out
      *
      * \sa MatrixBase::squaredNorm() */
    const typename ReturnType<ei_member_squaredNorm>::Type squaredNorm() const
    { return _expression(); }

    /** \returns a row (or column) vector expression of the norm
      * of each column (or row) of the referenced expression.
      *
      * Example: \include PartialRedux_norm.cpp
      * Output: \verbinclude PartialRedux_norm.out
      *
      * \sa MatrixBase::norm() */
    const typename ReturnType<ei_member_norm>::Type norm() const
    { return _expression(); }

    /** \returns a row (or column) vector expression of the sum
      * of each column (or row) of the referenced expression.
      *
      * Example: \include PartialRedux_sum.cpp
      * Output: \verbinclude PartialRedux_sum.out
      *
      * \sa MatrixBase::sum() */
    const typename ReturnType<ei_member_sum>::Type sum() const
    { return _expression(); }

    /** \returns a row (or column) vector expression representing
      * whether \b all coefficients of each respective column (or row) are \c true.
      *
      * \sa MatrixBase::all() */
    const typename ReturnType<ei_member_all>::Type all() const
    { return _expression(); }

    /** \returns a row (or column) vector expression representing
      * whether \b at \b least one coefficient of each respective column (or row) is \c true.
      *
      * \sa MatrixBase::any() */
    const typename ReturnType<ei_member_any>::Type any() const
    { return _expression(); }
    
    /** \returns a row (or column) vector expression representing
      * the number of \c true coefficients of each respective column (or row).
      *
      * Example: \include PartialRedux_count.cpp
      * Output: \verbinclude PartialRedux_count.out
      *
      * \sa MatrixBase::count() */
    const PartialReduxExpr<ExpressionType, ei_member_count<int>, Direction> count() const
    { return _expression(); }

    /** \returns a 3x3 matrix expression of the cross product
      * of each column or row of the referenced expression with the \a other vector.
      *
      * \geometry_module
      *
      * \sa MatrixBase::cross() */
    template<typename OtherDerived>
    const CrossReturnType cross(const MatrixBase<OtherDerived>& other) const
    {
      EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE(CrossReturnType,3,3)
      EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(OtherDerived,3)
      EIGEN_STATIC_ASSERT((ei_is_same_type<Scalar, typename OtherDerived::Scalar>::ret),
        YOU_MIXED_DIFFERENT_NUMERIC_TYPES__YOU_NEED_TO_USE_THE_CAST_METHOD_OF_MATRIXBASE_TO_CAST_NUMERIC_TYPES_EXPLICITLY)

      if(Direction==Vertical)
        return (CrossReturnType()
                                 << _expression().col(0).cross(other),
                                    _expression().col(1).cross(other),
                                    _expression().col(2).cross(other)).finished();
      else
        return (CrossReturnType() 
                                 << _expression().row(0).cross(other),
                                    _expression().row(1).cross(other),
                                    _expression().row(2).cross(other)).finished();
    }

  protected:
    ExpressionTypeNested m_matrix;
};

/** \array_module
  *
  * \returns a PartialRedux wrapper of *this providing additional partial reduction operations
  *
  * Example: \include MatrixBase_colwise.cpp
  * Output: \verbinclude MatrixBase_colwise.out
  *
  * \sa rowwise(), class PartialRedux
  */
template<typename Derived>
inline const PartialRedux<Derived,Vertical>
MatrixBase<Derived>::colwise() const
{
  return derived();
}

/** \array_module
  *
  * \returns a PartialRedux wrapper of *this providing additional partial reduction operations
  *
  * Example: \include MatrixBase_rowwise.cpp
  * Output: \verbinclude MatrixBase_rowwise.out
  *
  * \sa colwise(), class PartialRedux
  */
template<typename Derived>
inline const PartialRedux<Derived,Horizontal>
MatrixBase<Derived>::rowwise() const
{
  return derived();
}

/** \returns a row or column vector expression of \c *this reduxed by \a func
  *
  * The template parameter \a BinaryOp is the type of the functor
  * of the custom redux operator. Note that func must be an associative operator.
  *
  * \sa class PartialRedux, MatrixBase::colwise(), MatrixBase::rowwise()
  */
template<typename ExpressionType, int Direction>
template<typename BinaryOp>
const typename PartialRedux<ExpressionType,Direction>::template ReduxReturnType<BinaryOp>::Type
PartialRedux<ExpressionType,Direction>::redux(const BinaryOp& func) const
{
  return typename ReduxReturnType<BinaryOp>::Type(_expression(), func);
}

#endif // EIGEN_PARTIAL_REDUX_H
