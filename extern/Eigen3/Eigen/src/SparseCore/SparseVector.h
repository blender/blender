// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef EIGEN_SPARSEVECTOR_H
#define EIGEN_SPARSEVECTOR_H

namespace Eigen { 

/** \ingroup SparseCore_Module
  * \class SparseVector
  *
  * \brief a sparse vector class
  *
  * \tparam _Scalar the scalar type, i.e. the type of the coefficients
  *
  * See http://www.netlib.org/linalg/html_templates/node91.html for details on the storage scheme.
  *
  * This class can be extended with the help of the plugin mechanism described on the page
  * \ref TopicCustomizingEigen by defining the preprocessor symbol \c EIGEN_SPARSEVECTOR_PLUGIN.
  */

namespace internal {
template<typename _Scalar, int _Options, typename _Index>
struct traits<SparseVector<_Scalar, _Options, _Index> >
{
  typedef _Scalar Scalar;
  typedef _Index Index;
  typedef Sparse StorageKind;
  typedef MatrixXpr XprKind;
  enum {
    IsColVector = (_Options & RowMajorBit) ? 0 : 1,

    RowsAtCompileTime = IsColVector ? Dynamic : 1,
    ColsAtCompileTime = IsColVector ? 1 : Dynamic,
    MaxRowsAtCompileTime = RowsAtCompileTime,
    MaxColsAtCompileTime = ColsAtCompileTime,
    Flags = _Options | NestByRefBit | LvalueBit | (IsColVector ? 0 : RowMajorBit),
    CoeffReadCost = NumTraits<Scalar>::ReadCost,
    SupportedAccessPatterns = InnerRandomAccessPattern
  };
};
}

template<typename _Scalar, int _Options, typename _Index>
class SparseVector
  : public SparseMatrixBase<SparseVector<_Scalar, _Options, _Index> >
{
  public:
    EIGEN_SPARSE_PUBLIC_INTERFACE(SparseVector)
    EIGEN_SPARSE_INHERIT_ASSIGNMENT_OPERATOR(SparseVector, +=)
    EIGEN_SPARSE_INHERIT_ASSIGNMENT_OPERATOR(SparseVector, -=)

  protected:
  public:

    typedef SparseMatrixBase<SparseVector> SparseBase;
    enum { IsColVector = internal::traits<SparseVector>::IsColVector };
    
    enum {
      Options = _Options
    };

    internal::CompressedStorage<Scalar,Index> m_data;
    Index m_size;

    internal::CompressedStorage<Scalar,Index>& _data() { return m_data; }
    internal::CompressedStorage<Scalar,Index>& _data() const { return m_data; }

  public:

    EIGEN_STRONG_INLINE Index rows() const { return IsColVector ? m_size : 1; }
    EIGEN_STRONG_INLINE Index cols() const { return IsColVector ? 1 : m_size; }
    EIGEN_STRONG_INLINE Index innerSize() const { return m_size; }
    EIGEN_STRONG_INLINE Index outerSize() const { return 1; }

    EIGEN_STRONG_INLINE const Scalar* valuePtr() const { return &m_data.value(0); }
    EIGEN_STRONG_INLINE Scalar* valuePtr() { return &m_data.value(0); }

    EIGEN_STRONG_INLINE const Index* innerIndexPtr() const { return &m_data.index(0); }
    EIGEN_STRONG_INLINE Index* innerIndexPtr() { return &m_data.index(0); }

    inline Scalar coeff(Index row, Index col) const
    {
      eigen_assert((IsColVector ? col : row)==0);
      return coeff(IsColVector ? row : col);
    }
    inline Scalar coeff(Index i) const { return m_data.at(i); }

    inline Scalar& coeffRef(Index row, Index col)
    {
      eigen_assert((IsColVector ? col : row)==0);
      return coeff(IsColVector ? row : col);
    }

    /** \returns a reference to the coefficient value at given index \a i
      * This operation involes a log(rho*size) binary search. If the coefficient does not
      * exist yet, then a sorted insertion into a sequential buffer is performed.
      *
      * This insertion might be very costly if the number of nonzeros above \a i is large.
      */
    inline Scalar& coeffRef(Index i)
    {
      return m_data.atWithInsertion(i);
    }

  public:

    class InnerIterator;
    class ReverseInnerIterator;

    inline void setZero() { m_data.clear(); }

    /** \returns the number of non zero coefficients */
    inline Index nonZeros() const  { return static_cast<Index>(m_data.size()); }

    inline void startVec(Index outer)
    {
      EIGEN_UNUSED_VARIABLE(outer);
      eigen_assert(outer==0);
    }

    inline Scalar& insertBackByOuterInner(Index outer, Index inner)
    {
      EIGEN_UNUSED_VARIABLE(outer);
      eigen_assert(outer==0);
      return insertBack(inner);
    }
    inline Scalar& insertBack(Index i)
    {
      m_data.append(0, i);
      return m_data.value(m_data.size()-1);
    }

    inline Scalar& insert(Index row, Index col)
    {
      Index inner = IsColVector ? row : col;
      Index outer = IsColVector ? col : row;
      eigen_assert(outer==0);
      return insert(inner);
    }
    Scalar& insert(Index i)
    {
      Index startId = 0;
      Index p = Index(m_data.size()) - 1;
      // TODO smart realloc
      m_data.resize(p+2,1);

      while ( (p >= startId) && (m_data.index(p) > i) )
      {
        m_data.index(p+1) = m_data.index(p);
        m_data.value(p+1) = m_data.value(p);
        --p;
      }
      m_data.index(p+1) = i;
      m_data.value(p+1) = 0;
      return m_data.value(p+1);
    }

    /**
      */
    inline void reserve(Index reserveSize) { m_data.reserve(reserveSize); }


    inline void finalize() {}

    void prune(Scalar reference, RealScalar epsilon = NumTraits<RealScalar>::dummy_precision())
    {
      m_data.prune(reference,epsilon);
    }

    void resize(Index rows, Index cols)
    {
      eigen_assert(rows==1 || cols==1);
      resize(IsColVector ? rows : cols);
    }

    void resize(Index newSize)
    {
      m_size = newSize;
      m_data.clear();
    }

    void resizeNonZeros(Index size) { m_data.resize(size); }

    inline SparseVector() : m_size(0) { resize(0); }

    inline SparseVector(Index size) : m_size(0) { resize(size); }

    inline SparseVector(Index rows, Index cols) : m_size(0) { resize(rows,cols); }

    template<typename OtherDerived>
    inline SparseVector(const SparseMatrixBase<OtherDerived>& other)
      : m_size(0)
    {
      *this = other.derived();
    }

    inline SparseVector(const SparseVector& other)
      : m_size(0)
    {
      *this = other.derived();
    }

    inline void swap(SparseVector& other)
    {
      std::swap(m_size, other.m_size);
      m_data.swap(other.m_data);
    }

    inline SparseVector& operator=(const SparseVector& other)
    {
      if (other.isRValue())
      {
        swap(other.const_cast_derived());
      }
      else
      {
        resize(other.size());
        m_data = other.m_data;
      }
      return *this;
    }

    template<typename OtherDerived>
    inline SparseVector& operator=(const SparseMatrixBase<OtherDerived>& other)
    {
      if (int(RowsAtCompileTime)!=int(OtherDerived::RowsAtCompileTime))
        return assign(other.transpose());
      else
        return assign(other);
    }

    #ifndef EIGEN_PARSED_BY_DOXYGEN
    template<typename Lhs, typename Rhs>
    inline SparseVector& operator=(const SparseSparseProduct<Lhs,Rhs>& product)
    {
      return Base::operator=(product);
    }
    #endif

    friend std::ostream & operator << (std::ostream & s, const SparseVector& m)
    {
      for (Index i=0; i<m.nonZeros(); ++i)
        s << "(" << m.m_data.value(i) << "," << m.m_data.index(i) << ") ";
      s << std::endl;
      return s;
    }

    /** Destructor */
    inline ~SparseVector() {}

    /** Overloaded for performance */
    Scalar sum() const;

  public:

    /** \deprecated use setZero() and reserve() */
    EIGEN_DEPRECATED void startFill(Index reserve)
    {
      setZero();
      m_data.reserve(reserve);
    }

    /** \deprecated use insertBack(Index,Index) */
    EIGEN_DEPRECATED Scalar& fill(Index r, Index c)
    {
      eigen_assert(r==0 || c==0);
      return fill(IsColVector ? r : c);
    }

    /** \deprecated use insertBack(Index) */
    EIGEN_DEPRECATED Scalar& fill(Index i)
    {
      m_data.append(0, i);
      return m_data.value(m_data.size()-1);
    }

    /** \deprecated use insert(Index,Index) */
    EIGEN_DEPRECATED Scalar& fillrand(Index r, Index c)
    {
      eigen_assert(r==0 || c==0);
      return fillrand(IsColVector ? r : c);
    }

    /** \deprecated use insert(Index) */
    EIGEN_DEPRECATED Scalar& fillrand(Index i)
    {
      return insert(i);
    }

    /** \deprecated use finalize() */
    EIGEN_DEPRECATED void endFill() {}
    
#   ifdef EIGEN_SPARSEVECTOR_PLUGIN
#     include EIGEN_SPARSEVECTOR_PLUGIN
#   endif

protected:
    template<typename OtherDerived>
    EIGEN_DONT_INLINE SparseVector& assign(const SparseMatrixBase<OtherDerived>& _other)
    {
      const OtherDerived& other(_other.derived());
      const bool needToTranspose = (Flags & RowMajorBit) != (OtherDerived::Flags & RowMajorBit);
      if(needToTranspose)
      {
        Index size = other.size();
        Index nnz = other.nonZeros();
        resize(size);
        reserve(nnz);
        for(Index i=0; i<size; ++i)
        {
          typename OtherDerived::InnerIterator it(other, i);
          if(it)
              insert(i) = it.value();
        }
        return *this;
      }
      else
      {
        // there is no special optimization
        return Base::operator=(other);
      }
    }
};

template<typename Scalar, int _Options, typename _Index>
class SparseVector<Scalar,_Options,_Index>::InnerIterator
{
  public:
    InnerIterator(const SparseVector& vec, Index outer=0)
      : m_data(vec.m_data), m_id(0), m_end(static_cast<Index>(m_data.size()))
    {
      EIGEN_UNUSED_VARIABLE(outer);
      eigen_assert(outer==0);
    }

    InnerIterator(const internal::CompressedStorage<Scalar,Index>& data)
      : m_data(data), m_id(0), m_end(static_cast<Index>(m_data.size()))
    {}

    inline InnerIterator& operator++() { m_id++; return *this; }

    inline Scalar value() const { return m_data.value(m_id); }
    inline Scalar& valueRef() { return const_cast<Scalar&>(m_data.value(m_id)); }

    inline Index index() const { return m_data.index(m_id); }
    inline Index row() const { return IsColVector ? index() : 0; }
    inline Index col() const { return IsColVector ? 0 : index(); }

    inline operator bool() const { return (m_id < m_end); }

  protected:
    const internal::CompressedStorage<Scalar,Index>& m_data;
    Index m_id;
    const Index m_end;
};

template<typename Scalar, int _Options, typename _Index>
class SparseVector<Scalar,_Options,_Index>::ReverseInnerIterator
{
  public:
    ReverseInnerIterator(const SparseVector& vec, Index outer=0)
      : m_data(vec.m_data), m_id(static_cast<Index>(m_data.size())), m_start(0)
    {
      EIGEN_UNUSED_VARIABLE(outer);
      eigen_assert(outer==0);
    }

    ReverseInnerIterator(const internal::CompressedStorage<Scalar,Index>& data)
      : m_data(data), m_id(static_cast<Index>(m_data.size())), m_start(0)
    {}

    inline ReverseInnerIterator& operator--() { m_id--; return *this; }

    inline Scalar value() const { return m_data.value(m_id-1); }
    inline Scalar& valueRef() { return const_cast<Scalar&>(m_data.value(m_id-1)); }

    inline Index index() const { return m_data.index(m_id-1); }
    inline Index row() const { return IsColVector ? index() : 0; }
    inline Index col() const { return IsColVector ? 0 : index(); }

    inline operator bool() const { return (m_id > m_start); }

  protected:
    const internal::CompressedStorage<Scalar,Index>& m_data;
    Index m_id;
    const Index m_start;
};

} // end namespace Eigen

#endif // EIGEN_SPARSEVECTOR_H
