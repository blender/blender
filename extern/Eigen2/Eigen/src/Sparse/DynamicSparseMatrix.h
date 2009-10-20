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

#ifndef EIGEN_DYNAMIC_SPARSEMATRIX_H
#define EIGEN_DYNAMIC_SPARSEMATRIX_H

/** \class DynamicSparseMatrix
  *
  * \brief A sparse matrix class designed for matrix assembly purpose
  *
  * \param _Scalar the scalar type, i.e. the type of the coefficients
  *
  * Unlike SparseMatrix, this class provides a much higher degree of flexibility. In particular, it allows
  * random read/write accesses in log(rho*outer_size) where \c rho is the probability that a coefficient is
  * nonzero and outer_size is the number of columns if the matrix is column-major and the number of rows
  * otherwise.
  * 
  * Internally, the data are stored as a std::vector of compressed vector. The performances of random writes might
  * decrease as the number of nonzeros per inner-vector increase. In practice, we observed very good performance
  * till about 100 nonzeros/vector, and the performance remains relatively good till 500 nonzeros/vectors.
  *
  * \see SparseMatrix
  */
template<typename _Scalar, int _Flags>
struct ei_traits<DynamicSparseMatrix<_Scalar, _Flags> >
{
  typedef _Scalar Scalar;
  enum {
    RowsAtCompileTime = Dynamic,
    ColsAtCompileTime = Dynamic,
    MaxRowsAtCompileTime = Dynamic,
    MaxColsAtCompileTime = Dynamic,
    Flags = SparseBit | _Flags,
    CoeffReadCost = NumTraits<Scalar>::ReadCost,
    SupportedAccessPatterns = OuterRandomAccessPattern
  };
};

template<typename _Scalar, int _Flags>
class DynamicSparseMatrix
  : public SparseMatrixBase<DynamicSparseMatrix<_Scalar, _Flags> >
{
  public:
    EIGEN_SPARSE_GENERIC_PUBLIC_INTERFACE(DynamicSparseMatrix)
    // FIXME: why are these operator already alvailable ???
    // EIGEN_SPARSE_INHERIT_ASSIGNMENT_OPERATOR(DynamicSparseMatrix, +=)
    // EIGEN_SPARSE_INHERIT_ASSIGNMENT_OPERATOR(DynamicSparseMatrix, -=)
    typedef MappedSparseMatrix<Scalar,Flags> Map;

  protected:

    enum { IsRowMajor = Base::IsRowMajor };
    typedef DynamicSparseMatrix<Scalar,(Flags&~RowMajorBit)|(IsRowMajor?RowMajorBit:0)> TransposedSparseMatrix;

    int m_innerSize;
    std::vector<CompressedStorage<Scalar> > m_data;

  public:
  
    inline int rows() const { return IsRowMajor ? outerSize() : m_innerSize; }
    inline int cols() const { return IsRowMajor ? m_innerSize : outerSize(); }
    inline int innerSize() const { return m_innerSize; }
    inline int outerSize() const { return m_data.size(); }
    inline int innerNonZeros(int j) const { return m_data[j].size(); }
    
    std::vector<CompressedStorage<Scalar> >& _data() { return m_data; }
    const std::vector<CompressedStorage<Scalar> >& _data() const { return m_data; }

    /** \returns the coefficient value at given position \a row, \a col
      * This operation involes a log(rho*outer_size) binary search.
      */
    inline Scalar coeff(int row, int col) const
    {
      const int outer = IsRowMajor ? row : col;
      const int inner = IsRowMajor ? col : row;
      return m_data[outer].at(inner);
    }

    /** \returns a reference to the coefficient value at given position \a row, \a col
      * This operation involes a log(rho*outer_size) binary search. If the coefficient does not
      * exist yet, then a sorted insertion into a sequential buffer is performed.
      */
    inline Scalar& coeffRef(int row, int col)
    {
      const int outer = IsRowMajor ? row : col;
      const int inner = IsRowMajor ? col : row;
      return m_data[outer].atWithInsertion(inner);
    }

    class InnerIterator;

    inline void setZero()
    {
      for (int j=0; j<outerSize(); ++j)
        m_data[j].clear();
    }

    /** \returns the number of non zero coefficients */
    inline int nonZeros() const
    {
      int res = 0;
      for (int j=0; j<outerSize(); ++j)
        res += m_data[j].size();
      return res;
    }

    /** Set the matrix to zero and reserve the memory for \a reserveSize nonzero coefficients. */
    inline void startFill(int reserveSize = 1000)
    {
      if (outerSize()>0)
      {
        int reserveSizePerVector = std::max(reserveSize/outerSize(),4);
        for (int j=0; j<outerSize(); ++j)
        {
          m_data[j].clear();
          m_data[j].reserve(reserveSizePerVector);
        }
      }
    }

    /** inserts a nonzero coefficient at given coordinates \a row, \a col and returns its reference assuming that:
      *  1 - the coefficient does not exist yet
      *  2 - this the coefficient with greater inner coordinate for the given outer coordinate.
      * In other words, assuming \c *this is column-major, then there must not exists any nonzero coefficient of coordinates
      * \c i \c x \a col such that \c i >= \a row. Otherwise the matrix is invalid.
      * 
      * \see fillrand(), coeffRef()
      */
    inline Scalar& fill(int row, int col)
    {
      const int outer = IsRowMajor ? row : col;
      const int inner = IsRowMajor ? col : row;
      ei_assert(outer<int(m_data.size()) && inner<m_innerSize);
      ei_assert((m_data[outer].size()==0) || (m_data[outer].index(m_data[outer].size()-1)<inner));
      m_data[outer].append(0, inner);
      return m_data[outer].value(m_data[outer].size()-1);
    }

    /** Like fill() but with random inner coordinates.
      * Compared to the generic coeffRef(), the unique limitation is that we assume
      * the coefficient does not exist yet.
      */
    inline Scalar& fillrand(int row, int col)
    {
      const int outer = IsRowMajor ? row : col;
      const int inner = IsRowMajor ? col : row;
      
      int startId = 0;
      int id = m_data[outer].size() - 1;
      m_data[outer].resize(id+2,1);

      while ( (id >= startId) && (m_data[outer].index(id) > inner) )
      {
        m_data[outer].index(id+1) = m_data[outer].index(id);
        m_data[outer].value(id+1) = m_data[outer].value(id);
        --id;
      }
      m_data[outer].index(id+1) = inner;
      m_data[outer].value(id+1) = 0;
      return m_data[outer].value(id+1);
    }

    /** Does nothing. Provided for compatibility with SparseMatrix. */
    inline void endFill() {}
    
    void prune(Scalar reference, RealScalar epsilon = precision<RealScalar>())
    {
      for (int j=0; j<outerSize(); ++j)
        m_data[j].prune(reference,epsilon);
    }

    /** Resize the matrix without preserving the data (the matrix is set to zero)
      */
    void resize(int rows, int cols)
    {
      const int outerSize = IsRowMajor ? rows : cols;
      m_innerSize = IsRowMajor ? cols : rows;
      setZero();
      if (int(m_data.size()) != outerSize)
      {
        m_data.resize(outerSize);
      }
    }
    
    void resizeAndKeepData(int rows, int cols)
    {
      const int outerSize = IsRowMajor ? rows : cols;
      const int innerSize = IsRowMajor ? cols : rows;
      if (m_innerSize>innerSize)
      {
        // remove all coefficients with innerCoord>=innerSize
        // TODO
        std::cerr << "not implemented yet\n";
        exit(2);
      }
      if (m_data.size() != outerSize)
      {
        m_data.resize(outerSize);
      }
    }

    inline DynamicSparseMatrix()
      : m_innerSize(0), m_data(0)
    {
      ei_assert(innerSize()==0 && outerSize()==0);
    }

    inline DynamicSparseMatrix(int rows, int cols)
      : m_innerSize(0)
    {
      resize(rows, cols);
    }

    template<typename OtherDerived>
    inline DynamicSparseMatrix(const SparseMatrixBase<OtherDerived>& other)
      : m_innerSize(0)
    {
      *this = other.derived();
    }

    inline DynamicSparseMatrix(const DynamicSparseMatrix& other)
      : Base(), m_innerSize(0)
    {
      *this = other.derived();
    }

    inline void swap(DynamicSparseMatrix& other)
    {
      //EIGEN_DBG_SPARSE(std::cout << "SparseMatrix:: swap\n");
      std::swap(m_innerSize, other.m_innerSize);
      //std::swap(m_outerSize, other.m_outerSize);
      m_data.swap(other.m_data);
    }

    inline DynamicSparseMatrix& operator=(const DynamicSparseMatrix& other)
    {
      if (other.isRValue())
      {
        swap(other.const_cast_derived());
      }
      else
      {
        resize(other.rows(), other.cols());
        m_data = other.m_data;
      }
      return *this;
    }

    template<typename OtherDerived>
    inline DynamicSparseMatrix& operator=(const SparseMatrixBase<OtherDerived>& other)
    {
      return SparseMatrixBase<DynamicSparseMatrix>::operator=(other.derived());
    }

    /** Destructor */
    inline ~DynamicSparseMatrix() {}
};

template<typename Scalar, int _Flags>
class DynamicSparseMatrix<Scalar,_Flags>::InnerIterator : public SparseVector<Scalar,_Flags>::InnerIterator
{
    typedef typename SparseVector<Scalar,_Flags>::InnerIterator Base;
  public:
    InnerIterator(const DynamicSparseMatrix& mat, int outer)
      : Base(mat.m_data[outer]), m_outer(outer)
    {}
    
    inline int row() const { return IsRowMajor ? m_outer : Base::index(); }
    inline int col() const { return IsRowMajor ? Base::index() : m_outer; }

  protected:
    const int m_outer;

  private:
    InnerIterator& operator=(const InnerIterator&);
};

#endif // EIGEN_DYNAMIC_SPARSEMATRIX_H
