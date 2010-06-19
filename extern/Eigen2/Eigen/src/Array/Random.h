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

#ifndef EIGEN_RANDOM_H
#define EIGEN_RANDOM_H

template<typename Scalar> struct ei_scalar_random_op EIGEN_EMPTY_STRUCT {
  inline ei_scalar_random_op(void) {}
  inline const Scalar operator() (int, int) const { return ei_random<Scalar>(); }
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_random_op<Scalar> >
{ enum { Cost = 5 * NumTraits<Scalar>::MulCost, PacketAccess = false, IsRepeatable = false }; };

/** \array_module
  * 
  * \returns a random matrix (not an expression, the matrix is immediately evaluated).
  *
  * The parameters \a rows and \a cols are the number of rows and of columns of
  * the returned matrix. Must be compatible with this MatrixBase type.
  *
  * This variant is meant to be used for dynamic-size matrix types. For fixed-size types,
  * it is redundant to pass \a rows and \a cols as arguments, so ei_random() should be used
  * instead.
  *
  * \addexample RandomExample \label How to create a matrix with random coefficients
  *
  * Example: \include MatrixBase_random_int_int.cpp
  * Output: \verbinclude MatrixBase_random_int_int.out
  *
  * \sa MatrixBase::setRandom(), MatrixBase::Random(int), MatrixBase::Random()
  */
template<typename Derived>
inline const CwiseNullaryOp<ei_scalar_random_op<typename ei_traits<Derived>::Scalar>, Derived>
MatrixBase<Derived>::Random(int rows, int cols)
{
  return NullaryExpr(rows, cols, ei_scalar_random_op<Scalar>());
}

/** \array_module
  * 
  * \returns a random vector (not an expression, the vector is immediately evaluated).
  *
  * The parameter \a size is the size of the returned vector.
  * Must be compatible with this MatrixBase type.
  *
  * \only_for_vectors
  *
  * This variant is meant to be used for dynamic-size vector types. For fixed-size types,
  * it is redundant to pass \a size as argument, so ei_random() should be used
  * instead.
  *
  * Example: \include MatrixBase_random_int.cpp
  * Output: \verbinclude MatrixBase_random_int.out
  *
  * \sa MatrixBase::setRandom(), MatrixBase::Random(int,int), MatrixBase::Random()
  */
template<typename Derived>
inline const CwiseNullaryOp<ei_scalar_random_op<typename ei_traits<Derived>::Scalar>, Derived>
MatrixBase<Derived>::Random(int size)
{
  return NullaryExpr(size, ei_scalar_random_op<Scalar>());
}

/** \array_module
  * 
  * \returns a fixed-size random matrix or vector
  * (not an expression, the matrix is immediately evaluated).
  *
  * This variant is only for fixed-size MatrixBase types. For dynamic-size types, you
  * need to use the variants taking size arguments.
  *
  * Example: \include MatrixBase_random.cpp
  * Output: \verbinclude MatrixBase_random.out
  *
  * \sa MatrixBase::setRandom(), MatrixBase::Random(int,int), MatrixBase::Random(int)
  */
template<typename Derived>
inline const CwiseNullaryOp<ei_scalar_random_op<typename ei_traits<Derived>::Scalar>, Derived>
MatrixBase<Derived>::Random()
{
  return NullaryExpr(RowsAtCompileTime, ColsAtCompileTime, ei_scalar_random_op<Scalar>());
}

/** \array_module
  * 
  * Sets all coefficients in this expression to random values.
  *
  * Example: \include MatrixBase_setRandom.cpp
  * Output: \verbinclude MatrixBase_setRandom.out
  *
  * \sa class CwiseNullaryOp, setRandom(int), setRandom(int,int)
  */
template<typename Derived>
inline Derived& MatrixBase<Derived>::setRandom()
{
  return *this = Random(rows(), cols());
}

/** Resizes to the given \a size, and sets all coefficients in this expression to random values.
  *
  * \only_for_vectors
  *
  * Example: \include Matrix_setRandom_int.cpp
  * Output: \verbinclude Matrix_setRandom_int.out
  *
  * \sa MatrixBase::setRandom(), setRandom(int,int), class CwiseNullaryOp, MatrixBase::Random()
  */
template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
EIGEN_STRONG_INLINE Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>&
Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>::setRandom(int size)
{
  resize(size);
  return setRandom();
}

/** Resizes to the given size, and sets all coefficients in this expression to random values.
  *
  * \param rows the new number of rows
  * \param cols the new number of columns
  *
  * Example: \include Matrix_setRandom_int_int.cpp
  * Output: \verbinclude Matrix_setRandom_int_int.out
  *
  * \sa MatrixBase::setRandom(), setRandom(int), class CwiseNullaryOp, MatrixBase::Random()
  */
template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
EIGEN_STRONG_INLINE Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>&
Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>::setRandom(int rows, int cols)
{
  resize(rows, cols);
  return setRandom();
}

#endif // EIGEN_RANDOM_H
