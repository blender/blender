// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
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

#ifndef EIGEN_MINOR_H
#define EIGEN_MINOR_H

/** \nonstableyet 
  * \class Minor
  *
  * \brief Expression of a minor
  *
  * \param MatrixType the type of the object in which we are taking a minor
  *
  * This class represents an expression of a minor. It is the return
  * type of MatrixBase::minor() and most of the time this is the only way it
  * is used.
  *
  * \sa MatrixBase::minor()
  */
template<typename MatrixType>
struct ei_traits<Minor<MatrixType> >
{
  typedef typename MatrixType::Scalar Scalar;
  typedef typename ei_nested<MatrixType>::type MatrixTypeNested;
  typedef typename ei_unref<MatrixTypeNested>::type _MatrixTypeNested;
  enum {
    RowsAtCompileTime = (MatrixType::RowsAtCompileTime != Dynamic) ?
                          int(MatrixType::RowsAtCompileTime) - 1 : Dynamic,
    ColsAtCompileTime = (MatrixType::ColsAtCompileTime != Dynamic) ?
                          int(MatrixType::ColsAtCompileTime) - 1 : Dynamic,
    MaxRowsAtCompileTime = (MatrixType::MaxRowsAtCompileTime != Dynamic) ?
                             int(MatrixType::MaxRowsAtCompileTime) - 1 : Dynamic,
    MaxColsAtCompileTime = (MatrixType::MaxColsAtCompileTime != Dynamic) ?
                             int(MatrixType::MaxColsAtCompileTime) - 1 : Dynamic,
    Flags = _MatrixTypeNested::Flags & HereditaryBits,
    CoeffReadCost = _MatrixTypeNested::CoeffReadCost
  };
};

template<typename MatrixType> class Minor
  : public MatrixBase<Minor<MatrixType> >
{
  public:

    EIGEN_GENERIC_PUBLIC_INTERFACE(Minor)

    inline Minor(const MatrixType& matrix,
                       int row, int col)
      : m_matrix(matrix), m_row(row), m_col(col)
    {
      ei_assert(row >= 0 && row < matrix.rows()
          && col >= 0 && col < matrix.cols());
    }

    EIGEN_INHERIT_ASSIGNMENT_OPERATORS(Minor)

    inline int rows() const { return m_matrix.rows() - 1; }
    inline int cols() const { return m_matrix.cols() - 1; }

    inline Scalar& coeffRef(int row, int col)
    {
      return m_matrix.const_cast_derived().coeffRef(row + (row >= m_row), col + (col >= m_col));
    }

    inline const Scalar coeff(int row, int col) const
    {
      return m_matrix.coeff(row + (row >= m_row), col + (col >= m_col));
    }

  protected:
    const typename MatrixType::Nested m_matrix;
    const int m_row, m_col;
};

/** \nonstableyet 
  * \return an expression of the (\a row, \a col)-minor of *this,
  * i.e. an expression constructed from *this by removing the specified
  * row and column.
  *
  * Example: \include MatrixBase_minor.cpp
  * Output: \verbinclude MatrixBase_minor.out
  *
  * \sa class Minor
  */
template<typename Derived>
inline Minor<Derived>
MatrixBase<Derived>::minor(int row, int col)
{
  return Minor<Derived>(derived(), row, col);
}

/** \nonstableyet 
  * This is the const version of minor(). */
template<typename Derived>
inline const Minor<Derived>
MatrixBase<Derived>::minor(int row, int col) const
{
  return Minor<Derived>(derived(), row, col);
}

#endif // EIGEN_MINOR_H
