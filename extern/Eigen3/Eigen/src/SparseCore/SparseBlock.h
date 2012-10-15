// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef EIGEN_SPARSE_BLOCK_H
#define EIGEN_SPARSE_BLOCK_H

namespace Eigen { 

namespace internal {
template<typename MatrixType, int Size>
struct traits<SparseInnerVectorSet<MatrixType, Size> >
{
  typedef typename traits<MatrixType>::Scalar Scalar;
  typedef typename traits<MatrixType>::Index Index;
  typedef typename traits<MatrixType>::StorageKind StorageKind;
  typedef MatrixXpr XprKind;
  enum {
    IsRowMajor = (int(MatrixType::Flags)&RowMajorBit)==RowMajorBit,
    Flags = MatrixType::Flags,
    RowsAtCompileTime = IsRowMajor ? Size : MatrixType::RowsAtCompileTime,
    ColsAtCompileTime = IsRowMajor ? MatrixType::ColsAtCompileTime : Size,
    MaxRowsAtCompileTime = RowsAtCompileTime,
    MaxColsAtCompileTime = ColsAtCompileTime,
    CoeffReadCost = MatrixType::CoeffReadCost
  };
};
} // end namespace internal

template<typename MatrixType, int Size>
class SparseInnerVectorSet : internal::no_assignment_operator,
  public SparseMatrixBase<SparseInnerVectorSet<MatrixType, Size> >
{
  public:

    enum { IsRowMajor = internal::traits<SparseInnerVectorSet>::IsRowMajor };

    EIGEN_SPARSE_PUBLIC_INTERFACE(SparseInnerVectorSet)
    class InnerIterator: public MatrixType::InnerIterator
    {
      public:
        inline InnerIterator(const SparseInnerVectorSet& xpr, Index outer)
          : MatrixType::InnerIterator(xpr.m_matrix, xpr.m_outerStart + outer), m_outer(outer)
        {}
        inline Index row() const { return IsRowMajor ? m_outer : this->index(); }
        inline Index col() const { return IsRowMajor ? this->index() : m_outer; }
      protected:
        Index m_outer;
    };
    class ReverseInnerIterator: public MatrixType::ReverseInnerIterator
    {
      public:
        inline ReverseInnerIterator(const SparseInnerVectorSet& xpr, Index outer)
          : MatrixType::ReverseInnerIterator(xpr.m_matrix, xpr.m_outerStart + outer), m_outer(outer)
        {}
        inline Index row() const { return IsRowMajor ? m_outer : this->index(); }
        inline Index col() const { return IsRowMajor ? this->index() : m_outer; }
      protected:
        Index m_outer;
    };

    inline SparseInnerVectorSet(const MatrixType& matrix, Index outerStart, Index outerSize)
      : m_matrix(matrix), m_outerStart(outerStart), m_outerSize(outerSize)
    {
      eigen_assert( (outerStart>=0) && ((outerStart+outerSize)<=matrix.outerSize()) );
    }

    inline SparseInnerVectorSet(const MatrixType& matrix, Index outer)
      : m_matrix(matrix), m_outerStart(outer), m_outerSize(Size)
    {
      eigen_assert(Size!=Dynamic);
      eigen_assert( (outer>=0) && (outer<matrix.outerSize()) );
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

    EIGEN_STRONG_INLINE Index rows() const { return IsRowMajor ? m_outerSize.value() : m_matrix.rows(); }
    EIGEN_STRONG_INLINE Index cols() const { return IsRowMajor ? m_matrix.cols() : m_outerSize.value(); }

  protected:

    const typename MatrixType::Nested m_matrix;
    Index m_outerStart;
    const internal::variable_if_dynamic<Index, Size> m_outerSize;
};


/***************************************************************************
* specialisation for SparseMatrix
***************************************************************************/

template<typename _Scalar, int _Options, typename _Index, int Size>
class SparseInnerVectorSet<SparseMatrix<_Scalar, _Options, _Index>, Size>
  : public SparseMatrixBase<SparseInnerVectorSet<SparseMatrix<_Scalar, _Options, _Index>, Size> >
{
    typedef SparseMatrix<_Scalar, _Options, _Index> MatrixType;
  public:

    enum { IsRowMajor = internal::traits<SparseInnerVectorSet>::IsRowMajor };

    EIGEN_SPARSE_PUBLIC_INTERFACE(SparseInnerVectorSet)
    class InnerIterator: public MatrixType::InnerIterator
    {
      public:
        inline InnerIterator(const SparseInnerVectorSet& xpr, Index outer)
          : MatrixType::InnerIterator(xpr.m_matrix, xpr.m_outerStart + outer), m_outer(outer)
        {}
        inline Index row() const { return IsRowMajor ? m_outer : this->index(); }
        inline Index col() const { return IsRowMajor ? this->index() : m_outer; }
      protected:
        Index m_outer;
    };
    class ReverseInnerIterator: public MatrixType::ReverseInnerIterator
    {
      public:
        inline ReverseInnerIterator(const SparseInnerVectorSet& xpr, Index outer)
          : MatrixType::ReverseInnerIterator(xpr.m_matrix, xpr.m_outerStart + outer), m_outer(outer)
        {}
        inline Index row() const { return IsRowMajor ? m_outer : this->index(); }
        inline Index col() const { return IsRowMajor ? this->index() : m_outer; }
      protected:
        Index m_outer;
    };

    inline SparseInnerVectorSet(const MatrixType& matrix, Index outerStart, Index outerSize)
      : m_matrix(matrix), m_outerStart(outerStart), m_outerSize(outerSize)
    {
      eigen_assert( (outerStart>=0) && ((outerStart+outerSize)<=matrix.outerSize()) );
    }

    inline SparseInnerVectorSet(const MatrixType& matrix, Index outer)
      : m_matrix(matrix), m_outerStart(outer), m_outerSize(Size)
    {
      eigen_assert(Size==1);
      eigen_assert( (outer>=0) && (outer<matrix.outerSize()) );
    }

    template<typename OtherDerived>
    inline SparseInnerVectorSet& operator=(const SparseMatrixBase<OtherDerived>& other)
    {
      typedef typename internal::remove_all<typename MatrixType::Nested>::type _NestedMatrixType;
      _NestedMatrixType& matrix = const_cast<_NestedMatrixType&>(m_matrix);;
      // This assignement is slow if this vector set is not empty
      // and/or it is not at the end of the nonzeros of the underlying matrix.

      // 1 - eval to a temporary to avoid transposition and/or aliasing issues
      SparseMatrix<Scalar, IsRowMajor ? RowMajor : ColMajor, Index> tmp(other);

      // 2 - let's check whether there is enough allocated memory
      Index nnz           = tmp.nonZeros();
      Index nnz_previous  = nonZeros();
      Index free_size     = Index(matrix.data().allocatedSize()) + nnz_previous;
      Index nnz_head      = m_outerStart==0 ? 0 : matrix.outerIndexPtr()[m_outerStart];
      Index tail          = m_matrix.outerIndexPtr()[m_outerStart+m_outerSize.value()];
      Index nnz_tail      = matrix.nonZeros() - tail;

      if(nnz>free_size)
      {
        // realloc manually to reduce copies
        typename MatrixType::Storage newdata(m_matrix.nonZeros() - nnz_previous + nnz);

        std::memcpy(&newdata.value(0), &m_matrix.data().value(0), nnz_head*sizeof(Scalar));
        std::memcpy(&newdata.index(0), &m_matrix.data().index(0), nnz_head*sizeof(Index));

        std::memcpy(&newdata.value(nnz_head), &tmp.data().value(0), nnz*sizeof(Scalar));
        std::memcpy(&newdata.index(nnz_head), &tmp.data().index(0), nnz*sizeof(Index));

        std::memcpy(&newdata.value(nnz_head+nnz), &matrix.data().value(tail), nnz_tail*sizeof(Scalar));
        std::memcpy(&newdata.index(nnz_head+nnz), &matrix.data().index(tail), nnz_tail*sizeof(Index));

        matrix.data().swap(newdata);
      }
      else
      {
        // no need to realloc, simply copy the tail at its respective position and insert tmp
        matrix.data().resize(nnz_head + nnz + nnz_tail);

        if(nnz<nnz_previous)
        {
          std::memcpy(&matrix.data().value(nnz_head+nnz), &matrix.data().value(tail), nnz_tail*sizeof(Scalar));
          std::memcpy(&matrix.data().index(nnz_head+nnz), &matrix.data().index(tail), nnz_tail*sizeof(Index));
        }
        else
        {
          for(Index i=nnz_tail-1; i>=0; --i)
          {
            matrix.data().value(nnz_head+nnz+i) = matrix.data().value(tail+i);
            matrix.data().index(nnz_head+nnz+i) = matrix.data().index(tail+i);
          }
        }

        std::memcpy(&matrix.data().value(nnz_head), &tmp.data().value(0), nnz*sizeof(Scalar));
        std::memcpy(&matrix.data().index(nnz_head), &tmp.data().index(0), nnz*sizeof(Index));
      }

      // update outer index pointers
      Index p = nnz_head;
      for(Index k=0; k<m_outerSize.value(); ++k)
      {
        matrix.outerIndexPtr()[m_outerStart+k] = p;
        p += tmp.innerVector(k).nonZeros();
      }
      std::ptrdiff_t offset = nnz - nnz_previous;
      for(Index k = m_outerStart + m_outerSize.value(); k<=matrix.outerSize(); ++k)
      {
        matrix.outerIndexPtr()[k] += offset;
      }

      return *this;
    }

    inline SparseInnerVectorSet& operator=(const SparseInnerVectorSet& other)
    {
      return operator=<SparseInnerVectorSet>(other);
    }

    inline const Scalar* valuePtr() const
    { return m_matrix.valuePtr() + m_matrix.outerIndexPtr()[m_outerStart]; }
    inline Scalar* valuePtr()
    { return m_matrix.const_cast_derived().valuePtr() + m_matrix.outerIndexPtr()[m_outerStart]; }

    inline const Index* innerIndexPtr() const
    { return m_matrix.innerIndexPtr() + m_matrix.outerIndexPtr()[m_outerStart]; }
    inline Index* innerIndexPtr()
    { return m_matrix.const_cast_derived().innerIndexPtr() + m_matrix.outerIndexPtr()[m_outerStart]; }

    inline const Index* outerIndexPtr() const
    { return m_matrix.outerIndexPtr() + m_outerStart; }
    inline Index* outerIndexPtr()
    { return m_matrix.const_cast_derived().outerIndexPtr() + m_outerStart; }

    Index nonZeros() const
    {
      if(m_matrix.isCompressed())
        return  std::size_t(m_matrix.outerIndexPtr()[m_outerStart+m_outerSize.value()])
              - std::size_t(m_matrix.outerIndexPtr()[m_outerStart]);
      else if(m_outerSize.value()==0)
        return 0;
      else
        return Map<const Matrix<Index,Size,1> >(m_matrix.innerNonZeroPtr()+m_outerStart, m_outerSize.value()).sum();
    }

    const Scalar& lastCoeff() const
    {
      EIGEN_STATIC_ASSERT_VECTOR_ONLY(SparseInnerVectorSet);
      eigen_assert(nonZeros()>0);
      if(m_matrix.isCompressed())
        return m_matrix.valuePtr()[m_matrix.outerIndexPtr()[m_outerStart+1]-1];
      else
        return m_matrix.valuePtr()[m_matrix.outerIndexPtr()[m_outerStart]+m_matrix.innerNonZeroPtr()[m_outerStart]-1];
    }

//     template<typename Sparse>
//     inline SparseInnerVectorSet& operator=(const SparseMatrixBase<OtherDerived>& other)
//     {
//       return *this;
//     }

    EIGEN_STRONG_INLINE Index rows() const { return IsRowMajor ? m_outerSize.value() : m_matrix.rows(); }
    EIGEN_STRONG_INLINE Index cols() const { return IsRowMajor ? m_matrix.cols() : m_outerSize.value(); }

  protected:

    typename MatrixType::Nested m_matrix;
    Index m_outerStart;
    const internal::variable_if_dynamic<Index, Size> m_outerSize;

};

//----------

/** \returns the i-th row of the matrix \c *this. For row-major matrix only. */
template<typename Derived>
SparseInnerVectorSet<Derived,1> SparseMatrixBase<Derived>::row(Index i)
{
  EIGEN_STATIC_ASSERT(IsRowMajor,THIS_METHOD_IS_ONLY_FOR_ROW_MAJOR_MATRICES);
  return innerVector(i);
}

/** \returns the i-th row of the matrix \c *this. For row-major matrix only.
  * (read-only version) */
template<typename Derived>
const SparseInnerVectorSet<Derived,1> SparseMatrixBase<Derived>::row(Index i) const
{
  EIGEN_STATIC_ASSERT(IsRowMajor,THIS_METHOD_IS_ONLY_FOR_ROW_MAJOR_MATRICES);
  return innerVector(i);
}

/** \returns the i-th column of the matrix \c *this. For column-major matrix only. */
template<typename Derived>
SparseInnerVectorSet<Derived,1> SparseMatrixBase<Derived>::col(Index i)
{
  EIGEN_STATIC_ASSERT(!IsRowMajor,THIS_METHOD_IS_ONLY_FOR_COLUMN_MAJOR_MATRICES);
  return innerVector(i);
}

/** \returns the i-th column of the matrix \c *this. For column-major matrix only.
  * (read-only version) */
template<typename Derived>
const SparseInnerVectorSet<Derived,1> SparseMatrixBase<Derived>::col(Index i) const
{
  EIGEN_STATIC_ASSERT(!IsRowMajor,THIS_METHOD_IS_ONLY_FOR_COLUMN_MAJOR_MATRICES);
  return innerVector(i);
}

/** \returns the \a outer -th column (resp. row) of the matrix \c *this if \c *this
  * is col-major (resp. row-major).
  */
template<typename Derived>
SparseInnerVectorSet<Derived,1> SparseMatrixBase<Derived>::innerVector(Index outer)
{ return SparseInnerVectorSet<Derived,1>(derived(), outer); }

/** \returns the \a outer -th column (resp. row) of the matrix \c *this if \c *this
  * is col-major (resp. row-major). Read-only.
  */
template<typename Derived>
const SparseInnerVectorSet<Derived,1> SparseMatrixBase<Derived>::innerVector(Index outer) const
{ return SparseInnerVectorSet<Derived,1>(derived(), outer); }

/** \returns the i-th row of the matrix \c *this. For row-major matrix only. */
template<typename Derived>
SparseInnerVectorSet<Derived,Dynamic> SparseMatrixBase<Derived>::middleRows(Index start, Index size)
{
  EIGEN_STATIC_ASSERT(IsRowMajor,THIS_METHOD_IS_ONLY_FOR_ROW_MAJOR_MATRICES);
  return innerVectors(start, size);
}

/** \returns the i-th row of the matrix \c *this. For row-major matrix only.
  * (read-only version) */
template<typename Derived>
const SparseInnerVectorSet<Derived,Dynamic> SparseMatrixBase<Derived>::middleRows(Index start, Index size) const
{
  EIGEN_STATIC_ASSERT(IsRowMajor,THIS_METHOD_IS_ONLY_FOR_ROW_MAJOR_MATRICES);
  return innerVectors(start, size);
}

/** \returns the i-th column of the matrix \c *this. For column-major matrix only. */
template<typename Derived>
SparseInnerVectorSet<Derived,Dynamic> SparseMatrixBase<Derived>::middleCols(Index start, Index size)
{
  EIGEN_STATIC_ASSERT(!IsRowMajor,THIS_METHOD_IS_ONLY_FOR_COLUMN_MAJOR_MATRICES);
  return innerVectors(start, size);
}

/** \returns the i-th column of the matrix \c *this. For column-major matrix only.
  * (read-only version) */
template<typename Derived>
const SparseInnerVectorSet<Derived,Dynamic> SparseMatrixBase<Derived>::middleCols(Index start, Index size) const
{
  EIGEN_STATIC_ASSERT(!IsRowMajor,THIS_METHOD_IS_ONLY_FOR_COLUMN_MAJOR_MATRICES);
  return innerVectors(start, size);
}



/** \returns the \a outer -th column (resp. row) of the matrix \c *this if \c *this
  * is col-major (resp. row-major).
  */
template<typename Derived>
SparseInnerVectorSet<Derived,Dynamic> SparseMatrixBase<Derived>::innerVectors(Index outerStart, Index outerSize)
{ return SparseInnerVectorSet<Derived,Dynamic>(derived(), outerStart, outerSize); }

/** \returns the \a outer -th column (resp. row) of the matrix \c *this if \c *this
  * is col-major (resp. row-major). Read-only.
  */
template<typename Derived>
const SparseInnerVectorSet<Derived,Dynamic> SparseMatrixBase<Derived>::innerVectors(Index outerStart, Index outerSize) const
{ return SparseInnerVectorSet<Derived,Dynamic>(derived(), outerStart, outerSize); }

} // end namespace Eigen

#endif // EIGEN_SPARSE_BLOCK_H
