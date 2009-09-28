// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
// Copyright (C) 2008 Daniel Gomez Ferro <dgomezferro@gmail.com>
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

#ifndef EIGEN_SPARSE_BLOCK_H
#define EIGEN_SPARSE_BLOCK_H

template<typename MatrixType, int Size>
struct ei_traits<SparseInnerVectorSet<MatrixType, Size> >
{
  typedef typename ei_traits<MatrixType>::Scalar Scalar;
  enum {
    IsRowMajor = (int(MatrixType::Flags)&RowMajorBit)==RowMajorBit,
    Flags = MatrixType::Flags,
    RowsAtCompileTime = IsRowMajor ? Size : MatrixType::RowsAtCompileTime,
    ColsAtCompileTime = IsRowMajor ? MatrixType::ColsAtCompileTime : Size,
    CoeffReadCost = MatrixType::CoeffReadCost
  };
};

template<typename MatrixType, int Size>
class SparseInnerVectorSet : ei_no_assignment_operator,
  public SparseMatrixBase<SparseInnerVectorSet<MatrixType, Size> >
{
    enum { IsRowMajor = ei_traits<SparseInnerVectorSet>::IsRowMajor };
  public:

    EIGEN_SPARSE_GENERIC_PUBLIC_INTERFACE(SparseInnerVectorSet)
    class InnerIterator: public MatrixType::InnerIterator
    {
      public:
        inline InnerIterator(const SparseInnerVectorSet& xpr, int outer)
          : MatrixType::InnerIterator(xpr.m_matrix, xpr.m_outerStart + outer)
        {}
    };

    inline SparseInnerVectorSet(const MatrixType& matrix, int outerStart, int outerSize)
      : m_matrix(matrix), m_outerStart(outerStart), m_outerSize(outerSize)
    {
      ei_assert( (outerStart>=0) && ((outerStart+outerSize)<=matrix.outerSize()) );
    }

    inline SparseInnerVectorSet(const MatrixType& matrix, int outer)
      : m_matrix(matrix), m_outerStart(outer), m_outerSize(Size)
    {
      ei_assert(Size!=Dynamic);
      ei_assert( (outer>=0) && (outer<matrix.outerSize()) );
    }

//     template<typename OtherDerived>
//     inline SparseInnerVectorSet& operator=(const SparseMatrixBase<OtherDerived>& other)
//     {
//       return *this;
//     }

//     template<typename Sparse>
//     inline SparseInnerVectorSet& operator=(const SparseMatrixBase<OtherDerived>& other)
//     {
//       return *this;
//     }

    EIGEN_STRONG_INLINE int rows() const { return IsRowMajor ? m_outerSize.value() : m_matrix.rows(); }
    EIGEN_STRONG_INLINE int cols() const { return IsRowMajor ? m_matrix.cols() : m_outerSize.value(); }

  protected:

    const typename MatrixType::Nested m_matrix;
    int m_outerStart;
    const ei_int_if_dynamic<Size> m_outerSize;

};

/***************************************************************************
* specialisation for DynamicSparseMatrix
***************************************************************************/

template<typename _Scalar, int _Options, int Size>
class SparseInnerVectorSet<DynamicSparseMatrix<_Scalar, _Options>, Size>
  : public SparseMatrixBase<SparseInnerVectorSet<DynamicSparseMatrix<_Scalar, _Options>, Size> >
{
    typedef DynamicSparseMatrix<_Scalar, _Options> MatrixType;
    enum { IsRowMajor = ei_traits<SparseInnerVectorSet>::IsRowMajor };
  public:

    EIGEN_SPARSE_GENERIC_PUBLIC_INTERFACE(SparseInnerVectorSet)
    class InnerIterator: public MatrixType::InnerIterator
    {
      public:
        inline InnerIterator(const SparseInnerVectorSet& xpr, int outer)
          : MatrixType::InnerIterator(xpr.m_matrix, xpr.m_outerStart + outer)
        {}
    };

    inline SparseInnerVectorSet(const MatrixType& matrix, int outerStart, int outerSize)
      : m_matrix(matrix), m_outerStart(outerStart), m_outerSize(outerSize)
    {
      ei_assert( (outerStart>=0) && ((outerStart+outerSize)<=matrix.outerSize()) );
    }

    inline SparseInnerVectorSet(const MatrixType& matrix, int outer)
      : m_matrix(matrix), m_outerStart(outer), m_outerSize(Size)
    {
      ei_assert(Size!=Dynamic);
      ei_assert( (outer>=0) && (outer<matrix.outerSize()) );
    }

    template<typename OtherDerived>
    inline SparseInnerVectorSet& operator=(const SparseMatrixBase<OtherDerived>& other)
    {
      if (IsRowMajor != ((OtherDerived::Flags&RowMajorBit)==RowMajorBit))
      {
        // need to transpose => perform a block evaluation followed by a big swap
        DynamicSparseMatrix<Scalar,IsRowMajor?RowMajorBit:0> aux(other);
        *this = aux.markAsRValue();
      }
      else
      {
        // evaluate/copy vector per vector
        for (int j=0; j<m_outerSize.value(); ++j)
        {
          SparseVector<Scalar,IsRowMajor ? RowMajorBit : 0> aux(other.innerVector(j));
          m_matrix.const_cast_derived()._data()[m_outerStart+j].swap(aux._data());
        }
      }
      return *this;
    }

    inline SparseInnerVectorSet& operator=(const SparseInnerVectorSet& other)
    {
      return operator=<SparseInnerVectorSet>(other);
    }

//     template<typename Sparse>
//     inline SparseInnerVectorSet& operator=(const SparseMatrixBase<OtherDerived>& other)
//     {
//       return *this;
//     }

    EIGEN_STRONG_INLINE int rows() const { return IsRowMajor ? m_outerSize.value() : m_matrix.rows(); }
    EIGEN_STRONG_INLINE int cols() const { return IsRowMajor ? m_matrix.cols() : m_outerSize.value(); }

  protected:

    const typename MatrixType::Nested m_matrix;
    int m_outerStart;
    const ei_int_if_dynamic<Size> m_outerSize;

};


/***************************************************************************
* specialisation for SparseMatrix
***************************************************************************/
/*
template<typename _Scalar, int _Options, int Size>
class SparseInnerVectorSet<SparseMatrix<_Scalar, _Options>, Size>
  : public SparseMatrixBase<SparseInnerVectorSet<SparseMatrix<_Scalar, _Options>, Size> >
{
    typedef DynamicSparseMatrix<_Scalar, _Options> MatrixType;
    enum { IsRowMajor = ei_traits<SparseInnerVectorSet>::IsRowMajor };
  public:

    EIGEN_SPARSE_GENERIC_PUBLIC_INTERFACE(SparseInnerVectorSet)
    class InnerIterator: public MatrixType::InnerIterator
    {
      public:
        inline InnerIterator(const SparseInnerVectorSet& xpr, int outer)
          : MatrixType::InnerIterator(xpr.m_matrix, xpr.m_outerStart + outer)
        {}
    };

    inline SparseInnerVectorSet(const MatrixType& matrix, int outerStart, int outerSize)
      : m_matrix(matrix), m_outerStart(outerStart), m_outerSize(outerSize)
    {
      ei_assert( (outerStart>=0) && ((outerStart+outerSize)<=matrix.outerSize()) );
    }

    inline SparseInnerVectorSet(const MatrixType& matrix, int outer)
      : m_matrix(matrix), m_outerStart(outer)
    {
      ei_assert(Size==1);
      ei_assert( (outer>=0) && (outer<matrix.outerSize()) );
    }

    template<typename OtherDerived>
    inline SparseInnerVectorSet& operator=(const SparseMatrixBase<OtherDerived>& other)
    {
      if (IsRowMajor != ((OtherDerived::Flags&RowMajorBit)==RowMajorBit))
      {
        // need to transpose => perform a block evaluation followed by a big swap
        DynamicSparseMatrix<Scalar,IsRowMajor?RowMajorBit:0> aux(other);
        *this = aux.markAsRValue();
      }
      else
      {
        // evaluate/copy vector per vector
        for (int j=0; j<m_outerSize.value(); ++j)
        {
          SparseVector<Scalar,IsRowMajor ? RowMajorBit : 0> aux(other.innerVector(j));
          m_matrix.const_cast_derived()._data()[m_outerStart+j].swap(aux._data());
        }
      }
      return *this;
    }

    inline SparseInnerVectorSet& operator=(const SparseInnerVectorSet& other)
    {
      return operator=<SparseInnerVectorSet>(other);
    }

    inline const Scalar* _valuePtr() const
    { return m_matrix._valuePtr() + m_matrix._outerIndexPtr()[m_outerStart]; }
    inline const int* _innerIndexPtr() const
    { return m_matrix._innerIndexPtr() + m_matrix._outerIndexPtr()[m_outerStart]; }
    inline const int* _outerIndexPtr() const { return m_matrix._outerIndexPtr() + m_outerStart; }

//     template<typename Sparse>
//     inline SparseInnerVectorSet& operator=(const SparseMatrixBase<OtherDerived>& other)
//     {
//       return *this;
//     }

    EIGEN_STRONG_INLINE int rows() const { return IsRowMajor ? m_outerSize.value() : m_matrix.rows(); }
    EIGEN_STRONG_INLINE int cols() const { return IsRowMajor ? m_matrix.cols() : m_outerSize.value(); }

  protected:

    const typename MatrixType::Nested m_matrix;
    int m_outerStart;
    const ei_int_if_dynamic<Size> m_outerSize;

};
*/
//----------

/** \returns the i-th row of the matrix \c *this. For row-major matrix only. */
template<typename Derived>
SparseInnerVectorSet<Derived,1> SparseMatrixBase<Derived>::row(int i)
{
  EIGEN_STATIC_ASSERT(IsRowMajor,THIS_METHOD_IS_ONLY_FOR_ROW_MAJOR_MATRICES);
  return innerVector(i);
}

/** \returns the i-th row of the matrix \c *this. For row-major matrix only.
  * (read-only version) */
template<typename Derived>
const SparseInnerVectorSet<Derived,1> SparseMatrixBase<Derived>::row(int i) const
{
  EIGEN_STATIC_ASSERT(IsRowMajor,THIS_METHOD_IS_ONLY_FOR_ROW_MAJOR_MATRICES);
  return innerVector(i);
}

/** \returns the i-th column of the matrix \c *this. For column-major matrix only. */
template<typename Derived>
SparseInnerVectorSet<Derived,1> SparseMatrixBase<Derived>::col(int i)
{
  EIGEN_STATIC_ASSERT(!IsRowMajor,THIS_METHOD_IS_ONLY_FOR_COLUMN_MAJOR_MATRICES);
  return innerVector(i);
}

/** \returns the i-th column of the matrix \c *this. For column-major matrix only.
  * (read-only version) */
template<typename Derived>
const SparseInnerVectorSet<Derived,1> SparseMatrixBase<Derived>::col(int i) const
{
  EIGEN_STATIC_ASSERT(!IsRowMajor,THIS_METHOD_IS_ONLY_FOR_COLUMN_MAJOR_MATRICES);
  return innerVector(i);
}

/** \returns the \a outer -th column (resp. row) of the matrix \c *this if \c *this
  * is col-major (resp. row-major).
  */
template<typename Derived>
SparseInnerVectorSet<Derived,1> SparseMatrixBase<Derived>::innerVector(int outer)
{ return SparseInnerVectorSet<Derived,1>(derived(), outer); }

/** \returns the \a outer -th column (resp. row) of the matrix \c *this if \c *this
  * is col-major (resp. row-major). Read-only.
  */
template<typename Derived>
const SparseInnerVectorSet<Derived,1> SparseMatrixBase<Derived>::innerVector(int outer) const
{ return SparseInnerVectorSet<Derived,1>(derived(), outer); }

//----------

/** \returns the i-th row of the matrix \c *this. For row-major matrix only. */
template<typename Derived>
SparseInnerVectorSet<Derived,Dynamic> SparseMatrixBase<Derived>::subrows(int start, int size)
{
  EIGEN_STATIC_ASSERT(IsRowMajor,THIS_METHOD_IS_ONLY_FOR_ROW_MAJOR_MATRICES);
  return innerVectors(start, size);
}

/** \returns the i-th row of the matrix \c *this. For row-major matrix only.
  * (read-only version) */
template<typename Derived>
const SparseInnerVectorSet<Derived,Dynamic> SparseMatrixBase<Derived>::subrows(int start, int size) const
{
  EIGEN_STATIC_ASSERT(IsRowMajor,THIS_METHOD_IS_ONLY_FOR_ROW_MAJOR_MATRICES);
  return innerVectors(start, size);
}

/** \returns the i-th column of the matrix \c *this. For column-major matrix only. */
template<typename Derived>
SparseInnerVectorSet<Derived,Dynamic> SparseMatrixBase<Derived>::subcols(int start, int size)
{
  EIGEN_STATIC_ASSERT(!IsRowMajor,THIS_METHOD_IS_ONLY_FOR_COLUMN_MAJOR_MATRICES);
  return innerVectors(start, size);
}

/** \returns the i-th column of the matrix \c *this. For column-major matrix only.
  * (read-only version) */
template<typename Derived>
const SparseInnerVectorSet<Derived,Dynamic> SparseMatrixBase<Derived>::subcols(int start, int size) const
{
  EIGEN_STATIC_ASSERT(!IsRowMajor,THIS_METHOD_IS_ONLY_FOR_COLUMN_MAJOR_MATRICES);
  return innerVectors(start, size);
}

/** \returns the \a outer -th column (resp. row) of the matrix \c *this if \c *this
  * is col-major (resp. row-major).
  */
template<typename Derived>
SparseInnerVectorSet<Derived,Dynamic> SparseMatrixBase<Derived>::innerVectors(int outerStart, int outerSize)
{ return SparseInnerVectorSet<Derived,Dynamic>(derived(), outerStart, outerSize); }

/** \returns the \a outer -th column (resp. row) of the matrix \c *this if \c *this
  * is col-major (resp. row-major). Read-only.
  */
template<typename Derived>
const SparseInnerVectorSet<Derived,Dynamic> SparseMatrixBase<Derived>::innerVectors(int outerStart, int outerSize) const
{ return SparseInnerVectorSet<Derived,Dynamic>(derived(), outerStart, outerSize); }

# if 0
template<typename MatrixType, int BlockRows, int BlockCols, int PacketAccess>
class Block<MatrixType,BlockRows,BlockCols,PacketAccess,IsSparse>
  : public SparseMatrixBase<Block<MatrixType,BlockRows,BlockCols,PacketAccess,IsSparse> >
{
public:

    _EIGEN_GENERIC_PUBLIC_INTERFACE(Block, SparseMatrixBase<Block>)
    class InnerIterator;

    /** Column or Row constructor
      */
    inline Block(const MatrixType& matrix, int i)
      : m_matrix(matrix),
        // It is a row if and only if BlockRows==1 and BlockCols==MatrixType::ColsAtCompileTime,
        // and it is a column if and only if BlockRows==MatrixType::RowsAtCompileTime and BlockCols==1,
        // all other cases are invalid.
        // The case a 1x1 matrix seems ambiguous, but the result is the same anyway.
        m_startRow( (BlockRows==1) && (BlockCols==MatrixType::ColsAtCompileTime) ? i : 0),
        m_startCol( (BlockRows==MatrixType::RowsAtCompileTime) && (BlockCols==1) ? i : 0),
        m_blockRows(matrix.rows()), // if it is a row, then m_blockRows has a fixed-size of 1, so no pb to try to overwrite it
        m_blockCols(matrix.cols())  // same for m_blockCols
    {
      ei_assert( (i>=0) && (
          ((BlockRows==1) && (BlockCols==MatrixType::ColsAtCompileTime) && i<matrix.rows())
        ||((BlockRows==MatrixType::RowsAtCompileTime) && (BlockCols==1) && i<matrix.cols())));
    }

    /** Fixed-size constructor
      */
    inline Block(const MatrixType& matrix, int startRow, int startCol)
      : m_matrix(matrix), m_startRow(startRow), m_startCol(startCol),
        m_blockRows(matrix.rows()), m_blockCols(matrix.cols())
    {
      EIGEN_STATIC_ASSERT(RowsAtCompileTime!=Dynamic && RowsAtCompileTime!=Dynamic,THIS_METHOD_IS_ONLY_FOR_FIXED_SIZE)
      ei_assert(startRow >= 0 && BlockRows >= 1 && startRow + BlockRows <= matrix.rows()
          && startCol >= 0 && BlockCols >= 1 && startCol + BlockCols <= matrix.cols());
    }

    /** Dynamic-size constructor
      */
    inline Block(const MatrixType& matrix,
          int startRow, int startCol,
          int blockRows, int blockCols)
      : m_matrix(matrix), m_startRow(startRow), m_startCol(startCol),
                          m_blockRows(blockRows), m_blockCols(blockCols)
    {
      ei_assert((RowsAtCompileTime==Dynamic || RowsAtCompileTime==blockRows)
          && (ColsAtCompileTime==Dynamic || ColsAtCompileTime==blockCols));
      ei_assert(startRow >= 0 && blockRows >= 1 && startRow + blockRows <= matrix.rows()
          && startCol >= 0 && blockCols >= 1 && startCol + blockCols <= matrix.cols());
    }

    inline int rows() const { return m_blockRows.value(); }
    inline int cols() const { return m_blockCols.value(); }

    inline int stride(void) const { return m_matrix.stride(); }

    inline Scalar& coeffRef(int row, int col)
    {
      return m_matrix.const_cast_derived()
               .coeffRef(row + m_startRow.value(), col + m_startCol.value());
    }

    inline const Scalar coeff(int row, int col) const
    {
      return m_matrix.coeff(row + m_startRow.value(), col + m_startCol.value());
    }

    inline Scalar& coeffRef(int index)
    {
      return m_matrix.const_cast_derived()
             .coeffRef(m_startRow.value() + (RowsAtCompileTime == 1 ? 0 : index),
                       m_startCol.value() + (RowsAtCompileTime == 1 ? index : 0));
    }

    inline const Scalar coeff(int index) const
    {
      return m_matrix
             .coeff(m_startRow.value() + (RowsAtCompileTime == 1 ? 0 : index),
                    m_startCol.value() + (RowsAtCompileTime == 1 ? index : 0));
    }

  protected:

    const typename MatrixType::Nested m_matrix;
    const ei_int_if_dynamic<MatrixType::RowsAtCompileTime == 1 ? 0 : Dynamic> m_startRow;
    const ei_int_if_dynamic<MatrixType::ColsAtCompileTime == 1 ? 0 : Dynamic> m_startCol;
    const ei_int_if_dynamic<RowsAtCompileTime> m_blockRows;
    const ei_int_if_dynamic<ColsAtCompileTime> m_blockCols;

};
#endif

#endif // EIGEN_SPARSE_BLOCK_H
