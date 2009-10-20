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

#ifndef EIGEN_COMMAINITIALIZER_H
#define EIGEN_COMMAINITIALIZER_H

/** \class CommaInitializer
  *
  * \brief Helper class used by the comma initializer operator
  *
  * This class is internally used to implement the comma initializer feature. It is
  * the return type of MatrixBase::operator<<, and most of the time this is the only
  * way it is used.
  *
  * \sa \ref MatrixBaseCommaInitRef "MatrixBase::operator<<", CommaInitializer::finished()
  */
template<typename MatrixType>
struct CommaInitializer
{
  typedef typename ei_traits<MatrixType>::Scalar Scalar;
  inline CommaInitializer(MatrixType& mat, const Scalar& s)
    : m_matrix(mat), m_row(0), m_col(1), m_currentBlockRows(1)
  {
    m_matrix.coeffRef(0,0) = s;
  }

  template<typename OtherDerived>
  inline CommaInitializer(MatrixType& mat, const MatrixBase<OtherDerived>& other)
    : m_matrix(mat), m_row(0), m_col(other.cols()), m_currentBlockRows(other.rows())
  {
    m_matrix.block(0, 0, other.rows(), other.cols()) = other;
  }

  /* inserts a scalar value in the target matrix */
  CommaInitializer& operator,(const Scalar& s)
  {
    if (m_col==m_matrix.cols())
    {
      m_row+=m_currentBlockRows;
      m_col = 0;
      m_currentBlockRows = 1;
      ei_assert(m_row<m_matrix.rows()
        && "Too many rows passed to comma initializer (operator<<)");
    }
    ei_assert(m_col<m_matrix.cols()
      && "Too many coefficients passed to comma initializer (operator<<)");
    ei_assert(m_currentBlockRows==1);
    m_matrix.coeffRef(m_row, m_col++) = s;
    return *this;
  }

  /* inserts a matrix expression in the target matrix */
  template<typename OtherDerived>
  CommaInitializer& operator,(const MatrixBase<OtherDerived>& other)
  {
    if (m_col==m_matrix.cols())
    {
      m_row+=m_currentBlockRows;
      m_col = 0;
      m_currentBlockRows = other.rows();
      ei_assert(m_row+m_currentBlockRows<=m_matrix.rows()
        && "Too many rows passed to comma initializer (operator<<)");
    }
    ei_assert(m_col<m_matrix.cols()
      && "Too many coefficients passed to comma initializer (operator<<)");
    ei_assert(m_currentBlockRows==other.rows());
    if (OtherDerived::SizeAtCompileTime != Dynamic)
      m_matrix.template block<OtherDerived::RowsAtCompileTime != Dynamic ? OtherDerived::RowsAtCompileTime : 1,
                              OtherDerived::ColsAtCompileTime != Dynamic ? OtherDerived::ColsAtCompileTime : 1>
                    (m_row, m_col) = other;
    else
      m_matrix.block(m_row, m_col, other.rows(), other.cols()) = other;
    m_col += other.cols();
    return *this;
  }

  inline ~CommaInitializer()
  {
    ei_assert((m_row+m_currentBlockRows) == m_matrix.rows()
         && m_col == m_matrix.cols()
         && "Too few coefficients passed to comma initializer (operator<<)");
  }

  /** \returns the built matrix once all its coefficients have been set.
    * Calling finished is 100% optional. Its purpose is to write expressions
    * like this:
    * \code
    * quaternion.fromRotationMatrix((Matrix3f() << axis0, axis1, axis2).finished());
    * \endcode
    */
  inline MatrixType& finished() { return m_matrix; }

  MatrixType& m_matrix;   // target matrix
  int m_row;              // current row id
  int m_col;              // current col id
  int m_currentBlockRows; // current block height

private:
  CommaInitializer& operator=(const CommaInitializer&);
};

/** \anchor MatrixBaseCommaInitRef
  * Convenient operator to set the coefficients of a matrix.
  *
  * The coefficients must be provided in a row major order and exactly match
  * the size of the matrix. Otherwise an assertion is raised.
  *
  * \addexample CommaInit \label How to easily set all the coefficients of a matrix
  *
  * Example: \include MatrixBase_set.cpp
  * Output: \verbinclude MatrixBase_set.out
  *
  * \sa CommaInitializer::finished(), class CommaInitializer
  */
template<typename Derived>
inline CommaInitializer<Derived> MatrixBase<Derived>::operator<< (const Scalar& s)
{
  return CommaInitializer<Derived>(*static_cast<Derived*>(this), s);
}

/** \sa operator<<(const Scalar&) */
template<typename Derived>
template<typename OtherDerived>
inline CommaInitializer<Derived>
MatrixBase<Derived>::operator<<(const MatrixBase<OtherDerived>& other)
{
  return CommaInitializer<Derived>(*static_cast<Derived *>(this), other);
}

#endif // EIGEN_COMMAINITIALIZER_H
