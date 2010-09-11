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

#ifndef EIGEN_COREITERATORS_H
#define EIGEN_COREITERATORS_H

/* This file contains the respective InnerIterator definition of the expressions defined in Eigen/Core
 */

/** \class InnerIterator
  * \brief An InnerIterator allows to loop over the element of a sparse (or dense) matrix or expression
  *
  * todo
  */

// generic version for dense matrix and expressions
template<typename Derived> class MatrixBase<Derived>::InnerIterator
{
    typedef typename Derived::Scalar Scalar;
    enum { IsRowMajor = (Derived::Flags&RowMajorBit)==RowMajorBit };
  public:
    EIGEN_STRONG_INLINE InnerIterator(const Derived& expr, int outer)
      : m_expression(expr), m_inner(0), m_outer(outer), m_end(expr.rows())
    {}

    EIGEN_STRONG_INLINE Scalar value() const
    {
      return (IsRowMajor) ? m_expression.coeff(m_outer, m_inner)
                          : m_expression.coeff(m_inner, m_outer);
    }

    EIGEN_STRONG_INLINE InnerIterator& operator++() { m_inner++; return *this; }

    EIGEN_STRONG_INLINE int index() const { return m_inner; }
    inline int row() const { return IsRowMajor ? m_outer : index(); }
    inline int col() const { return IsRowMajor ? index() : m_outer; }

    EIGEN_STRONG_INLINE operator bool() const { return m_inner < m_end && m_inner>=0; }

  protected:
    const Derived& m_expression;
    int m_inner;
    const int m_outer;
    const int m_end;
};

#endif // EIGEN_COREITERATORS_H
