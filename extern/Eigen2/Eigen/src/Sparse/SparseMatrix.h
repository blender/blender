// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2008-2009 Gael Guennebaud <g.gael@free.fr>
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

/** \class SparseMatrix
  *
  * \brief Sparse matrix
  *
  * \param _Scalar the scalar type, i.e. the type of the coefficients
  *
  * See http://www.netlib.org/linalg/html_templates/node91.html for details on the storage scheme.
  *
  */
template<typename _Scalar, int _Flags>
struct ei_traits<SparseMatrix<_Scalar, _Flags> >
{
  typedef _Scalar Scalar;
  enum {
    RowsAtCompileTime = Dynamic,
    ColsAtCompileTime = Dynamic,
    MaxRowsAtCompileTime = Dynamic,
    MaxColsAtCompileTime = Dynamic,
    Flags = SparseBit | _Flags,
    CoeffReadCost = NumTraits<Scalar>::ReadCost,
    SupportedAccessPatterns = InnerRandomAccessPattern
  };
};



template<typename _Scalar, int _Flags>
class SparseMatrix
  : public SparseMatrixBase<SparseMatrix<_Scalar, _Flags> >
{
  public:
    EIGEN_SPARSE_GENERIC_PUBLIC_INTERFACE(SparseMatrix)
    EIGEN_SPARSE_INHERIT_ASSIGNMENT_OPERATOR(SparseMatrix, +=)
    EIGEN_SPARSE_INHERIT_ASSIGNMENT_OPERATOR(SparseMatrix, -=)
    // FIXME: why are these operator already alvailable ???
    // EIGEN_SPARSE_INHERIT_SCALAR_ASSIGNMENT_OPERATOR(SparseMatrix, *=)
    // EIGEN_SPARSE_INHERIT_SCALAR_ASSIGNMENT_OPERATOR(SparseMatrix, /=)

    typedef MappedSparseMatrix<Scalar,Flags> Map;

  protected:

    enum { IsRowMajor = Base::IsRowMajor };
    typedef SparseMatrix<Scalar,(Flags&~RowMajorBit)|(IsRowMajor?RowMajorBit:0)> TransposedSparseMatrix;

    int m_outerSize;
    int m_innerSize;
    int* m_outerIndex;
    CompressedStorage<Scalar> m_data;

  public:

    inline int rows() const { return IsRowMajor ? m_outerSize : m_innerSize; }
    inline int cols() const { return IsRowMajor ? m_innerSize : m_outerSize; }

    inline int innerSize() const { return m_innerSize; }
    inline int outerSize() const { return m_outerSize; }
    inline int innerNonZeros(int j) const { return m_outerIndex[j+1]-m_outerIndex[j]; }

    inline const Scalar* _valuePtr() const { return &m_data.value(0); }
    inline Scalar* _valuePtr() { return &m_data.value(0); }

    inline const int* _innerIndexPtr() const { return &m_data.index(0); }
    inline int* _innerIndexPtr() { return &m_data.index(0); }

    inline const int* _outerIndexPtr() const { return m_outerIndex; }
    inline int* _outerIndexPtr() { return m_outerIndex; }

    inline Scalar coeff(int row, int col) const
    {
      const int outer = IsRowMajor ? row : col;
      const int inner = IsRowMajor ? col : row;
      return m_data.atInRange(m_outerIndex[outer], m_outerIndex[outer+1], inner);
    }

    inline Scalar& coeffRef(int row, int col)
    {
      const int outer = IsRowMajor ? row : col;
      const int inner = IsRowMajor ? col : row;

      int start = m_outerIndex[outer];
      int end = m_outerIndex[outer+1];
      ei_assert(end>=start && "you probably called coeffRef on a non finalized matrix");
      ei_assert(end>start && "coeffRef cannot be called on a zero coefficient");
      const int id = m_data.searchLowerIndex(start,end-1,inner);
      ei_assert((id<end) && (m_data.index(id)==inner) && "coeffRef cannot be called on a zero coefficient");
      return m_data.value(id);
    }

  public:

    class InnerIterator;

    inline void setZero()
    {
      m_data.clear();
      //if (m_outerSize)
      memset(m_outerIndex, 0, (m_outerSize+1)*sizeof(int));
//       for (int i=0; i<m_outerSize; ++i)
//         m_outerIndex[i] = 0;
//       if (m_outerSize)
//         m_outerIndex[i] = 0;
    }

    /** \returns the number of non zero coefficients */
    inline int nonZeros() const  { return m_data.size(); }

    /** Initializes the filling process of \c *this.
      * \param reserveSize approximate number of nonzeros
      * Note that the matrix \c *this is zero-ed.
      */
    inline void startFill(int reserveSize = 1000)
    {
      setZero();
      m_data.reserve(reserveSize);
    }

    /**
      */
    inline Scalar& fill(int row, int col)
    {
      const int outer = IsRowMajor ? row : col;
      const int inner = IsRowMajor ? col : row;

      if (m_outerIndex[outer+1]==0)
      {
        // we start a new inner vector
        int i = outer;
        while (i>=0 && m_outerIndex[i]==0)
        {
          m_outerIndex[i] = m_data.size();
          --i;
        }
        m_outerIndex[outer+1] = m_outerIndex[outer];
      }
      else
      {
        ei_assert(m_data.index(m_data.size()-1)<inner && "wrong sorted insertion");
      }
      assert(size_t(m_outerIndex[outer+1]) == m_data.size());
      int id = m_outerIndex[outer+1];
      ++m_outerIndex[outer+1];

      m_data.append(0, inner);
      return m_data.value(id);
    }

    /** Like fill() but with random inner coordinates.
      */
    inline Scalar& fillrand(int row, int col)
    {
      const int outer = IsRowMajor ? row : col;
      const int inner = IsRowMajor ? col : row;
      if (m_outerIndex[outer+1]==0)
      {
        // we start a new inner vector
        // nothing special to do here
        int i = outer;
        while (i>=0 && m_outerIndex[i]==0)
        {
          m_outerIndex[i] = m_data.size();
          --i;
        }
        m_outerIndex[outer+1] = m_outerIndex[outer];
      }
      assert(size_t(m_outerIndex[outer+1]) == m_data.size() && "invalid outer index");
      size_t startId = m_outerIndex[outer];
      // FIXME let's make sure sizeof(long int) == sizeof(size_t)
      size_t id = m_outerIndex[outer+1];
      ++m_outerIndex[outer+1];

      float reallocRatio = 1;
      if (m_data.allocatedSize()<id+1)
      {
        // we need to reallocate the data, to reduce multiple reallocations
        // we use a smart resize algorithm based on the current filling ratio
        // we use float to avoid overflows
        float nnzEstimate = float(m_outerIndex[outer])*float(m_outerSize)/float(outer);
        reallocRatio = (nnzEstimate-float(m_data.size()))/float(m_data.size());
        // let's bounds the realloc ratio to
        //   1) reduce multiple minor realloc when the matrix is almost filled
        //   2) avoid to allocate too much memory when the matrix is almost empty
        reallocRatio = std::min(std::max(reallocRatio,1.5f),8.f);
      }
      m_data.resize(id+1,reallocRatio);

      while ( (id > startId) && (m_data.index(id-1) > inner) )
      {
        m_data.index(id) = m_data.index(id-1);
        m_data.value(id) = m_data.value(id-1);
        --id;
      }

      m_data.index(id) = inner;
      return (m_data.value(id) = 0);
    }

    inline void endFill()
    {
      int size = m_data.size();
      int i = m_outerSize;
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

    void prune(Scalar reference, RealScalar epsilon = precision<RealScalar>())
    {
      int k = 0;
      for (int j=0; j<m_outerSize; ++j)
      {
        int previousStart = m_outerIndex[j];
        m_outerIndex[j] = k;
        int end = m_outerIndex[j+1];
        for (int i=previousStart; i<end; ++i)
        {
          if (!ei_isMuchSmallerThan(m_data.value(i), reference, epsilon))
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

    void resize(int rows, int cols)
    {
//       std::cerr << this << " resize " << rows << "x" << cols << "\n";
      const int outerSize = IsRowMajor ? rows : cols;
      m_innerSize = IsRowMajor ? cols : rows;
      m_data.clear();
      if (m_outerSize != outerSize)
      {
        delete[] m_outerIndex;
        m_outerIndex = new int [outerSize+1];
        m_outerSize = outerSize;
        memset(m_outerIndex, 0, (m_outerSize+1)*sizeof(int));
      }
    }
    void resizeNonZeros(int size)
    {
      m_data.resize(size);
    }

    inline SparseMatrix()
      : m_outerSize(-1), m_innerSize(0), m_outerIndex(0)
    {
      resize(0, 0);
    }

    inline SparseMatrix(int rows, int cols)
      : m_outerSize(0), m_innerSize(0), m_outerIndex(0)
    {
      resize(rows, cols);
    }

    template<typename OtherDerived>
    inline SparseMatrix(const SparseMatrixBase<OtherDerived>& other)
      : m_outerSize(0), m_innerSize(0), m_outerIndex(0)
    {
      *this = other.derived();
    }

    inline SparseMatrix(const SparseMatrix& other)
      : Base(), m_outerSize(0), m_innerSize(0), m_outerIndex(0)
    {
      *this = other.derived();
    }

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
        memcpy(m_outerIndex, other.m_outerIndex, (m_outerSize+1)*sizeof(int));
        m_data = other.m_data;
      }
      return *this;
    }

    template<typename OtherDerived>
    inline SparseMatrix& operator=(const SparseMatrixBase<OtherDerived>& other)
    {
      const bool needToTranspose = (Flags & RowMajorBit) != (OtherDerived::Flags & RowMajorBit);
      if (needToTranspose)
      {
        // two passes algorithm:
        //  1 - compute the number of coeffs per dest inner vector
        //  2 - do the actual copy/eval
        // Since each coeff of the rhs has to be evaluated twice, let's evauluate it if needed
        //typedef typename ei_nested<OtherDerived,2>::type OtherCopy;
        typedef typename ei_eval<OtherDerived>::type OtherCopy;
        typedef typename ei_cleantype<OtherCopy>::type _OtherCopy;
        OtherCopy otherCopy(other.derived());

        resize(other.rows(), other.cols());
        Eigen::Map<VectorXi>(m_outerIndex,outerSize()).setZero();
        // pass 1
        // FIXME the above copy could be merged with that pass
        for (int j=0; j<otherCopy.outerSize(); ++j)
          for (typename _OtherCopy::InnerIterator it(otherCopy, j); it; ++it)
            ++m_outerIndex[it.index()];

        // prefix sum
        int count = 0;
        VectorXi positions(outerSize());
        for (int j=0; j<outerSize(); ++j)
        {
          int tmp = m_outerIndex[j];
          m_outerIndex[j] = count;
          positions[j] = count;
          count += tmp;
        }
        m_outerIndex[outerSize()] = count;
        // alloc
        m_data.resize(count);
        // pass 2
        for (int j=0; j<otherCopy.outerSize(); ++j)
          for (typename _OtherCopy::InnerIterator it(otherCopy, j); it; ++it)
          {
            int pos = positions[it.index()]++;
            m_data.index(pos) = j;
            m_data.value(pos) = it.value();
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
        for (int i=0; i<m.nonZeros(); ++i)
        {
          s << "(" << m.m_data.value(i) << "," << m.m_data.index(i) << ") ";
        }
        s << std::endl;
        s << std::endl;
        s << "Column pointers:\n";
        for (int i=0; i<m.outerSize(); ++i)
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
};

template<typename Scalar, int _Flags>
class SparseMatrix<Scalar,_Flags>::InnerIterator
{
  public:
    InnerIterator(const SparseMatrix& mat, int outer)
      : m_matrix(mat), m_outer(outer), m_id(mat.m_outerIndex[outer]), m_start(m_id), m_end(mat.m_outerIndex[outer+1])
    {}

    template<unsigned int Added, unsigned int Removed>
    InnerIterator(const Flagged<SparseMatrix,Added,Removed>& mat, int outer)
      : m_matrix(mat._expression()), m_outer(outer), m_id(m_matrix.m_outerIndex[outer]),
        m_start(m_id), m_end(m_matrix.m_outerIndex[outer+1])
    {}

    inline InnerIterator& operator++() { m_id++; return *this; }

    inline Scalar value() const { return m_matrix.m_data.value(m_id); }
    inline Scalar& valueRef() { return const_cast<Scalar&>(m_matrix.m_data.value(m_id)); }

    inline int index() const { return m_matrix.m_data.index(m_id); }
    inline int row() const { return IsRowMajor ? m_outer : index(); }
    inline int col() const { return IsRowMajor ? index() : m_outer; }

    inline operator bool() const { return (m_id < m_end) && (m_id>=m_start); }

  protected:
    const SparseMatrix& m_matrix;
    const int m_outer;
    int m_id;
    const int m_start;
    const int m_end;
};

#endif // EIGEN_SPARSEMATRIX_H
