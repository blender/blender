// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2006-2010 Benoit Jacob <jacob.benoit.1@gmail.com>
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

#ifndef EIGEN_BLOCKMETHODS_H
#define EIGEN_BLOCKMETHODS_H

#ifndef EIGEN_PARSED_BY_DOXYGEN

/** \internal expression type of a column */
typedef Block<Derived, internal::traits<Derived>::RowsAtCompileTime, 1, !IsRowMajor> ColXpr;
typedef const Block<const Derived, internal::traits<Derived>::RowsAtCompileTime, 1, !IsRowMajor> ConstColXpr;
/** \internal expression type of a row */
typedef Block<Derived, 1, internal::traits<Derived>::ColsAtCompileTime, IsRowMajor> RowXpr;
typedef const Block<const Derived, 1, internal::traits<Derived>::ColsAtCompileTime, IsRowMajor> ConstRowXpr;
/** \internal expression type of a block of whole columns */
typedef Block<Derived, internal::traits<Derived>::RowsAtCompileTime, Dynamic, !IsRowMajor> ColsBlockXpr;
typedef const Block<const Derived, internal::traits<Derived>::RowsAtCompileTime, Dynamic, !IsRowMajor> ConstColsBlockXpr;
/** \internal expression type of a block of whole rows */
typedef Block<Derived, Dynamic, internal::traits<Derived>::ColsAtCompileTime, IsRowMajor> RowsBlockXpr;
typedef const Block<const Derived, Dynamic, internal::traits<Derived>::ColsAtCompileTime, IsRowMajor> ConstRowsBlockXpr;
/** \internal expression type of a block of whole columns */
template<int N> struct NColsBlockXpr { typedef Block<Derived, internal::traits<Derived>::RowsAtCompileTime, N, !IsRowMajor> Type; };
template<int N> struct ConstNColsBlockXpr { typedef const Block<const Derived, internal::traits<Derived>::RowsAtCompileTime, N, !IsRowMajor> Type; };
/** \internal expression type of a block of whole rows */
template<int N> struct NRowsBlockXpr { typedef Block<Derived, N, internal::traits<Derived>::ColsAtCompileTime, IsRowMajor> Type; };
template<int N> struct ConstNRowsBlockXpr { typedef const Block<const Derived, N, internal::traits<Derived>::ColsAtCompileTime, IsRowMajor> Type; };


#endif // not EIGEN_PARSED_BY_DOXYGEN

/** \returns a dynamic-size expression of a block in *this.
  *
  * \param startRow the first row in the block
  * \param startCol the first column in the block
  * \param blockRows the number of rows in the block
  * \param blockCols the number of columns in the block
  *
  * Example: \include MatrixBase_block_int_int_int_int.cpp
  * Output: \verbinclude MatrixBase_block_int_int_int_int.out
  *
  * \note Even though the returned expression has dynamic size, in the case
  * when it is applied to a fixed-size matrix, it inherits a fixed maximal size,
  * which means that evaluating it does not cause a dynamic memory allocation.
  *
  * \sa class Block, block(Index,Index)
  */
inline Block<Derived> block(Index startRow, Index startCol, Index blockRows, Index blockCols)
{
  return Block<Derived>(derived(), startRow, startCol, blockRows, blockCols);
}

/** This is the const version of block(Index,Index,Index,Index). */
inline const Block<const Derived> block(Index startRow, Index startCol, Index blockRows, Index blockCols) const
{
  return Block<const Derived>(derived(), startRow, startCol, blockRows, blockCols);
}




/** \returns a dynamic-size expression of a top-right corner of *this.
  *
  * \param cRows the number of rows in the corner
  * \param cCols the number of columns in the corner
  *
  * Example: \include MatrixBase_topRightCorner_int_int.cpp
  * Output: \verbinclude MatrixBase_topRightCorner_int_int.out
  *
  * \sa class Block, block(Index,Index,Index,Index)
  */
inline Block<Derived> topRightCorner(Index cRows, Index cCols)
{
  return Block<Derived>(derived(), 0, cols() - cCols, cRows, cCols);
}

/** This is the const version of topRightCorner(Index, Index).*/
inline const Block<const Derived> topRightCorner(Index cRows, Index cCols) const
{
  return Block<const Derived>(derived(), 0, cols() - cCols, cRows, cCols);
}

/** \returns an expression of a fixed-size top-right corner of *this.
  *
  * The template parameters CRows and CCols are the number of rows and columns in the corner.
  *
  * Example: \include MatrixBase_template_int_int_topRightCorner.cpp
  * Output: \verbinclude MatrixBase_template_int_int_topRightCorner.out
  *
  * \sa class Block, block(Index,Index,Index,Index)
  */
template<int CRows, int CCols>
inline Block<Derived, CRows, CCols> topRightCorner()
{
  return Block<Derived, CRows, CCols>(derived(), 0, cols() - CCols);
}

/** This is the const version of topRightCorner<int, int>().*/
template<int CRows, int CCols>
inline const Block<const Derived, CRows, CCols> topRightCorner() const
{
  return Block<const Derived, CRows, CCols>(derived(), 0, cols() - CCols);
}




/** \returns a dynamic-size expression of a top-left corner of *this.
  *
  * \param cRows the number of rows in the corner
  * \param cCols the number of columns in the corner
  *
  * Example: \include MatrixBase_topLeftCorner_int_int.cpp
  * Output: \verbinclude MatrixBase_topLeftCorner_int_int.out
  *
  * \sa class Block, block(Index,Index,Index,Index)
  */
inline Block<Derived> topLeftCorner(Index cRows, Index cCols)
{
  return Block<Derived>(derived(), 0, 0, cRows, cCols);
}

/** This is the const version of topLeftCorner(Index, Index).*/
inline const Block<const Derived> topLeftCorner(Index cRows, Index cCols) const
{
  return Block<const Derived>(derived(), 0, 0, cRows, cCols);
}

/** \returns an expression of a fixed-size top-left corner of *this.
  *
  * The template parameters CRows and CCols are the number of rows and columns in the corner.
  *
  * Example: \include MatrixBase_template_int_int_topLeftCorner.cpp
  * Output: \verbinclude MatrixBase_template_int_int_topLeftCorner.out
  *
  * \sa class Block, block(Index,Index,Index,Index)
  */
template<int CRows, int CCols>
inline Block<Derived, CRows, CCols> topLeftCorner()
{
  return Block<Derived, CRows, CCols>(derived(), 0, 0);
}

/** This is the const version of topLeftCorner<int, int>().*/
template<int CRows, int CCols>
inline const Block<const Derived, CRows, CCols> topLeftCorner() const
{
  return Block<const Derived, CRows, CCols>(derived(), 0, 0);
}



/** \returns a dynamic-size expression of a bottom-right corner of *this.
  *
  * \param cRows the number of rows in the corner
  * \param cCols the number of columns in the corner
  *
  * Example: \include MatrixBase_bottomRightCorner_int_int.cpp
  * Output: \verbinclude MatrixBase_bottomRightCorner_int_int.out
  *
  * \sa class Block, block(Index,Index,Index,Index)
  */
inline Block<Derived> bottomRightCorner(Index cRows, Index cCols)
{
  return Block<Derived>(derived(), rows() - cRows, cols() - cCols, cRows, cCols);
}

/** This is the const version of bottomRightCorner(Index, Index).*/
inline const Block<const Derived> bottomRightCorner(Index cRows, Index cCols) const
{
  return Block<const Derived>(derived(), rows() - cRows, cols() - cCols, cRows, cCols);
}

/** \returns an expression of a fixed-size bottom-right corner of *this.
  *
  * The template parameters CRows and CCols are the number of rows and columns in the corner.
  *
  * Example: \include MatrixBase_template_int_int_bottomRightCorner.cpp
  * Output: \verbinclude MatrixBase_template_int_int_bottomRightCorner.out
  *
  * \sa class Block, block(Index,Index,Index,Index)
  */
template<int CRows, int CCols>
inline Block<Derived, CRows, CCols> bottomRightCorner()
{
  return Block<Derived, CRows, CCols>(derived(), rows() - CRows, cols() - CCols);
}

/** This is the const version of bottomRightCorner<int, int>().*/
template<int CRows, int CCols>
inline const Block<const Derived, CRows, CCols> bottomRightCorner() const
{
  return Block<const Derived, CRows, CCols>(derived(), rows() - CRows, cols() - CCols);
}



/** \returns a dynamic-size expression of a bottom-left corner of *this.
  *
  * \param cRows the number of rows in the corner
  * \param cCols the number of columns in the corner
  *
  * Example: \include MatrixBase_bottomLeftCorner_int_int.cpp
  * Output: \verbinclude MatrixBase_bottomLeftCorner_int_int.out
  *
  * \sa class Block, block(Index,Index,Index,Index)
  */
inline Block<Derived> bottomLeftCorner(Index cRows, Index cCols)
{
  return Block<Derived>(derived(), rows() - cRows, 0, cRows, cCols);
}

/** This is the const version of bottomLeftCorner(Index, Index).*/
inline const Block<const Derived> bottomLeftCorner(Index cRows, Index cCols) const
{
  return Block<const Derived>(derived(), rows() - cRows, 0, cRows, cCols);
}

/** \returns an expression of a fixed-size bottom-left corner of *this.
  *
  * The template parameters CRows and CCols are the number of rows and columns in the corner.
  *
  * Example: \include MatrixBase_template_int_int_bottomLeftCorner.cpp
  * Output: \verbinclude MatrixBase_template_int_int_bottomLeftCorner.out
  *
  * \sa class Block, block(Index,Index,Index,Index)
  */
template<int CRows, int CCols>
inline Block<Derived, CRows, CCols> bottomLeftCorner()
{
  return Block<Derived, CRows, CCols>(derived(), rows() - CRows, 0);
}

/** This is the const version of bottomLeftCorner<int, int>().*/
template<int CRows, int CCols>
inline const Block<const Derived, CRows, CCols> bottomLeftCorner() const
{
  return Block<const Derived, CRows, CCols>(derived(), rows() - CRows, 0);
}



/** \returns a block consisting of the top rows of *this.
  *
  * \param n the number of rows in the block
  *
  * Example: \include MatrixBase_topRows_int.cpp
  * Output: \verbinclude MatrixBase_topRows_int.out
  *
  * \sa class Block, block(Index,Index,Index,Index)
  */
inline RowsBlockXpr topRows(Index n)
{
  return RowsBlockXpr(derived(), 0, 0, n, cols());
}

/** This is the const version of topRows(Index).*/
inline ConstRowsBlockXpr topRows(Index n) const
{
  return ConstRowsBlockXpr(derived(), 0, 0, n, cols());
}

/** \returns a block consisting of the top rows of *this.
  *
  * \tparam N the number of rows in the block
  *
  * Example: \include MatrixBase_template_int_topRows.cpp
  * Output: \verbinclude MatrixBase_template_int_topRows.out
  *
  * \sa class Block, block(Index,Index,Index,Index)
  */
template<int N>
inline typename NRowsBlockXpr<N>::Type topRows()
{
  return typename NRowsBlockXpr<N>::Type(derived(), 0, 0, N, cols());
}

/** This is the const version of topRows<int>().*/
template<int N>
inline typename ConstNRowsBlockXpr<N>::Type topRows() const
{
  return typename ConstNRowsBlockXpr<N>::Type(derived(), 0, 0, N, cols());
}



/** \returns a block consisting of the bottom rows of *this.
  *
  * \param n the number of rows in the block
  *
  * Example: \include MatrixBase_bottomRows_int.cpp
  * Output: \verbinclude MatrixBase_bottomRows_int.out
  *
  * \sa class Block, block(Index,Index,Index,Index)
  */
inline RowsBlockXpr bottomRows(Index n)
{
  return RowsBlockXpr(derived(), rows() - n, 0, n, cols());
}

/** This is the const version of bottomRows(Index).*/
inline ConstRowsBlockXpr bottomRows(Index n) const
{
  return ConstRowsBlockXpr(derived(), rows() - n, 0, n, cols());
}

/** \returns a block consisting of the bottom rows of *this.
  *
  * \tparam N the number of rows in the block
  *
  * Example: \include MatrixBase_template_int_bottomRows.cpp
  * Output: \verbinclude MatrixBase_template_int_bottomRows.out
  *
  * \sa class Block, block(Index,Index,Index,Index)
  */
template<int N>
inline typename NRowsBlockXpr<N>::Type bottomRows()
{
  return typename NRowsBlockXpr<N>::Type(derived(), rows() - N, 0, N, cols());
}

/** This is the const version of bottomRows<int>().*/
template<int N>
inline typename ConstNRowsBlockXpr<N>::Type bottomRows() const
{
  return typename ConstNRowsBlockXpr<N>::Type(derived(), rows() - N, 0, N, cols());
}



/** \returns a block consisting of a range of rows of *this.
  *
  * \param startRow the index of the first row in the block
  * \param numRows the number of rows in the block
  *
  * Example: \include DenseBase_middleRows_int.cpp
  * Output: \verbinclude DenseBase_middleRows_int.out
  *
  * \sa class Block, block(Index,Index,Index,Index)
  */
inline RowsBlockXpr middleRows(Index startRow, Index numRows)
{
  return RowsBlockXpr(derived(), startRow, 0, numRows, cols());
}

/** This is the const version of middleRows(Index,Index).*/
inline ConstRowsBlockXpr middleRows(Index startRow, Index numRows) const
{
  return ConstRowsBlockXpr(derived(), startRow, 0, numRows, cols());
}

/** \returns a block consisting of a range of rows of *this.
  *
  * \tparam N the number of rows in the block
  * \param startRow the index of the first row in the block
  *
  * Example: \include DenseBase_template_int_middleRows.cpp
  * Output: \verbinclude DenseBase_template_int_middleRows.out
  *
  * \sa class Block, block(Index,Index,Index,Index)
  */
template<int N>
inline typename NRowsBlockXpr<N>::Type middleRows(Index startRow)
{
  return typename NRowsBlockXpr<N>::Type(derived(), startRow, 0, N, cols());
}

/** This is the const version of middleRows<int>().*/
template<int N>
inline typename ConstNRowsBlockXpr<N>::Type middleRows(Index startRow) const
{
  return typename ConstNRowsBlockXpr<N>::Type(derived(), startRow, 0, N, cols());
}



/** \returns a block consisting of the left columns of *this.
  *
  * \param n the number of columns in the block
  *
  * Example: \include MatrixBase_leftCols_int.cpp
  * Output: \verbinclude MatrixBase_leftCols_int.out
  *
  * \sa class Block, block(Index,Index,Index,Index)
  */
inline ColsBlockXpr leftCols(Index n)
{
  return ColsBlockXpr(derived(), 0, 0, rows(), n);
}

/** This is the const version of leftCols(Index).*/
inline ConstColsBlockXpr leftCols(Index n) const
{
  return ConstColsBlockXpr(derived(), 0, 0, rows(), n);
}

/** \returns a block consisting of the left columns of *this.
  *
  * \tparam N the number of columns in the block
  *
  * Example: \include MatrixBase_template_int_leftCols.cpp
  * Output: \verbinclude MatrixBase_template_int_leftCols.out
  *
  * \sa class Block, block(Index,Index,Index,Index)
  */
template<int N>
inline typename NColsBlockXpr<N>::Type leftCols()
{
  return typename NColsBlockXpr<N>::Type(derived(), 0, 0, rows(), N);
}

/** This is the const version of leftCols<int>().*/
template<int N>
inline typename ConstNColsBlockXpr<N>::Type leftCols() const
{
  return typename ConstNColsBlockXpr<N>::Type(derived(), 0, 0, rows(), N);
}



/** \returns a block consisting of the right columns of *this.
  *
  * \param n the number of columns in the block
  *
  * Example: \include MatrixBase_rightCols_int.cpp
  * Output: \verbinclude MatrixBase_rightCols_int.out
  *
  * \sa class Block, block(Index,Index,Index,Index)
  */
inline ColsBlockXpr rightCols(Index n)
{
  return ColsBlockXpr(derived(), 0, cols() - n, rows(), n);
}

/** This is the const version of rightCols(Index).*/
inline ConstColsBlockXpr rightCols(Index n) const
{
  return ConstColsBlockXpr(derived(), 0, cols() - n, rows(), n);
}

/** \returns a block consisting of the right columns of *this.
  *
  * \tparam N the number of columns in the block
  *
  * Example: \include MatrixBase_template_int_rightCols.cpp
  * Output: \verbinclude MatrixBase_template_int_rightCols.out
  *
  * \sa class Block, block(Index,Index,Index,Index)
  */
template<int N>
inline typename NColsBlockXpr<N>::Type rightCols()
{
  return typename NColsBlockXpr<N>::Type(derived(), 0, cols() - N, rows(), N);
}

/** This is the const version of rightCols<int>().*/
template<int N>
inline typename ConstNColsBlockXpr<N>::Type rightCols() const
{
  return typename ConstNColsBlockXpr<N>::Type(derived(), 0, cols() - N, rows(), N);
}



/** \returns a block consisting of a range of columns of *this.
  *
  * \param startCol the index of the first column in the block
  * \param numCols the number of columns in the block
  *
  * Example: \include DenseBase_middleCols_int.cpp
  * Output: \verbinclude DenseBase_middleCols_int.out
  *
  * \sa class Block, block(Index,Index,Index,Index)
  */
inline ColsBlockXpr middleCols(Index startCol, Index numCols)
{
  return ColsBlockXpr(derived(), 0, startCol, rows(), numCols);
}

/** This is the const version of middleCols(Index,Index).*/
inline ConstColsBlockXpr middleCols(Index startCol, Index numCols) const
{
  return ConstColsBlockXpr(derived(), 0, startCol, rows(), numCols);
}

/** \returns a block consisting of a range of columns of *this.
  *
  * \tparam N the number of columns in the block
  * \param startCol the index of the first column in the block
  *
  * Example: \include DenseBase_template_int_middleCols.cpp
  * Output: \verbinclude DenseBase_template_int_middleCols.out
  *
  * \sa class Block, block(Index,Index,Index,Index)
  */
template<int N>
inline typename NColsBlockXpr<N>::Type middleCols(Index startCol)
{
  return typename NColsBlockXpr<N>::Type(derived(), 0, startCol, rows(), N);
}

/** This is the const version of middleCols<int>().*/
template<int N>
inline typename ConstNColsBlockXpr<N>::Type middleCols(Index startCol) const
{
  return typename ConstNColsBlockXpr<N>::Type(derived(), 0, startCol, rows(), N);
}



/** \returns a fixed-size expression of a block in *this.
  *
  * The template parameters \a BlockRows and \a BlockCols are the number of
  * rows and columns in the block.
  *
  * \param startRow the first row in the block
  * \param startCol the first column in the block
  *
  * Example: \include MatrixBase_block_int_int.cpp
  * Output: \verbinclude MatrixBase_block_int_int.out
  *
  * \note since block is a templated member, the keyword template has to be used
  * if the matrix type is also a template parameter: \code m.template block<3,3>(1,1); \endcode
  *
  * \sa class Block, block(Index,Index,Index,Index)
  */
template<int BlockRows, int BlockCols>
inline Block<Derived, BlockRows, BlockCols> block(Index startRow, Index startCol)
{
  return Block<Derived, BlockRows, BlockCols>(derived(), startRow, startCol);
}

/** This is the const version of block<>(Index, Index). */
template<int BlockRows, int BlockCols>
inline const Block<const Derived, BlockRows, BlockCols> block(Index startRow, Index startCol) const
{
  return Block<const Derived, BlockRows, BlockCols>(derived(), startRow, startCol);
}

/** \returns an expression of the \a i-th column of *this. Note that the numbering starts at 0.
  *
  * Example: \include MatrixBase_col.cpp
  * Output: \verbinclude MatrixBase_col.out
  *
  * \sa row(), class Block */
inline ColXpr col(Index i)
{
  return ColXpr(derived(), i);
}

/** This is the const version of col(). */
inline ConstColXpr col(Index i) const
{
  return ConstColXpr(derived(), i);
}

/** \returns an expression of the \a i-th row of *this. Note that the numbering starts at 0.
  *
  * Example: \include MatrixBase_row.cpp
  * Output: \verbinclude MatrixBase_row.out
  *
  * \sa col(), class Block */
inline RowXpr row(Index i)
{
  return RowXpr(derived(), i);
}

/** This is the const version of row(). */
inline ConstRowXpr row(Index i) const
{
  return ConstRowXpr(derived(), i);
}

#endif // EIGEN_BLOCKMETHODS_H
