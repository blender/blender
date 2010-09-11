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

#ifndef EIGEN_CWISE_NULLARY_OP_H
#define EIGEN_CWISE_NULLARY_OP_H

/** \class CwiseNullaryOp
  *
  * \brief Generic expression of a matrix where all coefficients are defined by a functor
  *
  * \param NullaryOp template functor implementing the operator
  *
  * This class represents an expression of a generic nullary operator.
  * It is the return type of the Ones(), Zero(), Constant(), Identity() and Random() functions,
  * and most of the time this is the only way it is used.
  *
  * However, if you want to write a function returning such an expression, you
  * will need to use this class.
  *
  * \sa class CwiseUnaryOp, class CwiseBinaryOp, MatrixBase::NullaryExpr()
  */
template<typename NullaryOp, typename MatrixType>
struct ei_traits<CwiseNullaryOp<NullaryOp, MatrixType> > : ei_traits<MatrixType>
{
  enum {
    Flags = (ei_traits<MatrixType>::Flags
      & (  HereditaryBits
         | (ei_functor_has_linear_access<NullaryOp>::ret ? LinearAccessBit : 0)
         | (ei_functor_traits<NullaryOp>::PacketAccess ? PacketAccessBit : 0)))
      | (ei_functor_traits<NullaryOp>::IsRepeatable ? 0 : EvalBeforeNestingBit),
    CoeffReadCost = ei_functor_traits<NullaryOp>::Cost
  };
};

template<typename NullaryOp, typename MatrixType>
class CwiseNullaryOp : ei_no_assignment_operator,
  public MatrixBase<CwiseNullaryOp<NullaryOp, MatrixType> >
{
  public:

    EIGEN_GENERIC_PUBLIC_INTERFACE(CwiseNullaryOp)

    CwiseNullaryOp(int rows, int cols, const NullaryOp& func = NullaryOp())
      : m_rows(rows), m_cols(cols), m_functor(func)
    {
      ei_assert(rows > 0
          && (RowsAtCompileTime == Dynamic || RowsAtCompileTime == rows)
          && cols > 0
          && (ColsAtCompileTime == Dynamic || ColsAtCompileTime == cols));
    }

    EIGEN_STRONG_INLINE int rows() const { return m_rows.value(); }
    EIGEN_STRONG_INLINE int cols() const { return m_cols.value(); }

    EIGEN_STRONG_INLINE const Scalar coeff(int rows, int cols) const
    {
      return m_functor(rows, cols);
    }

    template<int LoadMode>
    EIGEN_STRONG_INLINE PacketScalar packet(int, int) const
    {
      return m_functor.packetOp();
    }

    EIGEN_STRONG_INLINE const Scalar coeff(int index) const
    {
      if(RowsAtCompileTime == 1)
        return m_functor(0, index);
      else
        return m_functor(index, 0);
    }

    template<int LoadMode>
    EIGEN_STRONG_INLINE PacketScalar packet(int) const
    {
      return m_functor.packetOp();
    }

  protected:
    const ei_int_if_dynamic<RowsAtCompileTime> m_rows;
    const ei_int_if_dynamic<ColsAtCompileTime> m_cols;
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
MatrixBase<Derived>::NullaryExpr(int rows, int cols, const CustomNullaryOp& func)
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
MatrixBase<Derived>::NullaryExpr(int size, const CustomNullaryOp& func)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  ei_assert(IsVectorAtCompileTime);
  if(RowsAtCompileTime == 1) return CwiseNullaryOp<CustomNullaryOp, Derived>(1, size, func);
  else return CwiseNullaryOp<CustomNullaryOp, Derived>(size, 1, func);
}

/** \returns an expression of a matrix defined by a custom functor \a func
  *
  * This variant is only for fixed-size MatrixBase types. For dynamic-size types, you
  * need to use the variants taking size arguments.
  *
  * The template parameter \a CustomNullaryOp is the type of the functor.
  *
  * \sa class CwiseNullaryOp
  */
template<typename Derived>
template<typename CustomNullaryOp>
EIGEN_STRONG_INLINE const CwiseNullaryOp<CustomNullaryOp, Derived>
MatrixBase<Derived>::NullaryExpr(const CustomNullaryOp& func)
{
  return CwiseNullaryOp<CustomNullaryOp, Derived>(RowsAtCompileTime, ColsAtCompileTime, func);
}

/** \returns an expression of a constant matrix of value \a value
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
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::ConstantReturnType
MatrixBase<Derived>::Constant(int rows, int cols, const Scalar& value)
{
  return NullaryExpr(rows, cols, ei_scalar_constant_op<Scalar>(value));
}

/** \returns an expression of a constant matrix of value \a value
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
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::ConstantReturnType
MatrixBase<Derived>::Constant(int size, const Scalar& value)
{
  return NullaryExpr(size, ei_scalar_constant_op<Scalar>(value));
}

/** \returns an expression of a constant matrix of value \a value
  *
  * This variant is only for fixed-size MatrixBase types. For dynamic-size types, you
  * need to use the variants taking size arguments.
  *
  * The template parameter \a CustomNullaryOp is the type of the functor.
  *
  * \sa class CwiseNullaryOp
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::ConstantReturnType
MatrixBase<Derived>::Constant(const Scalar& value)
{
  EIGEN_STATIC_ASSERT_FIXED_SIZE(Derived)
  return NullaryExpr(RowsAtCompileTime, ColsAtCompileTime, ei_scalar_constant_op<Scalar>(value));
}

/** \returns true if all coefficients in this matrix are approximately equal to \a value, to within precision \a prec */
template<typename Derived>
bool MatrixBase<Derived>::isApproxToConstant
(const Scalar& value, RealScalar prec) const
{
  for(int j = 0; j < cols(); ++j)
    for(int i = 0; i < rows(); ++i)
      if(!ei_isApprox(coeff(i, j), value, prec))
        return false;
  return true;
}

/** This is just an alias for isApproxToConstant().
  *
  * \returns true if all coefficients in this matrix are approximately equal to \a value, to within precision \a prec */
template<typename Derived>
bool MatrixBase<Derived>::isConstant
(const Scalar& value, RealScalar prec) const
{
  return isApproxToConstant(value, prec);
}

/** Alias for setConstant(): sets all coefficients in this expression to \a value.
  *
  * \sa setConstant(), Constant(), class CwiseNullaryOp
  */
template<typename Derived>
EIGEN_STRONG_INLINE void MatrixBase<Derived>::fill(const Scalar& value)
{
  setConstant(value);
}

/** Sets all coefficients in this expression to \a value.
  *
  * \sa fill(), setConstant(int,const Scalar&), setConstant(int,int,const Scalar&), setZero(), setOnes(), Constant(), class CwiseNullaryOp, setZero(), setOnes()
  */
template<typename Derived>
EIGEN_STRONG_INLINE Derived& MatrixBase<Derived>::setConstant(const Scalar& value)
{
  return derived() = Constant(rows(), cols(), value);
}

/** Resizes to the given \a size, and sets all coefficients in this expression to the given \a value.
  *
  * \only_for_vectors
  *
  * Example: \include Matrix_set_int.cpp
  * Output: \verbinclude Matrix_setConstant_int.out
  *
  * \sa MatrixBase::setConstant(const Scalar&), setConstant(int,int,const Scalar&), class CwiseNullaryOp, MatrixBase::Constant(const Scalar&)
  */
template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
EIGEN_STRONG_INLINE Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>&
Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>::setConstant(int size, const Scalar& value)
{
  resize(size);
  return setConstant(value);
}

/** Resizes to the given size, and sets all coefficients in this expression to the given \a value.
  *
  * \param rows the new number of rows
  * \param cols the new number of columns
  *
  * Example: \include Matrix_setConstant_int_int.cpp
  * Output: \verbinclude Matrix_setConstant_int_int.out
  *
  * \sa MatrixBase::setConstant(const Scalar&), setConstant(int,const Scalar&), class CwiseNullaryOp, MatrixBase::Constant(const Scalar&)
  */
template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
EIGEN_STRONG_INLINE Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>&
Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>::setConstant(int rows, int cols, const Scalar& value)
{
  resize(rows, cols);
  return setConstant(value);
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
  * \addexample Zero \label How to take get a zero matrix
  *
  * Example: \include MatrixBase_zero_int_int.cpp
  * Output: \verbinclude MatrixBase_zero_int_int.out
  *
  * \sa Zero(), Zero(int)
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::ConstantReturnType
MatrixBase<Derived>::Zero(int rows, int cols)
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
  * \sa Zero(), Zero(int,int)
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::ConstantReturnType
MatrixBase<Derived>::Zero(int size)
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
  * \sa Zero(int), Zero(int,int)
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::ConstantReturnType
MatrixBase<Derived>::Zero()
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
bool MatrixBase<Derived>::isZero(RealScalar prec) const
{
  for(int j = 0; j < cols(); ++j)
    for(int i = 0; i < rows(); ++i)
      if(!ei_isMuchSmallerThan(coeff(i, j), static_cast<Scalar>(1), prec))
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
EIGEN_STRONG_INLINE Derived& MatrixBase<Derived>::setZero()
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
  * \sa MatrixBase::setZero(), setZero(int,int), class CwiseNullaryOp, MatrixBase::Zero()
  */
template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
EIGEN_STRONG_INLINE Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>&
Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>::setZero(int size)
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
  * \sa MatrixBase::setZero(), setZero(int), class CwiseNullaryOp, MatrixBase::Zero()
  */
template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
EIGEN_STRONG_INLINE Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>&
Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>::setZero(int rows, int cols)
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
  * \addexample One \label How to get a matrix with all coefficients equal one
  *
  * Example: \include MatrixBase_ones_int_int.cpp
  * Output: \verbinclude MatrixBase_ones_int_int.out
  *
  * \sa Ones(), Ones(int), isOnes(), class Ones
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::ConstantReturnType
MatrixBase<Derived>::Ones(int rows, int cols)
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
  * \sa Ones(), Ones(int,int), isOnes(), class Ones
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::ConstantReturnType
MatrixBase<Derived>::Ones(int size)
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
  * \sa Ones(int), Ones(int,int), isOnes(), class Ones
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::ConstantReturnType
MatrixBase<Derived>::Ones()
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
bool MatrixBase<Derived>::isOnes
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
EIGEN_STRONG_INLINE Derived& MatrixBase<Derived>::setOnes()
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
  * \sa MatrixBase::setOnes(), setOnes(int,int), class CwiseNullaryOp, MatrixBase::Ones()
  */
template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
EIGEN_STRONG_INLINE Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>&
Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>::setOnes(int size)
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
  * \sa MatrixBase::setOnes(), setOnes(int), class CwiseNullaryOp, MatrixBase::Ones()
  */
template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
EIGEN_STRONG_INLINE Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>&
Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>::setOnes(int rows, int cols)
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
  * \addexample Identity \label How to get an identity matrix
  *
  * Example: \include MatrixBase_identity_int_int.cpp
  * Output: \verbinclude MatrixBase_identity_int_int.out
  *
  * \sa Identity(), setIdentity(), isIdentity()
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::IdentityReturnType
MatrixBase<Derived>::Identity(int rows, int cols)
{
  return NullaryExpr(rows, cols, ei_scalar_identity_op<Scalar>());
}

/** \returns an expression of the identity matrix (not necessarily square).
  *
  * This variant is only for fixed-size MatrixBase types. For dynamic-size types, you
  * need to use the variant taking size arguments.
  *
  * Example: \include MatrixBase_identity.cpp
  * Output: \verbinclude MatrixBase_identity.out
  *
  * \sa Identity(int,int), setIdentity(), isIdentity()
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::IdentityReturnType
MatrixBase<Derived>::Identity()
{
  EIGEN_STATIC_ASSERT_FIXED_SIZE(Derived)
  return NullaryExpr(RowsAtCompileTime, ColsAtCompileTime, ei_scalar_identity_op<Scalar>());
}

/** \returns true if *this is approximately equal to the identity matrix
  *          (not necessarily square),
  *          within the precision given by \a prec.
  *
  * Example: \include MatrixBase_isIdentity.cpp
  * Output: \verbinclude MatrixBase_isIdentity.out
  *
  * \sa class CwiseNullaryOp, Identity(), Identity(int,int), setIdentity()
  */
template<typename Derived>
bool MatrixBase<Derived>::isIdentity
(RealScalar prec) const
{
  for(int j = 0; j < cols(); ++j)
  {
    for(int i = 0; i < rows(); ++i)
    {
      if(i == j)
      {
        if(!ei_isApprox(coeff(i, j), static_cast<Scalar>(1), prec))
          return false;
      }
      else
      {
        if(!ei_isMuchSmallerThan(coeff(i, j), static_cast<RealScalar>(1), prec))
          return false;
      }
    }
  }
  return true;
}

template<typename Derived, bool Big = (Derived::SizeAtCompileTime>=16)>
struct ei_setIdentity_impl
{
  static EIGEN_STRONG_INLINE Derived& run(Derived& m)
  {
    return m = Derived::Identity(m.rows(), m.cols());
  }
};

template<typename Derived>
struct ei_setIdentity_impl<Derived, true>
{
  static EIGEN_STRONG_INLINE Derived& run(Derived& m)
  {
    m.setZero();
    const int size = std::min(m.rows(), m.cols());
    for(int i = 0; i < size; ++i) m.coeffRef(i,i) = typename Derived::Scalar(1);
    return m;
  }
};

/** Writes the identity expression (not necessarily square) into *this.
  *
  * Example: \include MatrixBase_setIdentity.cpp
  * Output: \verbinclude MatrixBase_setIdentity.out
  *
  * \sa class CwiseNullaryOp, Identity(), Identity(int,int), isIdentity()
  */
template<typename Derived>
EIGEN_STRONG_INLINE Derived& MatrixBase<Derived>::setIdentity()
{
  return ei_setIdentity_impl<Derived>::run(derived());
}

/** Resizes to the given size, and writes the identity expression (not necessarily square) into *this.
  *
  * \param rows the new number of rows
  * \param cols the new number of columns
  *
  * Example: \include Matrix_setIdentity_int_int.cpp
  * Output: \verbinclude Matrix_setIdentity_int_int.out
  *
  * \sa MatrixBase::setIdentity(), class CwiseNullaryOp, MatrixBase::Identity()
  */
template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
EIGEN_STRONG_INLINE Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>&
Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>::setIdentity(int rows, int cols)
{
  resize(rows, cols);
  return setIdentity();
}

/** \returns an expression of the i-th unit (basis) vector.
  *
  * \only_for_vectors
  *
  * \sa MatrixBase::Unit(int), MatrixBase::UnitX(), MatrixBase::UnitY(), MatrixBase::UnitZ(), MatrixBase::UnitW()
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::BasisReturnType MatrixBase<Derived>::Unit(int size, int i)
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
  * \sa MatrixBase::Unit(int,int), MatrixBase::UnitX(), MatrixBase::UnitY(), MatrixBase::UnitZ(), MatrixBase::UnitW()
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::BasisReturnType MatrixBase<Derived>::Unit(int i)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return BasisReturnType(SquareMatrixType::Identity(),i);
}

/** \returns an expression of the X axis unit vector (1{,0}^*)
  *
  * \only_for_vectors
  *
  * \sa MatrixBase::Unit(int,int), MatrixBase::Unit(int), MatrixBase::UnitY(), MatrixBase::UnitZ(), MatrixBase::UnitW()
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::BasisReturnType MatrixBase<Derived>::UnitX()
{ return Derived::Unit(0); }

/** \returns an expression of the Y axis unit vector (0,1{,0}^*)
  *
  * \only_for_vectors
  *
  * \sa MatrixBase::Unit(int,int), MatrixBase::Unit(int), MatrixBase::UnitY(), MatrixBase::UnitZ(), MatrixBase::UnitW()
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::BasisReturnType MatrixBase<Derived>::UnitY()
{ return Derived::Unit(1); }

/** \returns an expression of the Z axis unit vector (0,0,1{,0}^*)
  *
  * \only_for_vectors
  *
  * \sa MatrixBase::Unit(int,int), MatrixBase::Unit(int), MatrixBase::UnitY(), MatrixBase::UnitZ(), MatrixBase::UnitW()
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::BasisReturnType MatrixBase<Derived>::UnitZ()
{ return Derived::Unit(2); }

/** \returns an expression of the W axis unit vector (0,0,0,1)
  *
  * \only_for_vectors
  *
  * \sa MatrixBase::Unit(int,int), MatrixBase::Unit(int), MatrixBase::UnitY(), MatrixBase::UnitZ(), MatrixBase::UnitW()
  */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::BasisReturnType MatrixBase<Derived>::UnitW()
{ return Derived::Unit(3); }

#endif // EIGEN_CWISE_NULLARY_OP_H
