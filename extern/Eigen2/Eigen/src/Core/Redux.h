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

#ifndef EIGEN_REDUX_H
#define EIGEN_REDUX_H

template<typename BinaryOp, typename Derived, int Start, int Length>
struct ei_redux_impl
{
  enum {
    HalfLength = Length/2
  };

  typedef typename ei_result_of<BinaryOp(typename Derived::Scalar)>::type Scalar;

  static Scalar run(const Derived &mat, const BinaryOp& func)
  {
    return func(
      ei_redux_impl<BinaryOp, Derived, Start, HalfLength>::run(mat, func),
      ei_redux_impl<BinaryOp, Derived, Start+HalfLength, Length - HalfLength>::run(mat, func));
  }
};

template<typename BinaryOp, typename Derived, int Start>
struct ei_redux_impl<BinaryOp, Derived, Start, 1>
{
  enum {
    col = Start / Derived::RowsAtCompileTime,
    row = Start % Derived::RowsAtCompileTime
  };

  typedef typename ei_result_of<BinaryOp(typename Derived::Scalar)>::type Scalar;

  static Scalar run(const Derived &mat, const BinaryOp &)
  {
    return mat.coeff(row, col);
  }
};

template<typename BinaryOp, typename Derived, int Start>
struct ei_redux_impl<BinaryOp, Derived, Start, Dynamic>
{
  typedef typename ei_result_of<BinaryOp(typename Derived::Scalar)>::type Scalar;
  static Scalar run(const Derived& mat, const BinaryOp& func)
  {
    ei_assert(mat.rows()>0 && mat.cols()>0 && "you are using a non initialized matrix");
    Scalar res;
    res = mat.coeff(0,0);
    for(int i = 1; i < mat.rows(); ++i)
      res = func(res, mat.coeff(i, 0));
    for(int j = 1; j < mat.cols(); ++j)
      for(int i = 0; i < mat.rows(); ++i)
        res = func(res, mat.coeff(i, j));
    return res;
  }
};

/** \returns the result of a full redux operation on the whole matrix or vector using \a func
  *
  * The template parameter \a BinaryOp is the type of the functor \a func which must be
  * an assiociative operator. Both current STL and TR1 functor styles are handled.
  *
  * \sa MatrixBase::sum(), MatrixBase::minCoeff(), MatrixBase::maxCoeff(), MatrixBase::colwise(), MatrixBase::rowwise()
  */
template<typename Derived>
template<typename BinaryOp>
typename ei_result_of<BinaryOp(typename ei_traits<Derived>::Scalar)>::type
MatrixBase<Derived>::redux(const BinaryOp& func) const
{
  const bool unroll = SizeAtCompileTime * CoeffReadCost
                    + (SizeAtCompileTime-1) * ei_functor_traits<BinaryOp>::Cost
                    <= EIGEN_UNROLLING_LIMIT;
  return ei_redux_impl<BinaryOp, Derived, 0, unroll ? int(SizeAtCompileTime) : Dynamic>
            ::run(derived(), func);
}

/** \returns the minimum of all coefficients of *this
  */
template<typename Derived>
inline typename ei_traits<Derived>::Scalar
MatrixBase<Derived>::minCoeff() const
{
  return this->redux(Eigen::ei_scalar_min_op<Scalar>());
}

/** \returns the maximum of all coefficients of *this
  */
template<typename Derived>
inline typename ei_traits<Derived>::Scalar
MatrixBase<Derived>::maxCoeff() const
{
  return this->redux(Eigen::ei_scalar_max_op<Scalar>());
}

#endif // EIGEN_REDUX_H
