// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
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

#ifndef EIGEN_SPARSEMATRIX_H
#define EIGEN_SPARSEMATRIX_H

/** \ingroup Sparse_Module
  *
  * \class SparseMatrix
  *
  * \brief The main sparse matrix class
  *
  * This class implements a sparse matrix using the very common compressed row/column storage
  * scheme.
  *
  * \tparam _Scalar the scalar type, i.e. the type of the coefficients
  * \tparam _Options Union of bit flags controlling the storage scheme. Currently the only possibility
  *                 is RowMajor. The default is 0 which means column-major.
  * \tparam _Index the type of the indices. Default is \c int.
  *
  * See http://www.netlib.org/linalg/html_templates/node91.html for details on the storage scheme.
  *
  * This class can be extended with the help of the plugin mechanism described on the page
  * \ref TopicCustomizingEigen by defining the preprocessor symbol \c EIGEN_SPARSEMATRIX_PLUGIN.
  */

namespace internal {
template<typename _Scalar, int _Options, typename _Index>
struct traits<SparseMatrix<_Scalar, _Options, _Index> >
{
  typedef _Scalar Scalar;
  typedef _Index Index;
  typedef Sparse StorageKind;
  typedef MatrixXpr XprKind;
  enum {
    RowsAtCompileTime = Dynamic,
    ColsAtCompileTime = Dynamic,
    MaxRowsAtCompileTime = Dynamic,
    MaxColsAtCompileTime = Dynamic,
    Flags = _Options | NestByRefBit | LvalueBit,
    CoeffReadCost = NumTraits<Scalar>::ReadCost,
    SupportedAccessPatterns = InnerRandomAccessPattern
  };
};

} // end namespace internal

template<typename _Scalar, int _Options, typename _Index>
class SparseMatrix
  : public SparseMatrixBase<SparseMatrix<_Scalar, _Options, _Index> >
{
  public:
    EIGEN_SPARSE_PUBLIC_INTERFACE(SparseMatrix)
//     using Base::operator=;
    EIGEN_SPARSE_INHERIT_ASSIGNMENT_OPERATOR(SparseMatrix, +=)
    EIGEN_SPARSE_INHERIT_ASSIGNMENT_OPERATOR(SparseMatrix, -=)
    // FIXME: why are these operator already alvailable ???
    // EIGEN_SPARSE_INHERIT_SCALAR_ASSIGNMENT_OPERATOR(SparseMatrix, *=)
    // EIGEN_SPARSE_INHERIT_SCALAR_ASSIGNMENT_OPERATOR(SparseMatrix, /=)

    typedef MappedSparseMatrix<Scalar,Flags> Map;
    using Base::IsRowMajor;
    typedef CompressedStorage<Scalar,Index> Storage;
    enum {
      Options = _Options
    };

  protected:

    typedef SparseMatrix<Scalar,(Flags&~RowMajorBit)|(IsRowMajor?RowMajorBit:0)> TransposedSparseMatrix;

    Index m_outerSize;
    Index m_innerSize;
    Index* m_outerIndex;
    CompressedStorage<Scalar,Index> m_data;

  public:

    inline Index rows() const { return IsRowMajor ? m_outerSize : m_innerSize; }
    inline Index cols() const { return IsRowMajor ? m_innerSize : m_outerSize; }

    inline Index innerSize() const { return m_innerSize; }
    inline Index outerSize() const { return m_outerSize; }
    inline Index innerNonZeros(Index j) const { return m_outerIndex[j+1]-m_outerIndex[j]; }

    inline const Scalar* _valuePtr() const { return &m_data.value(0); }
    inline Scalar* _valuePtr() { return &m_data.value(0); }

    inline const Index* _innerIndexPtr() const { return &m_data.index(0); }
    inline Index* _innerIndexPtr() { return &m_data.index(0); }

    inline const Index* _outerIndexPtr() const { return m_outerIndex; }
    inline Index* _outerIndexPtr() { return m_outerIndex; }

    inline Storage& data() { return m_data; }
    inline const Storage& data() const { return m_data; }

    inline Scalar coeff(Index row, Index col) const
    {
      const Index outer = IsRowMajor ? row : col;
      const Index inner = IsRowMajor ? col : row;
      return m_data.atInRange(m_outerIndex[outer], m_outerIndex[outer+1], inner);
    }

    inline Scalar& coeffRef(Index row, Index col)
    {
      const Index outer = IsRowMajor ? row : col;
      const Index inner = IsRowMajor ? col : row;

      Index start = m_outerIndex[outer];
      Index end = m_outerIndex[outer+1];
      eigen_assert(end>=start && "you probably called coeffRef on a non finalized matrix");
      eigen_assert(end>start && "coeffRef cannot be called on a zero coefficient");
      const Index p = m_data.searchLowerIndex(start,end-1,inner);
      eigen_assert((p<end) && (m_data.index(p)==inner) && "coeffRef cannot be called on a zero coefficient");
      return m_data.value(p);
    }

  public:

    class InnerIterator;

    /** Removes all non zeros */
    inline void setZero()
    {
      m_data.clear();
      memset(m_outerIndex, 0, (m_outerSize+1)*sizeof(Index));
    }

    /** \returns the number of non zero coefficients */
    inline Index nonZeros() const  { return static_cast<Index>(m_data.size()); }

    /** Preallocates \a reserveSize non zeros */
    inline void reserve(Index reserveSize)
    {
      m_data.reserve(reserveSize);
    }

    //--- low level purely coherent filling ---

    /** \returns a reference to the non zero coefficient at position \a row, \a col assuming that:
      * - the nonzero does not already exist
      * - the new coefficient is the last one according to the storage order
      *
      * Before filling a given inner vector you must call the statVec(Index) function.
      *
      * After an insertion session, you should call the finalize() function.
      *
      * \sa insert, insertBackByOuterInner, startVec */
    inline Scalar& insertBack(Index row, Index col)
    {
      return insertBackByOuterInner(IsRowMajor?row:col, IsRowMajor?col:row);
    }

    /** \sa insertBack, startVec */
    inline Scalar& insertBackByOuterInner(Index outer, Index inner)
    {
      eigen_assert(size_t(m_outerIndex[outer+1]) == m_data.size() && "Invalid ordered insertion (invalid outer index)");
      eigen_assert( (m_outerIndex[outer+1]-m_outerIndex[outer]==0 || m_data.index(m_data.size()-1)<inner) && "Invalid ordered insertion (invalid inner index)");
      Index p = m_outerIndex[outer+1];
      ++m_outerIndex[outer+1];
      m_data.append(0, inner);
      return m_data.value(p);
    }

    /** \warning use it only if you know what you are doing */
    inline Scalar& insertBackByOuterInnerUnordered(Index outer, Index inner)
    {
      Index p = m_outerIndex[outer+1];
      ++m_outerIndex[outer+1];
      m_data.append(0, inner);
      return m_data.value(p);
    }

    /** \sa insertBack, insertBackByOuterInner */
    inline void startVec(Index outer)
    {
      eigen_assert(m_outerIndex[outer]==int(m_data.size()) && "You must call startVec for each inner vector sequentially");
      eigen_assert(m_outerIndex[outer+1]==0 && "You must call startVec for each inner vector sequentially");
      m_outerIndex[outer+1] = m_outerIndex[outer];
    }

    //---

    /** \returns a reference to a novel non zero coefficient with coordinates \a row x \a col.
      * The non zero coefficient must \b not already exist.
      *
      * \warning This function can be extremely slow if the non zero coefficients
      * are not inserted in a coherent order.
      *
      * After an insertion session, you should call the finalize() function.
      */
    EIGEN_DONT_INLINE Scalar& insert(Index row, Index col)
    {
      const Index outer = IsRowMajor ? row : col;
      const Index inner = IsRowMajor ? col : row;

      Index previousOuter = outer;
      if (m_outerIndex[outer+1]==0)
      {
        // we start a new inner vector
        while (previousOuter>=0 && m_outerIndex[previousOuter]==0)
        {
          m_outerIndex[previousOuter] = static_cast<Index>(m_data.size());
          --previousOuter;
        }
        m_outerIndex[outer+1] = m_outerIndex[outer];
      }

      // here we have to handle the tricky case where the outerIndex array
      // starts with: [ 0 0 0 0 0 1 ...] and we are inserting in, e.g.,
      // the 2nd inner vector...
      bool isLastVec = (!(previousOuter==-1 && m_data.size()!=0))
                    && (size_t(m_outerIndex[outer+1]) == m_data.size());

      size_t startId = m_outerIndex[outer];
      // FIXME let's make sure sizeof(long int) == sizeof(size_t)
      size_t p = m_outerIndex[outer+1];
      ++m_outerIndex[outer+1];

      float reallocRatio = 1;
      if (m_data.allocatedSize()<=m_data.size())
      {
        // if there is no preallocated memory, let's reserve a minimum of 32 elements
        if (m_data.size()==0)
        {
          m_data.reserve(32);
        }
        else
        {
          // we need to reallocate the data, to reduce multiple reallocations
          // we use a smart resize algorithm based on the current filling ratio
          // in addition, we use float to avoid integers overflows
          float nnzEstimate = float(m_outerIndex[outer])*float(m_outerSize)/float(outer+1);
          reallocRatio = (nnzEstimate-float(m_data.size()))/float(m_data.size());
          // furthermore we bound the realloc ratio to:
          //   1) reduce multiple minor realloc when the matrix is almost filled
          //   2) avoid to allocate too much memory when the matrix is almost empty
          reallocRatio = (std::min)((std::max)(reallocRatio,1.5f),8.f);
        }
      }
      m_data.resize(m_data.size()+1,reallocRatio);

      if (!isLastVec)
      {
        if (previousOuter==-1)
        {
          // oops wrong guess.
          // let's correct the outer offsets
          for (Index k=0; k<=(outer+1); ++k)
            m_outerIndex[k] = 0;
          Index k=outer+1;
          while(m_outerIndex[k]==0)
            m_outerIndex[k++] = 1;
          while (k<=m_outerSize && m_outerIndex[k]!=0)
            m_outerIndex[k++]++;
          p = 0;
          --k;
          k = m_outerIndex[k]-1;
          while (k>0)
          {
            m_data.index(k) = m_data.index(k-1);
            m_data.value(k) = m_data.value(k-1);
            k--;
          }
        }
        else
        {
          // we are not inserting into the last inner vec
          // update outer indices:
          Index j = outer+2;
          while (j<=m_outerSize && m_outerIndex[j]!=0)
            m_outerIndex[j++]++;
          --j;
          // shift data of last vecs:
          Index k = m_outerIndex[j]-1;
          while (k>=Index(p))
          {
            m_data.index(k) = m_data.index(k-1);
            m_data.value(k) = m_data.value(k-1);
            k--;
          }
        }
      }

      while ( (p > startId) && (m_data.index(p-1) > inner) )
      {
        m_data.index(p) = m_data.index(p-1);
        m_data.value(p) = m_data.value(p-1);
        --p;
      }

      m_data.index(p) = inner;
      return (m_data.value(p) = 0);
    }




    /** Must be called after inserting a set of non zero entries.
      */
    inline void finalize()
    {
      Index size = static_cast<Index>(m_data.size());
      Index i = m_outerSize;
      // find the last filled column
      while (i>=0 && m_outerIndex[i]==0)
        --i;
      ++i;
      while (i<=m_outerSize)
      {
        m_outerIndex[i] = size;
        ++i;
      }
    }

    /** Suppress all nonzeros which are smaller than \a reference under the tolerence \a epsilon */
    void prune(Scalar reference, RealScalar epsilon = NumTraits<RealScalar>::dummy_precision())
    {
      prune(default_prunning_func(reference,epsilon));
    }
    
    /** Suppress all nonzeros which do not satisfy the predicate \a keep.
      * The functor type \a KeepFunc must implement the following function:
      * \code
      * bool operator() (const Index& row, const Index& col, const Scalar& value) const;
      * \endcode
      * \sa prune(Scalar,RealScalar)
      */
    template<typename KeepFunc>
    void prune(const KeepFunc& keep = KeepFunc())
    {
      Index k = 0;
      for(Index j=0; j<m_outerSize; ++j)
      {
        Index previousStart = m_outerIndex[j];
        m_outerIndex[j] = k;
        Index end = m_outerIndex[j+1];
        for(Index i=previousStart; i<end; ++i)
        {
          if(keep(IsRowMajor?j:m_data.index(i), IsRowMajor?m_data.index(i):j, m_data.value(i)))
          {
            m_data.value(k) = m_data.value(i);
            m_data.index(k) = m_data.index(i);
            ++k;
          }
        }
      }
      m_outerIndex[m_outerSize] = k;
      m_data.resize(k,0);
    }

    /** Resizes the matrix to a \a rows x \a cols matrix and initializes it to zero
      * \sa resizeNonZeros(Index), reserve(), setZero()
      */
    void resize(Index rows, Index cols)
    {
      const Index outerSize = IsRowMajor ? rows : cols;
      m_innerSize = IsRowMajor ? cols : rows;
      m_data.clear();
      if (m_outerSize != outerSize || m_outerSize==0)
      {
        delete[] m_outerIndex;
        m_outerIndex = new Index [outerSize+1];
        m_outerSize = outerSize;
      }
      memset(m_outerIndex, 0, (m_outerSize+1)*sizeof(Index));
    }

    /** Low level API
      * Resize the nonzero vector to \a size */
    void resizeNonZeros(Index size)
    {
      m_data.resize(size);
    }

    /** Default constructor yielding an empty \c 0 \c x \c 0 matrix */
    inline SparseMatrix()
      : m_outerSize(-1), m_innerSize(0), m_outerIndex(0)
    {
      resize(0, 0);
    }

    /** Constructs a \a rows \c x \a cols empty matrix */
    inline SparseMatrix(Index rows, Index cols)
      : m_outerSize(0), m_innerSize(0), m_outerIndex(0)
    {
      resize(rows, cols);
    }

    /** Constructs a sparse matrix from the sparse expression \a other */
    template<typename OtherDerived>
    inline SparseMatrix(const SparseMatrixBase<OtherDerived>& other)
      : m_outerSize(0), m_innerSize(0), m_outerIndex(0)
    {
      *this = other.derived();
    }

    /** Copy constructor */
    inline SparseMatrix(const SparseMatrix& other)
      : Base(), m_outerSize(0), m_innerSize(0), m_outerIndex(0)
    {
      *this = other.derived();
    }

    /** Swap the content of two sparse matrices of same type (optimization) */
    inline void swap(SparseMatrix& other)
    {
      //EIGEN_DBG_SPARSE(std::cout << "SparseMatrix:: swap\n");
      std::swap(m_outerIndex, other.m_outerIndex);
      std::swap(m_innerSize, other.m_innerSize);
      std::swap(m_outerSize, other.m_outerSize);
      m_data.swap(other.m_data);
    }

    inline SparseMatrix& operator=(const SparseMatrix& other)
    {
//       std::cout << "SparseMatrix& operator=(const SparseMatrix& other)\n";
      if (other.isRValue())
      {
        swap(other.const_cast_derived());
      }
      else
      {
        resize(other.rows(), other.cols());
        memcpy(m_outerIndex, other.m_outerIndex, (m_outerSize+1)*sizeof(Index));
        m_data = other.m_data;
      }
      return *this;
    }

    #ifndef EIGEN_PARSED_BY_DOXYGEN
    template<typename Lhs, typename Rhs>
    inline SparseMatrix& operator=(const SparseSparseProduct<Lhs,Rhs>& product)
    { return Base::operator=(product); }
    
    template<typename OtherDerived>
    inline SparseMatrix& operator=(const ReturnByValue<OtherDerived>& other)
    { return Base::operator=(other); }
    
    template<typename OtherDerived>
    inline SparseMatrix& operator=(const EigenBase<OtherDerived>& other)
    { return Base::operator=(other); }
    #endif

    template<typename OtherDerived>
    EIGEN_DONT_INLINE SparseMatrix& operator=(const SparseMatrixBase<OtherDerived>& other)
    {
      const bool needToTranspose = (Flags & RowMajorBit) != (OtherDerived::Flags & RowMajorBit);
      if (needToTranspose)
      {
        // two passes algorithm:
        //  1 - compute the number of coeffs per dest inner vector
        //  2 - do the actual copy/eval
        // Since each coeff of the rhs has to be evaluated twice, let's evaluate it if needed
        typedef typename internal::nested<OtherDerived,2>::type OtherCopy;
        typedef typename internal::remove_all<OtherCopy>::type _OtherCopy;
        OtherCopy otherCopy(other.derived());

        resize(other.rows(), other.cols());
        Eigen::Map<Matrix<Index, Dynamic, 1> > (m_outerIndex,outerSize()).setZero();
        // pass 1
        // FIXME the above copy could be merged with that pass
        for (Index j=0; j<otherCopy.outerSize(); ++j)
          for (typename _OtherCopy::InnerIterator it(otherCopy, j); it; ++it)
            ++m_outerIndex[it.index()];

        // prefix sum
        Index count = 0;
        VectorXi positions(outerSize());
        for (Index j=0; j<outerSize(); ++j)
        {
          Index tmp = m_outerIndex[j];
          m_outerIndex[j] = count;
          positions[j] = count;
          count += tmp;
        }
        m_outerIndex[outerSize()] = count;
        // alloc
        m_data.resize(count);
        // pass 2
        for (Index j=0; j<otherCopy.outerSize(); ++j)
        {
          for (typename _OtherCopy::InnerIterator it(otherCopy, j); it; ++it)
          {
            Index pos = positions[it.index()]++;
            m_data.index(pos) = j;
            m_data.value(pos) = it.value();
          }
        }
        return *this;
      }
      else
      {
        // there is no special optimization
        return SparseMatrixBase<SparseMatrix>::operator=(other.derived());
      }
    }

    friend std::ostream & operator << (std::ostream & s, const SparseMatrix& m)
    {
      EIGEN_DBG_SPARSE(
        s << "Nonzero entries:\n";
        for (Index i=0; i<m.nonZeros(); ++i)
        {
          s << "(" << m.m_data.value(i) << "," << m.m_data.index(i) << ") ";
        }
        s << std::endl;
        s << std::endl;
        s << "Column pointers:\n";
        for (Index i=0; i<m.outerSize(); ++i)
        {
          s << m.m_outerIndex[i] << " ";
        }
        s << " $" << std::endl;
        s << std::endl;
      );
      s << static_cast<const SparseMatrixBase<SparseMatrix>&>(m);
      return s;
    }

    /** Destructor */
    inline ~SparseMatrix()
    {
      delete[] m_outerIndex;
    }

    /** Overloaded for performance */
    Scalar sum() const;

  public:

    /** \deprecated use setZero() and reserve()
      * Initializes the filling process of \c *this.
      * \param reserveSize approximate number of nonzeros
      * Note that the matrix \c *this is zero-ed.
      */
    EIGEN_DEPRECATED void startFill(Index reserveSize = 1000)
    {
      setZero();
      m_data.reserve(reserveSize);
    }

    /** \deprecated use insert()
      * Like fill() but with random inner coordinates.
      */
    EIGEN_DEPRECATED Scalar& fillrand(Index row, Index col)
    {
      return insert(row,col);
    }

    /** \deprecated use insert()
      */
    EIGEN_DEPRECATED Scalar& fill(Index row, Index col)
    {
      const Index outer = IsRowMajor ? row : col;
      const Index inner = IsRowMajor ? col : row;

      if (m_outerIndex[outer+1]==0)
      {
        // we start a new inner vector
        Index i = outer;
        while (i>=0 && m_outerIndex[i]==0)
        {
          m_outerIndex[i] = m_data.size();
          --i;
        }
        m_outerIndex[outer+1] = m_outerIndex[outer];
      }
      else
      {
        eigen_assert(m_data.index(m_data.size()-1)<inner && "wrong sorted insertion");
      }
//       std::cerr << size_t(m_outerIndex[outer+1]) << " == " << m_data.size() << "\n";
      assert(size_t(m_outerIndex[outer+1]) == m_data.size());
      Index p = m_outerIndex[outer+1];
      ++m_outerIndex[outer+1];

      m_data.append(0, inner);
      return m_data.value(p);
    }

    /** \deprecated use finalize */
    EIGEN_DEPRECATED void endFill() { finalize(); }
    
#   ifdef EIGEN_SPARSEMATRIX_PLUGIN
#     include EIGEN_SPARSEMATRIX_PLUGIN
#   endif

private:
  struct default_prunning_func {
    default_prunning_func(Scalar ref, RealScalar eps) : reference(ref), epsilon(eps) {}
    inline bool operator() (const Index&, const Index&, const Scalar& value) const
    {
      return !internal::isMuchSmallerThan(value, reference, epsilon);
    }
    Scalar reference;
    RealScalar epsilon;
  };
};

template<typename Scalar, int _Options, typename _Index>
class SparseMatrix<Scalar,_Options,_Index>::InnerIterator
{
  public:
    InnerIterator(const SparseMatrix& mat, Index outer)
      : m_values(mat._valuePtr()), m_indices(mat._innerIndexPtr()), m_outer(outer), m_id(mat.m_outerIndex[outer]), m_end(mat.m_outerIndex[outer+1])
    {}

    inline InnerIterator& operator++() { m_id++; return *this; }

    inline const Scalar& value() const { return m_values[m_id]; }
    inline Scalar& valueRef() { return const_cast<Scalar&>(m_values[m_id]); }

    inline Index index() const { return m_indices[m_id]; }
    inline Index outer() const { return m_outer; }
    inline Index row() const { return IsRowMajor ? m_outer : index(); }
    inline Index col() const { return IsRowMajor ? index() : m_outer; }

    inline operator bool() const { return (m_id < m_end); }

  protected:
    const Scalar* m_values;
    const Index* m_indices;
    const Index m_outer;
    Index m_id;
    const Index m_end;
};

#endif // EIGEN_SPARSEMATRIX_H
