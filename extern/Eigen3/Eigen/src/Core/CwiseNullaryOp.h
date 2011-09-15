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

#ifndef EIGEN_CWISE_NULLARY_OP_H
#define EIGEN_CWISE_NULLARY_OP_H

/** \class CwiseNullaryOp
  * \ingroup Core_Module
  *
  * \brief Generic expression of a matrix where all coefficients are defined by a functor
  *
  * \param NullaryOp template functor implementing the operator
  * \param PlainObjectType the underlying plain matrix/array type
  *
  * This class represents an expression of a generic nullary operator.
  * It is the return type of the Ones(), Zero(), Constant(), Identity() and Random() methods,
  * and most of the time this is the only way it is used.
  *
  * However, if you want to write a function returning such an expression, you
  * will need to use this class.
  *
  * \sa class CwiseUnaryOp, class CwiseBinaryOp, DenseBase::NullaryExpr()
  */

namespace internal {
template<typename NullaryOp, typename PlainObjectType>
struct traits<CwiseNullaryOp<NullaryOp, PlainObjectType> > : traits<PlainObjectType>
{
  enum {
    Flags = (traits<PlainObjectType>::Flags
      & (  HereditaryBits
         | (functor_has_linear_access<NullaryOp>::ret ? LinearAccessBit : 0)
         | (functor_traits<NullaryOp>::PacketAccess ? PacketAccessBit : 0)))
      | (functor_traits<NullaryOp>::IsRepeatable ? 0 : EvalBeforeNestingBit),
    CoeffReadCost = functor_traits<NullaryOp>::Cost
  };
};
}

template<typename NullaryOp, typename PlainObjectType>
class CwiseNullaryOp : internal::no_assignment_operator,
  public internal::dense_xpr_base< CwiseNullaryOp<NullaryOp, PlainObjectType> >::type
{
  public:

    typedef typename internal::dense_xpr_base<CwiseNullaryOp>::type Base;
    EIGEN_DENSE_PUBLIC_INTERFACE(CwiseNullaryOp)

    CwiseNullaryOp(Index rows, Index cols, const NullaryOp& func = NullaryOp())
      : m_rows(rows), m_cols(cols), m_functor(func)
    {
      eigen_assert(rows >= 0
            && (RowsAtCompileTime == Dynamic || RowsAtCompileTime == rows)
            &&  cols >= 0
            && (ColsAtCompileTime == Dynamic || ColsAtCompileTime == cols));
    }

    EIGEN_STRONG_INLINE Index rows() const { return m_rows.value(); }
    EIGEN_STRONG_INLINE Index cols() const { return m_cols.value(); }

    EIGEN_STRONG_INLINE const Scalar coeff(Index rows, Index cols) const
    {
      return m_functor(rows, cols);
    }

    template<int LoadMode>
    EIGEN_STRONG_INLINE PacketScalar packet(Index row, Index col) const
    {
      return m_functor.packetOp(row, col);
    }

    EIGEN_STRONG_INLINE const Scalar coeff(Index index) const
    {
      return m_functor(index);
    }

    template<int LoadMode>
    EIGEN_STRONG_INLINE PacketScalar packet(Index index) const
    {
      return m_functor.packetOp(index);
    }

  protected:
    const internal::variable_if_dynamic<Index, RowsAtCompileTime> m_rows;
    const internal::variable_if_dynamic<Index, ColsAtCompileTime> m_cols;
    const NullaryOp m_functor;
};


/** \returns an expression of a matrix defined by a custom functor \a func
  *
  * The parameters \a rows and \a cols are the number of rows and of columns of
  * the returned matrix. Must be compatible with this MatrixBase type.
  *
  * This variant is meant to be used for dynamic-size matrix types. For fixed-size types,
  * it is redundant to pass \a rows and \a cols as arguments, so Zero() should be used
  * instead.
  *
  * The template parameter \a CustomNullaryOp is the type of the functor.
  *
  * \sa class CwiseNullaryOp
  */
template<typename Derived>
template<typename CustomNullaryOp>
EIGEN_STRONG_INLINE const CwiseNullaryOp<CustomNullaryOp, Derived>
DenseBase<Derived>::NullaryExpr(Index rows, Index cols, const CustomNullaryOp& func)
{
  return CwiseNullaryOp<CustomNullaryOp, Derived>(rows, cols, func);
}

/** \returns an expression of a matrix defined by a custom functor \a func
  *
  * The parameter \a size is the size of the returned vector.
  * Must be compatible with this MatrixBase type.
  *
  * \only_for_vectors
  *
  * This variant is meant to be used for dynamic-size vector types. For fixed-size types,
  * it is redundant to pass \a size as argument, so Zero() should be used
  * instead.
  *
  * The template parameter \a CustomNullaryOp is the type of the functor.
  *
  * \sa class CwiseNullaryOp
  */
template<typename Derived>
template<typename CustomNullaryOp>
EIGEN_STRONG_INLINE const CwiseNullaryOp<CustomNullaryOp, Derived>
DenseBase<Derived>::NullaryExpr(Index size, const CustomNullaryOp& func)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  if(RowsAtCompileTime == 1) return CwiseNullaryOp<CustomNullaryOp, Derived>(1, size, func);
  else return CwiseNullaryOp<CustomNullaryOp, Derived>(size, 1, func);
}

/** \returns an expression of a matrix defined by a custom functor \a func
  *
  * This variant is only for fixed-size DenseBase types. For dynamic-size types, you
  * need to use the variants taking size arguments.
  *
  * The template parameter \a CustomNullaryOp is the type of the functor.
  *
  * \sa class CwiseNullaryOp
  */
template<typename Derived>
template<typename CustomNullaryOp>
EIGEN_STRONG_INLINE const CwiseNullaryOp<CustomNullaryOp, Derived>
DenseBase<Derived>::NullaryExpr(const CustomNullaryOp& func)
{
  return CwiseNullaryOp<CustomNullaryOp, Derived>(RowsAtCompileTime, ColsAtCompileTime, func);
}

/** \returns an expression of a constant matrix of value \a value
  *
  * The parameters \a rows and \a cols are the number of rows and of columns of
  * the returned matrix. Must be compatible with this DenseBase type.
  *
  * This variant is meant to be used for dynamic-size matrix types. For fixed-size types,
  * it is redundant to pass \a rows and \a cols as arguments, so Zero() should be used
  * instead.
  *
  * The template parameter \a CustomNullaryOp is the type of the functor.
  *
  * \sa class CwiseNullaryOp
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename DenseBase<Derived>::ConstantReturnType
DenseBase<Derived>::Constant(Index rows, Index cols, const Scalar& value)
{
  return DenseBase<Derived>::NullaryExpr(rows, cols, internal::scalar_constant_op<Scalar>(value));
}

/** \returns an expression of a constant matrix of value \a value
  *
  * The parameter \a size is the size of the returned vector.
  * Must be compatible with this DenseBase type.
  *
  * \only_for_vectors
  *
  * This variant is meant to be used for dynamic-size vector types. For fixed-size types,
  * it is redundant to pass \a size as argument, so Zero() should be used
  * instead.
  *
  * The template parameter \a CustomNullaryOp is the type of the functor.
  *
  * \sa class CwiseNullaryOp
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename DenseBase<Derived>::ConstantReturnType
DenseBase<Derived>::Constant(Index size, const Scalar& value)
{
  return DenseBase<Derived>::NullaryExpr(size, internal::scalar_constant_op<Scalar>(value));
}

/** \returns an expression of a constant matrix of value \a value
  *
  * This variant is only for fixed-size DenseBase types. For dynamic-size types, you
  * need to use the variants taking size arguments.
  *
  * The template parameter \a CustomNullaryOp is the type of the functor.
  *
  * \sa class CwiseNullaryOp
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename DenseBase<Derived>::ConstantReturnType
DenseBase<Derived>::Constant(const Scalar& value)
{
  EIGEN_STATIC_ASSERT_FIXED_SIZE(Derived)
  return DenseBase<Derived>::NullaryExpr(RowsAtCompileTime, ColsAtCompileTime, internal::scalar_constant_op<Scalar>(value));
}

/**
  * \brief Sets a linearly space vector.
  *
  * The function generates 'size' equally spaced values in the closed interval [low,high].
  * This particular version of LinSpaced() uses sequential access, i.e. vector access is
  * assumed to be a(0), a(1), ..., a(size). This assumption allows for better vectorization
  * and yields faster code than the random access version.
  *
  * \only_for_vectors
  *
  * Example: \include DenseBase_LinSpaced_seq.cpp
  * Output: \verbinclude DenseBase_LinSpaced_seq.out
  *
  * \sa setLinSpaced(Index,const Scalar&,const Scalar&), LinSpaced(Index,Scalar,Scalar), CwiseNullaryOp
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename DenseBase<Derived>::SequentialLinSpacedReturnType
DenseBase<Derived>::LinSpaced(Sequential_t, Index size, const Scalar& low, const Scalar& high)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return DenseBase<Derived>::NullaryExpr(size, internal::linspaced_op<Scalar,false>(low,high,size));
}

/**
  * \copydoc DenseBase::LinSpaced(Sequential_t, Index, const Scalar&, const Scalar&)
  * Special version for fixed size types which does not require the size parameter.
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename DenseBase<Derived>::SequentialLinSpacedReturnType
DenseBase<Derived>::LinSpaced(Sequential_t, const Scalar& low, const Scalar& high)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  EIGEN_STATIC_ASSERT_FIXED_SIZE(Derived)
  return DenseBase<Derived>::NullaryExpr(Derived::SizeAtCompileTime, internal::linspaced_op<Scalar,false>(low,high,Derived::SizeAtCompileTime));
}

/**
  * \brief Sets a linearly space vector.
  *
  * The function generates 'size' equally spaced values in the closed interval [low,high].
  *
  * \only_for_vectors
  *
  * Example: \include DenseBase_LinSpaced.cpp
  * Output: \verbinclude DenseBase_LinSpaced.out
  *
  * \sa setLinSpaced(Index,const Scalar&,const Scalar&), LinSpaced(Sequential_t,Index,const Scalar&,const Scalar&,Index), CwiseNullaryOp
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename DenseBase<Derived>::RandomAccessLinSpacedReturnType
DenseBase<Derived>::LinSpaced(Index size, const Scalar& low, const Scalar& high)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return DenseBase<Derived>::NullaryExpr(size, internal::linspaced_op<Scalar,true>(low,high,size));
}

/**
  * \copydoc DenseBase::LinSpaced(Index, const Scalar&, const Scalar&)
  * Special version for fixed size types which does not require the size parameter.
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename DenseBase<Derived>::RandomAccessLinSpacedReturnType
DenseBase<Derived>::LinSpaced(const Scalar& low, const Scalar& high)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  EIGEN_STATIC_ASSERT_FIXED_SIZE(Derived)
  return DenseBase<Derived>::NullaryExpr(Derived::SizeAtCompileTime, internal::linspaced_op<Scalar,true>(low,high,Derived::SizeAtCompileTime));
}

/** \returns true if all coefficients in this matrix are approximately equal to \a value, to within precision \a prec */
template<typename Derived>
bool DenseBase<Derived>::isApproxToConstant
(const Scalar& value, RealScalar prec) const
{
  for(Index j = 0; j < cols(); ++j)
    for(Index i = 0; i < rows(); ++i)
      if(!internal::isApprox(this->coeff(i, j), value, prec))
        return false;
  return true;
}

/** This is just an alias for isApproxToConstant().
  *
  * \returns true if all coefficients in this matrix are approximately equal to \a value, to within precision \a prec */
template<typename Derived>
bool DenseBase<Derived>::isConstant
(const Scalar& value, RealScalar prec) const
{
  return isApproxToConstant(value, prec);
}

/** Alias for setConstant(): sets all coefficients in this expression to \a value.
  *
  * \sa setConstant(), Constant(), class CwiseNullaryOp
  */
template<typename Derived>
EIGEN_STRONG_INLINE void DenseBase<Derived>::fill(const Scalar& value)
{
  setConstant(value);
}

/** Sets all coefficients in this expression to \a value.
  *
  * \sa fill(), setConstant(Index,const Scalar&), setConstant(Index,Index,const Scalar&), setZero(), setOnes(), Constant(), class CwiseNullaryOp, setZero(), setOnes()
  */
template<typename Derived>
EIGEN_STRONG_INLINE Derived& DenseBase<Derived>::setConstant(const Scalar& value)
{
  return derived() = Constant(rows(), cols(), value);
}

/** Resizes to the given \a size, and sets all coefficients in this expression to the given \a value.
  *
  * \only_for_vectors
  *
  * Example: \include Matrix_setConstant_int.cpp
  * Output: \verbinclude Matrix_setConstant_int.out
  *
  * \sa MatrixBase::setConstant(const Scalar&), setConstant(Index,Index,const Scalar&), class CwiseNullaryOp, MatrixBase::Constant(const Scalar&)
  */
template<typename Derived>
EIGEN_STRONG_INLINE Derived&
PlainObjectBase<Derived>::setConstant(Index size, const Scalar& value)
{
  resize(size);
  return setConstant(value);
}

/** Resizes to the given size, and sets all coefficients in this expression to the given \a value.
  *
  * \param rows the new number of rows
  * \param cols the new number of columns
  * \param value the value to which all coefficients are set
  *
  * Example: \include Matrix_setConstant_int_int.cpp
  * Output: \verbinclude Matrix_setConstant_int_int.out
  *
  * \sa MatrixBase::setConstant(const Scalar&), setConstant(Index,const Scalar&), class CwiseNullaryOp, MatrixBase::Constant(const Scalar&)
  */
template<typename Derived>
EIGEN_STRONG_INLINE Derived&
PlainObjectBase<Derived>::setConstant(Index rows, Index cols, const Scalar& value)
{
  resize(rows, cols);
  return setConstant(value);
}

/**
  * \brief Sets a linearly space vector.
  *
  * The function generates 'size' equally spaced values in the closed interval [low,high].
  *
  * \only_for_vectors
  *
  * Example: \include DenseBase_setLinSpaced.cpp
  * Output: \verbinclude DenseBase_setLinSpaced.out
  *
  * \sa CwiseNullaryOp
  */
template<typename Derived>
EIGEN_STRONG_INLINE Derived& DenseBase<Derived>::setLinSpaced(Index size, const Scalar& low, const Scalar& high)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return derived() = Derived::NullaryExpr(size, internal::linspaced_op<Scalar,false>(low,high,size));
}

// zero:

/** \returns an expression of a zero matrix.
  *
  * The parameters \a rows and \a cols are the number of rows and of columns of
  * the returned matrix. Must be compatible with this MatrixBase type.
  *
  * This variant is meant to be used for dynamic-size matrix types. For fixed-size types,
  * it is redundant to pass \a rows and \a cols as arguments, so Zero() should be used
  * instead.
  *
  * Example: \include MatrixBase_zero_int_int.cpp
  * Output: \verbinclude MatrixBase_zero_int_int.out
  *
  * \sa Zero(), Zero(Index)
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename DenseBase<Derived>::ConstantReturnType
DenseBase<Derived>::Zero(Index rows, Index cols)
{
  return Constant(rows, cols, Scalar(0));
}

/** \returns an expression of a zero vector.
  *
  * The parameter \a size is the size of the returned vector.
  * Must be compatible with this MatrixBase type.
  *
  * \only_for_vectors
  *
  * This variant is meant to be used for dynamic-size vector types. For fixed-size types,
  * it is redundant to pass \a size as argument, so Zero() should be used
  * instead.
  *
  * Example: \include MatrixBase_zero_int.cpp
  * Output: \verbinclude MatrixBase_zero_int.out
  *
  * \sa Zero(), Zero(Index,Index)
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename DenseBase<Derived>::ConstantReturnType
DenseBase<Derived>::Zero(Index size)
{
  return Constant(size, Scalar(0));
}

/** \returns an expression of a fixed-size zero matrix or vector.
  *
  * This variant is only for fixed-size MatrixBase types. For dynamic-size types, you
  * need to use the variants taking size arguments.
  *
  * Example: \include MatrixBase_zero.cpp
  * Output: \verbinclude MatrixBase_zero.out
  *
  * \sa Zero(Index), Zero(Index,Index)
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename DenseBase<Derived>::ConstantReturnType
DenseBase<Derived>::Zero()
{
  return Constant(Scalar(0));
}

/** \returns true if *this is approximately equal to the zero matrix,
  *          within the precision given by \a prec.
  *
  * Example: \include MatrixBase_isZero.cpp
  * Output: \verbinclude MatrixBase_isZero.out
  *
  * \sa class CwiseNullaryOp, Zero()
  */
template<typename Derived>
bool DenseBase<Derived>::isZero(RealScalar prec) const
{
  for(Index j = 0; j < cols(); ++j)
    for(Index i = 0; i < rows(); ++i)
      if(!internal::isMuchSmallerThan(this->coeff(i, j), static_cast<Scalar>(1), prec))
        return false;
  return true;
}

/** Sets all coefficients in this expression to zero.
  *
  * Example: \include MatrixBase_setZero.cpp
  * Output: \verbinclude MatrixBase_setZero.out
  *
  * \sa class CwiseNullaryOp, Zero()
  */
template<typename Derived>
EIGEN_STRONG_INLINE Derived& DenseBase<Derived>::setZero()
{
  return setConstant(Scalar(0));
}

/** Resizes to the given \a size, and sets all coefficients in this expression to zero.
  *
  * \only_for_vectors
  *
  * Example: \include Matrix_setZero_int.cpp
  * Output: \verbinclude Matrix_setZero_int.out
  *
  * \sa DenseBase::setZero(), setZero(Index,Index), class CwiseNullaryOp, DenseBase::Zero()
  */
template<typename Derived>
EIGEN_STRONG_INLINE Derived&
PlainObjectBase<Derived>::setZero(Index size)
{
  resize(size);
  return setConstant(Scalar(0));
}

/** Resizes to the given size, and sets all coefficients in this expression to zero.
  *
  * \param rows the new number of rows
  * \param cols the new number of columns
  *
  * Example: \include Matrix_setZero_int_int.cpp
  * Output: \verbinclude Matrix_setZero_int_int.out
  *
  * \sa DenseBase::setZero(), setZero(Index), class CwiseNullaryOp, DenseBase::Zero()
  */
template<typename Derived>
EIGEN_STRONG_INLINE Derived&
PlainObjectBase<Derived>::setZero(Index rows, Index cols)
{
  resize(rows, cols);
  return setConstant(Scalar(0));
}

// ones:

/** \returns an expression of a matrix where all coefficients equal one.
  *
  * The parameters \a rows and \a cols are the number of rows and of columns of
  * the returned matrix. Must be compatible with this MatrixBase type.
  *
  * This variant is meant to be used for dynamic-size matrix types. For fixed-size types,
  * it is redundant to pass \a rows and \a cols as arguments, so Ones() should be used
  * instead.
  *
  * Example: \include MatrixBase_ones_int_int.cpp
  * Output: \verbinclude MatrixBase_ones_int_int.out
  *
  * \sa Ones(), Ones(Index), isOnes(), class Ones
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename DenseBase<Derived>::ConstantReturnType
DenseBase<Derived>::Ones(Index rows, Index cols)
{
  return Constant(rows, cols, Scalar(1));
}

/** \returns an expression of a vector where all coefficients equal one.
  *
  * The parameter \a size is the size of the returned vector.
  * Must be compatible with this MatrixBase type.
  *
  * \only_for_vectors
  *
  * This variant is meant to be used for dynamic-size vector types. For fixed-size types,
  * it is redundant to pass \a size as argument, so Ones() should be used
  * instead.
  *
  * Example: \include MatrixBase_ones_int.cpp
  * Output: \verbinclude MatrixBase_ones_int.out
  *
  * \sa Ones(), Ones(Index,Index), isOnes(), class Ones
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename DenseBase<Derived>::ConstantReturnType
DenseBase<Derived>::Ones(Index size)
{
  return Constant(size, Scalar(1));
}

/** \returns an expression of a fixed-size matrix or vector where all coefficients equal one.
  *
  * This variant is only for fixed-size MatrixBase types. For dynamic-size types, you
  * need to use the variants taking size arguments.
  *
  * Example: \include MatrixBase_ones.cpp
  * Output: \verbinclude MatrixBase_ones.out
  *
  * \sa Ones(Index), Ones(Index,Index), isOnes(), class Ones
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename DenseBase<Derived>::ConstantReturnType
DenseBase<Derived>::Ones()
{
  return Constant(Scalar(1));
}

/** \returns true if *this is approximately equal to the matrix where all coefficients
  *          are equal to 1, within the precision given by \a prec.
  *
  * Example: \include MatrixBase_isOnes.cpp
  * Output: \verbinclude MatrixBase_isOnes.out
  *
  * \sa class CwiseNullaryOp, Ones()
  */
template<typename Derived>
bool DenseBase<Derived>::isOnes
(RealScalar prec) const
{
  return isApproxToConstant(Scalar(1), prec);
}

/** Sets all coefficients in this expression to one.
  *
  * Example: \include MatrixBase_setOnes.cpp
  * Output: \verbinclude MatrixBase_setOnes.out
  *
  * \sa class CwiseNullaryOp, Ones()
  */
template<typename Derived>
EIGEN_STRONG_INLINE Derived& DenseBase<Derived>::setOnes()
{
  return setConstant(Scalar(1));
}

/** Resizes to the given \a size, and sets all coefficients in this expression to one.
  *
  * \only_for_vectors
  *
  * Example: \include Matrix_setOnes_int.cpp
  * Output: \verbinclude Matrix_setOnes_int.out
  *
  * \sa MatrixBase::setOnes(), setOnes(Index,Index), class CwiseNullaryOp, MatrixBase::Ones()
  */
template<typename Derived>
EIGEN_STRONG_INLINE Derived&
PlainObjectBase<Derived>::setOnes(Index size)
{
  resize(size);
  return setConstant(Scalar(1));
}

/** Resizes to the given size, and sets all coefficients in this expression to one.
  *
  * \param rows the new number of rows
  * \param cols the new number of columns
  *
  * Example: \include Matrix_setOnes_int_int.cpp
  * Output: \verbinclude Matrix_setOnes_int_int.out
  *
  * \sa MatrixBase::setOnes(), setOnes(Index), class CwiseNullaryOp, MatrixBase::Ones()
  */
template<typename Derived>
EIGEN_STRONG_INLINE Derived&
PlainObjectBase<Derived>::setOnes(Index rows, Index cols)
{
  resize(rows, cols);
  return setConstant(Scalar(1));
}

// Identity:

/** \returns an expression of the identity matrix (not necessarily square).
  *
  * The parameters \a rows and \a cols are the number of rows and of columns of
  * the returned matrix. Must be compatible with this MatrixBase type.
  *
  * This variant is meant to be used for dynamic-size matrix types. For fixed-size types,
  * it is redundant to pass \a rows and \a cols as arguments, so Identity() should be used
  * instead.
  *
  * Example: \include MatrixBase_identity_int_int.cpp
  * Output: \verbinclude MatrixBase_identity_int_int.out
  *
  * \sa Identity(), setIdentity(), isIdentity()
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::IdentityReturnType
MatrixBase<Derived>::Identity(Index rows, Index cols)
{
  return DenseBase<Derived>::NullaryExpr(rows, cols, internal::scalar_identity_op<Scalar>());
}

/** \returns an expression of the identity matrix (not necessarily square).
  *
  * This variant is only for fixed-size MatrixBase types. For dynamic-size types, you
  * need to use the variant taking size arguments.
  *
  * Example: \include MatrixBase_identity.cpp
  * Output: \verbinclude MatrixBase_identity.out
  *
  * \sa Identity(Index,Index), setIdentity(), isIdentity()
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::IdentityReturnType
MatrixBase<Derived>::Identity()
{
  EIGEN_STATIC_ASSERT_FIXED_SIZE(Derived)
  return MatrixBase<Derived>::NullaryExpr(RowsAtCompileTime, ColsAtCompileTime, internal::scalar_identity_op<Scalar>());
}

/** \returns true if *this is approximately equal to the identity matrix
  *          (not necessarily square),
  *          within the precision given by \a prec.
  *
  * Example: \include MatrixBase_isIdentity.cpp
  * Output: \verbinclude MatrixBase_isIdentity.out
  *
  * \sa class CwiseNullaryOp, Identity(), Identity(Index,Index), setIdentity()
  */
template<typename Derived>
bool MatrixBase<Derived>::isIdentity
(RealScalar prec) const
{
  for(Index j = 0; j < cols(); ++j)
  {
    for(Index i = 0; i < rows(); ++i)
    {
      if(i == j)
      {
        if(!internal::isApprox(this->coeff(i, j), static_cast<Scalar>(1), prec))
          return false;
      }
      else
      {
        if(!internal::isMuchSmallerThan(this->coeff(i, j), static_cast<RealScalar>(1), prec))
          return false;
      }
    }
  }
  return true;
}

namespace internal {

template<typename Derived, bool Big = (Derived::SizeAtCompileTime>=16)>
struct setIdentity_impl
{
  static EIGEN_STRONG_INLINE Derived& run(Derived& m)
  {
    return m = Derived::Identity(m.rows(), m.cols());
  }
};

template<typename Derived>
struct setIdentity_impl<Derived, true>
{
  typedef typename Derived::Index Index;
  static EIGEN_STRONG_INLINE Derived& run(Derived& m)
  {
    m.setZero();
    const Index size = (std::min)(m.rows(), m.cols());
    for(Index i = 0; i < size; ++i) m.coeffRef(i,i) = typename Derived::Scalar(1);
    return m;
  }
};

} // end namespace internal

/** Writes the identity expression (not necessarily square) into *this.
  *
  * Example: \include MatrixBase_setIdentity.cpp
  * Output: \verbinclude MatrixBase_setIdentity.out
  *
  * \sa class CwiseNullaryOp, Identity(), Identity(Index,Index), isIdentity()
  */
template<typename Derived>
EIGEN_STRONG_INLINE Derived& MatrixBase<Derived>::setIdentity()
{
  return internal::setIdentity_impl<Derived>::run(derived());
}

/** \brief Resizes to the given size, and writes the identity expression (not necessarily square) into *this.
  *
  * \param rows the new number of rows
  * \param cols the new number of columns
  *
  * Example: \include Matrix_setIdentity_int_int.cpp
  * Output: \verbinclude Matrix_setIdentity_int_int.out
  *
  * \sa MatrixBase::setIdentity(), class CwiseNullaryOp, MatrixBase::Identity()
  */
template<typename Derived>
EIGEN_STRONG_INLINE Derived& MatrixBase<Derived>::setIdentity(Index rows, Index cols)
{
  derived().resize(rows, cols);
  return setIdentity();
}

/** \returns an expression of the i-th unit (basis) vector.
  *
  * \only_for_vectors
  *
  * \sa MatrixBase::Unit(Index), MatrixBase::UnitX(), MatrixBase::UnitY(), MatrixBase::UnitZ(), MatrixBase::UnitW()
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::BasisReturnType MatrixBase<Derived>::Unit(Index size, Index i)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return BasisReturnType(SquareMatrixType::Identity(size,size), i);
}

/** \returns an expression of the i-th unit (basis) vector.
  *
  * \only_for_vectors
  *
  * This variant is for fixed-size vector only.
  *
  * \sa MatrixBase::Unit(Index,Index), MatrixBase::UnitX(), MatrixBase::UnitY(), MatrixBase::UnitZ(), MatrixBase::UnitW()
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::BasisReturnType MatrixBase<Derived>::Unit(Index i)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return BasisReturnType(SquareMatrixType::Identity(),i);
}

/** \returns an expression of the X axis unit vector (1{,0}^*)
  *
  * \only_for_vectors
  *
  * \sa MatrixBase::Unit(Index,Index), MatrixBase::Unit(Index), MatrixBase::UnitY(), MatrixBase::UnitZ(), MatrixBase::UnitW()
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::BasisReturnType MatrixBase<Derived>::UnitX()
{ return Derived::Unit(0); }

/** \returns an expression of the Y axis unit vector (0,1{,0}^*)
  *
  * \only_for_vectors
  *
  * \sa MatrixBase::Unit(Index,Index), MatrixBase::Unit(Index), MatrixBase::UnitY(), MatrixBase::UnitZ(), MatrixBase::UnitW()
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::BasisReturnType MatrixBase<Derived>::UnitY()
{ return Derived::Unit(1); }

/** \returns an expression of the Z axis unit vector (0,0,1{,0}^*)
  *
  * \only_for_vectors
  *
  * \sa MatrixBase::Unit(Index,Index), MatrixBase::Unit(Index), MatrixBase::UnitY(), MatrixBase::UnitZ(), MatrixBase::UnitW()
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::BasisReturnType MatrixBase<Derived>::UnitZ()
{ return Derived::Unit(2); }

/** \returns an expression of the W axis unit vector (0,0,0,1)
  *
  * \only_for_vectors
  *
  * \sa MatrixBase::Unit(Index,Index), MatrixBase::Unit(Index), MatrixBase::UnitY(), MatrixBase::UnitZ(), MatrixBase::UnitW()
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::BasisReturnType MatrixBase<Derived>::UnitW()
{ return Derived::Unit(3); }

#endif // EIGEN_CWISE_NULLARY_OP_H
