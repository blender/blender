// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
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

#ifndef EIGEN_BLOCK2_H
#define EIGEN_BLOCK2_H

/** \returns a dynamic-size expression of a corner of *this.
  *
  * \param type the type of corner. Can be \a Eigen::TopLeft, \a Eigen::TopRight,
  * \a Eigen::BottomLeft, \a Eigen::BottomRight.
  * \param cRows the number of rows in the corner
  * \param cCols the number of columns in the corner
  *
  * Example: \include MatrixBase_corner_enum_int_int.cpp
  * Output: \verbinclude MatrixBase_corner_enum_int_int.out
  *
  * \note Even though the returned expression has dynamic size, in the case
  * when it is applied to a fixed-size matrix, it inherits a fixed maximal size,
  * which means that evaluating it does not cause a dynamic memory allocation.
  *
  * \sa class Block, block(Index,Index,Index,Index)
  */
template<typename Derived>
inline Block<Derived> DenseBase<Derived>
  ::corner(CornerType type, Index cRows, Index cCols)
{
  switch(type)
  {
    default:
      eigen_assert(false && "Bad corner type.");
    case TopLeft:
      return Block<Derived>(derived(), 0, 0, cRows, cCols);
    case TopRight:
      return Block<Derived>(derived(), 0, cols() - cCols, cRows, cCols);
    case BottomLeft:
      return Block<Derived>(derived(), rows() - cRows, 0, cRows, cCols);
    case BottomRight:
      return Block<Derived>(derived(), rows() - cRows, cols() - cCols, cRows, cCols);
  }
}

/** This is the const version of corner(CornerType, Index, Index).*/
template<typename Derived>
inline const Block<Derived>
DenseBase<Derived>::corner(CornerType type, Index cRows, Index cCols) const
{
  switch(type)
  {
    default:
      eigen_assert(false && "Bad corner type.");
    case TopLeft:
      return Block<Derived>(derived(), 0, 0, cRows, cCols);
    case TopRight:
      return Block<Derived>(derived(), 0, cols() - cCols, cRows, cCols);
    case BottomLeft:
      return Block<Derived>(derived(), rows() - cRows, 0, cRows, cCols);
    case BottomRight:
      return Block<Derived>(derived(), rows() - cRows, cols() - cCols, cRows, cCols);
  }
}

/** \returns a fixed-size expression of a corner of *this.
  *
  * \param type the type of corner. Can be \a Eigen::TopLeft, \a Eigen::TopRight,
  * \a Eigen::BottomLeft, \a Eigen::BottomRight.
  *
  * The template parameters CRows and CCols arethe number of rows and columns in the corner.
  *
  * Example: \include MatrixBase_template_int_int_corner_enum.cpp
  * Output: \verbinclude MatrixBase_template_int_int_corner_enum.out
  *
  * \sa class Block, block(Index,Index,Index,Index)
  */
template<typename Derived>
template<int CRows, int CCols>
inline Block<Derived, CRows, CCols>
DenseBase<Derived>::corner(CornerType type)
{
  switch(type)
  {
    default:
      eigen_assert(false && "Bad corner type.");
    case TopLeft:
      return Block<Derived, CRows, CCols>(derived(), 0, 0);
    case TopRight:
      return Block<Derived, CRows, CCols>(derived(), 0, cols() - CCols);
    case BottomLeft:
      return Block<Derived, CRows, CCols>(derived(), rows() - CRows, 0);
    case BottomRight:
      return Block<Derived, CRows, CCols>(derived(), rows() - CRows, cols() - CCols);
  }
}

/** This is the const version of corner<int, int>(CornerType).*/
template<typename Derived>
template<int CRows, int CCols>
inline const Block<Derived, CRows, CCols>
DenseBase<Derived>::corner(CornerType type) const
{
  switch(type)
  {
    default:
      eigen_assert(false && "Bad corner type.");
    case TopLeft:
      return Block<Derived, CRows, CCols>(derived(), 0, 0);
    case TopRight:
      return Block<Derived, CRows, CCols>(derived(), 0, cols() - CCols);
    case BottomLeft:
      return Block<Derived, CRows, CCols>(derived(), rows() - CRows, 0);
    case BottomRight:
      return Block<Derived, CRows, CCols>(derived(), rows() - CRows, cols() - CCols);
  }
}

#endif // EIGEN_BLOCK2_H
