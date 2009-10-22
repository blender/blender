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

#ifndef EIGEN_SPARSETRIANGULARSOLVER_H
#define EIGEN_SPARSETRIANGULARSOLVER_H

// forward substitution, row-major
template<typename Lhs, typename Rhs>
struct ei_solve_triangular_selector<Lhs,Rhs,LowerTriangular,RowMajor|IsSparse>
{
  typedef typename Rhs::Scalar Scalar;
  static void run(const Lhs& lhs, Rhs& other)
  {
    for(int col=0 ; col<other.cols() ; ++col)
    {
      for(int i=0; i<lhs.rows(); ++i)
      {
        Scalar tmp = other.coeff(i,col);
        Scalar lastVal = 0;
        int lastIndex = 0;
        for(typename Lhs::InnerIterator it(lhs, i); it; ++it)
        {
          lastVal = it.value();
          lastIndex = it.index();
          tmp -= lastVal * other.coeff(lastIndex,col);
        }
        if (Lhs::Flags & UnitDiagBit)
          other.coeffRef(i,col) = tmp;
        else
        {
          ei_assert(lastIndex==i);
          other.coeffRef(i,col) = tmp/lastVal;
        }
      }
    }
  }
};

// backward substitution, row-major
template<typename Lhs, typename Rhs>
struct ei_solve_triangular_selector<Lhs,Rhs,UpperTriangular,RowMajor|IsSparse>
{
  typedef typename Rhs::Scalar Scalar;
  static void run(const Lhs& lhs, Rhs& other)
  {
    for(int col=0 ; col<other.cols() ; ++col)
    {
      for(int i=lhs.rows()-1 ; i>=0 ; --i)
      {
        Scalar tmp = other.coeff(i,col);
        typename Lhs::InnerIterator it(lhs, i);
        if (it.index() == i)
          ++it;
        for(; it; ++it)
        {
          tmp -= it.value() * other.coeff(it.index(),col);
        }

        if (Lhs::Flags & UnitDiagBit)
          other.coeffRef(i,col) = tmp;
        else
        {
          typename Lhs::InnerIterator it(lhs, i);
          ei_assert(it.index() == i);
          other.coeffRef(i,col) = tmp/it.value();
        }
      }
    }
  }
};

// forward substitution, col-major
template<typename Lhs, typename Rhs>
struct ei_solve_triangular_selector<Lhs,Rhs,LowerTriangular,ColMajor|IsSparse>
{
  typedef typename Rhs::Scalar Scalar;
  static void run(const Lhs& lhs, Rhs& other)
  {
    for(int col=0 ; col<other.cols() ; ++col)
    {
      for(int i=0; i<lhs.cols(); ++i)
      {
        typename Lhs::InnerIterator it(lhs, i);
        if(!(Lhs::Flags & UnitDiagBit))
        {
          // std::cerr << it.value() << " ; " << it.index() << " == " << i << "\n";
          ei_assert(it.index()==i);
          other.coeffRef(i,col) /= it.value();
        }
        Scalar tmp = other.coeffRef(i,col);
        if (it.index()==i)
          ++it;
        for(; it; ++it)
          other.coeffRef(it.index(), col) -= tmp * it.value();
      }
    }
  }
};

// backward substitution, col-major
template<typename Lhs, typename Rhs>
struct ei_solve_triangular_selector<Lhs,Rhs,UpperTriangular,ColMajor|IsSparse>
{
  typedef typename Rhs::Scalar Scalar;
  static void run(const Lhs& lhs, Rhs& other)
  {
    for(int col=0 ; col<other.cols() ; ++col)
    {
      for(int i=lhs.cols()-1; i>=0; --i)
      {
        if(!(Lhs::Flags & UnitDiagBit))
        {
          // FIXME lhs.coeff(i,i) might not be always efficient while it must simply be the
          // last element of the column !
          other.coeffRef(i,col) /= lhs.coeff(i,i);
        }
        Scalar tmp = other.coeffRef(i,col);
        typename Lhs::InnerIterator it(lhs, i);
        for(; it && it.index()<i; ++it)
          other.coeffRef(it.index(), col) -= tmp * it.value();
      }
    }
  }
};

template<typename Derived>
template<typename OtherDerived>
void SparseMatrixBase<Derived>::solveTriangularInPlace(MatrixBase<OtherDerived>& other) const
{
  ei_assert(derived().cols() == derived().rows());
  ei_assert(derived().cols() == other.rows());
  ei_assert(!(Flags & ZeroDiagBit));
  ei_assert(Flags & (UpperTriangularBit|LowerTriangularBit));

  enum { copy = ei_traits<OtherDerived>::Flags & RowMajorBit };

  typedef typename ei_meta_if<copy,
    typename ei_plain_matrix_type_column_major<OtherDerived>::type, OtherDerived&>::ret OtherCopy;
  OtherCopy otherCopy(other.derived());

  ei_solve_triangular_selector<Derived, typename ei_unref<OtherCopy>::type>::run(derived(), otherCopy);

  if (copy)
    other = otherCopy;
}

template<typename Derived>
template<typename OtherDerived>
typename ei_plain_matrix_type_column_major<OtherDerived>::type
SparseMatrixBase<Derived>::solveTriangular(const MatrixBase<OtherDerived>& other) const
{
  typename ei_plain_matrix_type_column_major<OtherDerived>::type res(other);
  solveTriangularInPlace(res);
  return res;
}

#endif // EIGEN_SPARSETRIANGULARSOLVER_H
