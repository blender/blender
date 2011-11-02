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

#ifndef EIGEN_ARRAY_CWISE_OPERATORS_H
#define EIGEN_ARRAY_CWISE_OPERATORS_H

/***************************************************************************
* The following functions were defined in Core
***************************************************************************/


/** \deprecated ArrayBase::abs() */
template<typename ExpressionType>
EIGEN_STRONG_INLINE const EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_abs_op)
Cwise<ExpressionType>::abs() const
{
  return _expression();
}

/** \deprecated ArrayBase::abs2() */
template<typename ExpressionType>
EIGEN_STRONG_INLINE const EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_abs2_op)
Cwise<ExpressionType>::abs2() const
{
  return _expression();
}

/** \deprecated ArrayBase::exp() */
template<typename ExpressionType>
inline const EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_exp_op)
Cwise<ExpressionType>::exp() const
{
  return _expression();
}

/** \deprecated ArrayBase::log() */
template<typename ExpressionType>
inline const EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_log_op)
Cwise<ExpressionType>::log() const
{
  return _expression();
}

/** \deprecated ArrayBase::operator*() */
template<typename ExpressionType>
template<typename OtherDerived>
EIGEN_STRONG_INLINE const EIGEN_CWISE_PRODUCT_RETURN_TYPE(ExpressionType,OtherDerived)
Cwise<ExpressionType>::operator*(const MatrixBase<OtherDerived> &other) const
{
  return EIGEN_CWISE_PRODUCT_RETURN_TYPE(ExpressionType,OtherDerived)(_expression(), other.derived());
}

/** \deprecated ArrayBase::operator/() */
template<typename ExpressionType>
template<typename OtherDerived>
EIGEN_STRONG_INLINE const EIGEN_CWISE_BINOP_RETURN_TYPE(internal::scalar_quotient_op)
Cwise<ExpressionType>::operator/(const MatrixBase<OtherDerived> &other) const
{
  return EIGEN_CWISE_BINOP_RETURN_TYPE(internal::scalar_quotient_op)(_expression(), other.derived());
}

/** \deprecated ArrayBase::operator*=() */
template<typename ExpressionType>
template<typename OtherDerived>
inline ExpressionType& Cwise<ExpressionType>::operator*=(const MatrixBase<OtherDerived> &other)
{
  return m_matrix.const_cast_derived() = *this * other;
}

/** \deprecated ArrayBase::operator/=() */
template<typename ExpressionType>
template<typename OtherDerived>
inline ExpressionType& Cwise<ExpressionType>::operator/=(const MatrixBase<OtherDerived> &other)
{
  return m_matrix.const_cast_derived() = *this / other;
}

/***************************************************************************
* The following functions were defined in Array
***************************************************************************/

// -- unary operators --

/** \deprecated ArrayBase::sqrt() */
template<typename ExpressionType>
inline const EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_sqrt_op)
Cwise<ExpressionType>::sqrt() const
{
  return _expression();
}

/** \deprecated ArrayBase::cos() */
template<typename ExpressionType>
inline const EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_cos_op)
Cwise<ExpressionType>::cos() const
{
  return _expression();
}


/** \deprecated ArrayBase::sin() */
template<typename ExpressionType>
inline const EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_sin_op)
Cwise<ExpressionType>::sin() const
{
  return _expression();
}


/** \deprecated ArrayBase::log() */
template<typename ExpressionType>
inline const EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_pow_op)
Cwise<ExpressionType>::pow(const Scalar& exponent) const
{
  return EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_pow_op)(_expression(), internal::scalar_pow_op<Scalar>(exponent));
}


/** \deprecated ArrayBase::inverse() */
template<typename ExpressionType>
inline const EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_inverse_op)
Cwise<ExpressionType>::inverse() const
{
  return _expression();
}

/** \deprecated ArrayBase::square() */
template<typename ExpressionType>
inline const EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_square_op)
Cwise<ExpressionType>::square() const
{
  return _expression();
}

/** \deprecated ArrayBase::cube() */
template<typename ExpressionType>
inline const EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_cube_op)
Cwise<ExpressionType>::cube() const
{
  return _expression();
}


// -- binary operators --

/** \deprecated ArrayBase::operator<() */
template<typename ExpressionType>
template<typename OtherDerived>
inline const EIGEN_CWISE_BINOP_RETURN_TYPE(std::less)
Cwise<ExpressionType>::operator<(const MatrixBase<OtherDerived> &other) const
{
  return EIGEN_CWISE_BINOP_RETURN_TYPE(std::less)(_expression(), other.derived());
}

/** \deprecated ArrayBase::<=() */
template<typename ExpressionType>
template<typename OtherDerived>
inline const EIGEN_CWISE_BINOP_RETURN_TYPE(std::less_equal)
Cwise<ExpressionType>::operator<=(const MatrixBase<OtherDerived> &other) const
{
  return EIGEN_CWISE_BINOP_RETURN_TYPE(std::less_equal)(_expression(), other.derived());
}

/** \deprecated ArrayBase::operator>() */
template<typename ExpressionType>
template<typename OtherDerived>
inline const EIGEN_CWISE_BINOP_RETURN_TYPE(std::greater)
Cwise<ExpressionType>::operator>(const MatrixBase<OtherDerived> &other) const
{
  return EIGEN_CWISE_BINOP_RETURN_TYPE(std::greater)(_expression(), other.derived());
}

/** \deprecated ArrayBase::operator>=() */
template<typename ExpressionType>
template<typename OtherDerived>
inline const EIGEN_CWISE_BINOP_RETURN_TYPE(std::greater_equal)
Cwise<ExpressionType>::operator>=(const MatrixBase<OtherDerived> &other) const
{
  return EIGEN_CWISE_BINOP_RETURN_TYPE(std::greater_equal)(_expression(), other.derived());
}

/** \deprecated ArrayBase::operator==() */
template<typename ExpressionType>
template<typename OtherDerived>
inline const EIGEN_CWISE_BINOP_RETURN_TYPE(std::equal_to)
Cwise<ExpressionType>::operator==(const MatrixBase<OtherDerived> &other) const
{
  return EIGEN_CWISE_BINOP_RETURN_TYPE(std::equal_to)(_expression(), other.derived());
}

/** \deprecated ArrayBase::operator!=() */
template<typename ExpressionType>
template<typename OtherDerived>
inline const EIGEN_CWISE_BINOP_RETURN_TYPE(std::not_equal_to)
Cwise<ExpressionType>::operator!=(const MatrixBase<OtherDerived> &other) const
{
  return EIGEN_CWISE_BINOP_RETURN_TYPE(std::not_equal_to)(_expression(), other.derived());
}

// comparisons to scalar value

/** \deprecated ArrayBase::operator<(Scalar) */
template<typename ExpressionType>
inline const EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::less)
Cwise<ExpressionType>::operator<(Scalar s) const
{
  return EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::less)(_expression(),
            typename ExpressionType::ConstantReturnType(_expression().rows(), _expression().cols(), s));
}

/** \deprecated ArrayBase::operator<=(Scalar) */
template<typename ExpressionType>
inline const EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::less_equal)
Cwise<ExpressionType>::operator<=(Scalar s) const
{
  return EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::less_equal)(_expression(),
            typename ExpressionType::ConstantReturnType(_expression().rows(), _expression().cols(), s));
}

/** \deprecated ArrayBase::operator>(Scalar) */
template<typename ExpressionType>
inline const EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::greater)
Cwise<ExpressionType>::operator>(Scalar s) const
{
  return EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::greater)(_expression(),
            typename ExpressionType::ConstantReturnType(_expression().rows(), _expression().cols(), s));
}

/** \deprecated ArrayBase::operator>=(Scalar) */
template<typename ExpressionType>
inline const EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::greater_equal)
Cwise<ExpressionType>::operator>=(Scalar s) const
{
  return EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::greater_equal)(_expression(),
            typename ExpressionType::ConstantReturnType(_expression().rows(), _expression().cols(), s));
}

/** \deprecated ArrayBase::operator==(Scalar) */
template<typename ExpressionType>
inline const EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::equal_to)
Cwise<ExpressionType>::operator==(Scalar s) const
{
  return EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::equal_to)(_expression(),
            typename ExpressionType::ConstantReturnType(_expression().rows(), _expression().cols(), s));
}

/** \deprecated ArrayBase::operator!=(Scalar) */
template<typename ExpressionType>
inline const EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::not_equal_to)
Cwise<ExpressionType>::operator!=(Scalar s) const
{
  return EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::not_equal_to)(_expression(),
            typename ExpressionType::ConstantReturnType(_expression().rows(), _expression().cols(), s));
}

// scalar addition

/** \deprecated ArrayBase::operator+(Scalar) */
template<typename ExpressionType>
inline const typename Cwise<ExpressionType>::ScalarAddReturnType
Cwise<ExpressionType>::operator+(const Scalar& scalar) const
{
  return typename Cwise<ExpressionType>::ScalarAddReturnType(m_matrix, internal::scalar_add_op<Scalar>(scalar));
}

/** \deprecated ArrayBase::operator+=(Scalar) */
template<typename ExpressionType>
inline ExpressionType& Cwise<ExpressionType>::operator+=(const Scalar& scalar)
{
  return m_matrix.const_cast_derived() = *this + scalar;
}

/** \deprecated ArrayBase::operator-(Scalar) */
template<typename ExpressionType>
inline const typename Cwise<ExpressionType>::ScalarAddReturnType
Cwise<ExpressionType>::operator-(const Scalar& scalar) const
{
  return *this + (-scalar);
}

/** \deprecated ArrayBase::operator-=(Scalar) */
template<typename ExpressionType>
inline ExpressionType& Cwise<ExpressionType>::operator-=(const Scalar& scalar)
{
  return m_matrix.const_cast_derived() = *this - scalar;
}

#endif // EIGEN_ARRAY_CWISE_OPERATORS_H
