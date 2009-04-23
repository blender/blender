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

#ifndef EIGEN_VISITOR_H
#define EIGEN_VISITOR_H

template<typename Visitor, typename Derived, int UnrollCount>
struct ei_visitor_impl
{
  enum {
    col = (UnrollCount-1) / Derived::RowsAtCompileTime,
    row = (UnrollCount-1) % Derived::RowsAtCompileTime
  };

  inline static void run(const Derived &mat, Visitor& visitor)
  {
    ei_visitor_impl<Visitor, Derived, UnrollCount-1>::run(mat, visitor);
    visitor(mat.coeff(row, col), row, col);
  }
};

template<typename Visitor, typename Derived>
struct ei_visitor_impl<Visitor, Derived, 1>
{
  inline static void run(const Derived &mat, Visitor& visitor)
  {
    return visitor.init(mat.coeff(0, 0), 0, 0);
  }
};

template<typename Visitor, typename Derived>
struct ei_visitor_impl<Visitor, Derived, Dynamic>
{
  inline static void run(const Derived& mat, Visitor& visitor)
  {
    visitor.init(mat.coeff(0,0), 0, 0);
    for(int i = 1; i < mat.rows(); ++i)
      visitor(mat.coeff(i, 0), i, 0);
    for(int j = 1; j < mat.cols(); ++j)
      for(int i = 0; i < mat.rows(); ++i)
        visitor(mat.coeff(i, j), i, j);
  }
};


/** Applies the visitor \a visitor to the whole coefficients of the matrix or vector.
  *
  * The template parameter \a Visitor is the type of the visitor and provides the following interface:
  * \code
  * struct MyVisitor {
  *   // called for the first coefficient
  *   void init(const Scalar& value, int i, int j);
  *   // called for all other coefficients
  *   void operator() (const Scalar& value, int i, int j);
  * };
  * \endcode
  *
  * \note compared to one or two \em for \em loops, visitors offer automatic
  * unrolling for small fixed size matrix.
  *
  * \sa minCoeff(int*,int*), maxCoeff(int*,int*), MatrixBase::redux()
  */
template<typename Derived>
template<typename Visitor>
void MatrixBase<Derived>::visit(Visitor& visitor) const
{
  const bool unroll = SizeAtCompileTime * CoeffReadCost
                    + (SizeAtCompileTime-1) * ei_functor_traits<Visitor>::Cost
                    <= EIGEN_UNROLLING_LIMIT;
  return ei_visitor_impl<Visitor, Derived,
      unroll ? int(SizeAtCompileTime) : Dynamic
    >::run(derived(), visitor);
}

/** \internal
  * \brief Base class to implement min and max visitors
  */
template <typename Scalar>
struct ei_coeff_visitor
{
  int row, col;
  Scalar res;
  inline void init(const Scalar& value, int i, int j)
  {
    res = value;
    row = i;
    col = j;
  }
};

/** \internal
  * \brief Visitor computing the min coefficient with its value and coordinates
  *
  * \sa MatrixBase::minCoeff(int*, int*)
  */
template <typename Scalar>
struct ei_min_coeff_visitor : ei_coeff_visitor<Scalar>
{
  void operator() (const Scalar& value, int i, int j)
  {
    if(value < this->res)
    {
      this->res = value;
      this->row = i;
      this->col = j;
    }
  }
};

template<typename Scalar>
struct ei_functor_traits<ei_min_coeff_visitor<Scalar> > {
  enum {
    Cost = NumTraits<Scalar>::AddCost
  };
};

/** \internal
  * \brief Visitor computing the max coefficient with its value and coordinates
  *
  * \sa MatrixBase::maxCoeff(int*, int*)
  */
template <typename Scalar>
struct ei_max_coeff_visitor : ei_coeff_visitor<Scalar>
{
  void operator() (const Scalar& value, int i, int j)
  {
    if(value > this->res)
    {
      this->res = value;
      this->row = i;
      this->col = j;
    }
  }
};

template<typename Scalar>
struct ei_functor_traits<ei_max_coeff_visitor<Scalar> > {
  enum {
    Cost = NumTraits<Scalar>::AddCost
  };
};

/** \returns the minimum of all coefficients of *this
  * and puts in *row and *col its location.
  *
  * \sa MatrixBase::maxCoeff(int*,int*), MatrixBase::visitor(), MatrixBase::minCoeff()
  */
template<typename Derived>
typename ei_traits<Derived>::Scalar
MatrixBase<Derived>::minCoeff(int* row, int* col) const
{
  ei_min_coeff_visitor<Scalar> minVisitor;
  this->visit(minVisitor);
  *row = minVisitor.row;
  if (col) *col = minVisitor.col;
  return minVisitor.res;
}

/** \returns the maximum of all coefficients of *this
  * and puts in *row and *col its location.
  *
  * \sa MatrixBase::minCoeff(int*,int*), MatrixBase::visitor(), MatrixBase::maxCoeff()
  */
template<typename Derived>
typename ei_traits<Derived>::Scalar
MatrixBase<Derived>::maxCoeff(int* row, int* col) const
{
  ei_max_coeff_visitor<Scalar> maxVisitor;
  this->visit(maxVisitor);
  *row = maxVisitor.row;
  if (col) *col = maxVisitor.col;
  return maxVisitor.res;
}


#endif // EIGEN_VISITOR_H
