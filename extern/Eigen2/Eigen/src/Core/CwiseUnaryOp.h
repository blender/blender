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

#ifndef EIGEN_CWISE_UNARY_OP_H
#define EIGEN_CWISE_UNARY_OP_H

/** \class CwiseUnaryOp
  *
  * \brief Generic expression of a coefficient-wise unary operator of a matrix or a vector
  *
  * \param UnaryOp template functor implementing the operator
  * \param MatrixType the type of the matrix we are applying the unary operator
  *
  * This class represents an expression of a generic unary operator of a matrix or a vector.
  * It is the return type of the unary operator-, of a matrix or a vector, and most
  * of the time this is the only way it is used.
  *
  * \sa MatrixBase::unaryExpr(const CustomUnaryOp &) const, class CwiseBinaryOp, class CwiseNullaryOp
  */
template<typename UnaryOp, typename MatrixType>
struct ei_traits<CwiseUnaryOp<UnaryOp, MatrixType> >
 : ei_traits<MatrixType>
{
  typedef typename ei_result_of<
                     UnaryOp(typename MatrixType::Scalar)
                   >::type Scalar;
  typedef typename MatrixType::Nested MatrixTypeNested;
  typedef typename ei_unref<MatrixTypeNested>::type _MatrixTypeNested;
  enum {
    Flags = (_MatrixTypeNested::Flags & (
      HereditaryBits | LinearAccessBit | AlignedBit
      | (ei_functor_traits<UnaryOp>::PacketAccess ? PacketAccessBit : 0))),
    CoeffReadCost = _MatrixTypeNested::CoeffReadCost + ei_functor_traits<UnaryOp>::Cost
  };
};

template<typename UnaryOp, typename MatrixType>
class CwiseUnaryOp : ei_no_assignment_operator,
  public MatrixBase<CwiseUnaryOp<UnaryOp, MatrixType> >
{
  public:

    EIGEN_GENERIC_PUBLIC_INTERFACE(CwiseUnaryOp)

    inline CwiseUnaryOp(const MatrixType& mat, const UnaryOp& func = UnaryOp())
      : m_matrix(mat), m_functor(func) {}

    EIGEN_STRONG_INLINE int rows() const { return m_matrix.rows(); }
    EIGEN_STRONG_INLINE int cols() const { return m_matrix.cols(); }

    EIGEN_STRONG_INLINE const Scalar coeff(int row, int col) const
    {
      return m_functor(m_matrix.coeff(row, col));
    }

    template<int LoadMode>
    EIGEN_STRONG_INLINE PacketScalar packet(int row, int col) const
    {
      return m_functor.packetOp(m_matrix.template packet<LoadMode>(row, col));
    }

    EIGEN_STRONG_INLINE const Scalar coeff(int index) const
    {
      return m_functor(m_matrix.coeff(index));
    }

    template<int LoadMode>
    EIGEN_STRONG_INLINE PacketScalar packet(int index) const
    {
      return m_functor.packetOp(m_matrix.template packet<LoadMode>(index));
    }

  protected:
    const typename MatrixType::Nested m_matrix;
    const UnaryOp m_functor;
};

/** \returns an expression of a custom coefficient-wise unary operator \a func of *this
  *
  * The template parameter \a CustomUnaryOp is the type of the functor
  * of the custom unary operator.
  *
  * \addexample CustomCwiseUnaryFunctors \label How to use custom coeff wise unary functors
  *
  * Example:
  * \include class_CwiseUnaryOp.cpp
  * Output: \verbinclude class_CwiseUnaryOp.out
  *
  * \sa class CwiseUnaryOp, class CwiseBinarOp, MatrixBase::operator-, Cwise::abs
  */
template<typename Derived>
template<typename CustomUnaryOp>
EIGEN_STRONG_INLINE const CwiseUnaryOp<CustomUnaryOp, Derived>
MatrixBase<Derived>::unaryExpr(const CustomUnaryOp& func) const
{
  return CwiseUnaryOp<CustomUnaryOp, Derived>(derived(), func);
}

/** \returns an expression of the opposite of \c *this
  */
template<typename Derived>
EIGEN_STRONG_INLINE const CwiseUnaryOp<ei_scalar_opposite_op<typename ei_traits<Derived>::Scalar>,Derived>
MatrixBase<Derived>::operator-() const
{
  return derived();
}

/** \returns an expression of the coefficient-wise absolute value of \c *this
  *
  * Example: \include Cwise_abs.cpp
  * Output: \verbinclude Cwise_abs.out
  *
  * \sa abs2()
  */
template<typename ExpressionType>
EIGEN_STRONG_INLINE const EIGEN_CWISE_UNOP_RETURN_TYPE(ei_scalar_abs_op)
Cwise<ExpressionType>::abs() const
{
  return _expression();
}

/** \returns an expression of the coefficient-wise squared absolute value of \c *this
  *
  * Example: \include Cwise_abs2.cpp
  * Output: \verbinclude Cwise_abs2.out
  *
  * \sa abs(), square()
  */
template<typename ExpressionType>
EIGEN_STRONG_INLINE const EIGEN_CWISE_UNOP_RETURN_TYPE(ei_scalar_abs2_op)
Cwise<ExpressionType>::abs2() const
{
  return _expression();
}

/** \returns an expression of the complex conjugate of \c *this.
  *
  * \sa adjoint() */
template<typename Derived>
EIGEN_STRONG_INLINE typename MatrixBase<Derived>::ConjugateReturnType
MatrixBase<Derived>::conjugate() const
{
  return ConjugateReturnType(derived());
}

/** \returns an expression of the real part of \c *this.
  *
  * \sa imag() */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::RealReturnType
MatrixBase<Derived>::real() const { return derived(); }

/** \returns an expression of the imaginary part of \c *this.
  *
  * \sa real() */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::ImagReturnType
MatrixBase<Derived>::imag() const { return derived(); }

/** \returns an expression of *this with the \a Scalar type casted to
  * \a NewScalar.
  *
  * The template parameter \a NewScalar is the type we are casting the scalars to.
  *
  * \sa class CwiseUnaryOp
  */
template<typename Derived>
template<typename NewType>
EIGEN_STRONG_INLINE const CwiseUnaryOp<ei_scalar_cast_op<typename ei_traits<Derived>::Scalar, NewType>, Derived>
MatrixBase<Derived>::cast() const
{
  return derived();
}

/** \relates MatrixBase */
template<typename Derived>
EIGEN_STRONG_INLINE const typename MatrixBase<Derived>::ScalarMultipleReturnType
MatrixBase<Derived>::operator*(const Scalar& scalar) const
{
  return CwiseUnaryOp<ei_scalar_multiple_op<Scalar>, Derived>
    (derived(), ei_scalar_multiple_op<Scalar>(scalar));
}

/** \relates MatrixBase */
template<typename Derived>
EIGEN_STRONG_INLINE const CwiseUnaryOp<ei_scalar_quotient1_op<typename ei_traits<Derived>::Scalar>, Derived>
MatrixBase<Derived>::operator/(const Scalar& scalar) const
{
  return CwiseUnaryOp<ei_scalar_quotient1_op<Scalar>, Derived>
    (derived(), ei_scalar_quotient1_op<Scalar>(scalar));
}

template<typename Derived>
EIGEN_STRONG_INLINE Derived&
MatrixBase<Derived>::operator*=(const Scalar& other)
{
  return *this = *this * other;
}

template<typename Derived>
EIGEN_STRONG_INLINE Derived&
MatrixBase<Derived>::operator/=(const Scalar& other)
{
  return *this = *this / other;
}

#endif // EIGEN_CWISE_UNARY_OP_H
