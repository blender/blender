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

#ifndef EIGEN_SPARSEVECTOR_H
#define EIGEN_SPARSEVECTOR_H

/** \class SparseVector
  *
  * \brief a sparse vector class
  *
  * \param _Scalar the scalar type, i.e. the type of the coefficients
  *
  * See http://www.netlib.org/linalg/html_templates/node91.html for details on the storage scheme.
  *
  */
template<typename _Scalar, int _Flags>
struct ei_traits<SparseVector<_Scalar, _Flags> >
{
  typedef _Scalar Scalar;
  enum {
    IsColVector = _Flags & RowMajorBit ? 0 : 1,

    RowsAtCompileTime = IsColVector ? Dynamic : 1,
    ColsAtCompileTime = IsColVector ? 1 : Dynamic,
    MaxRowsAtCompileTime = RowsAtCompileTime,
    MaxColsAtCompileTime = ColsAtCompileTime,
    Flags = SparseBit | _Flags,
    CoeffReadCost = NumTraits<Scalar>::ReadCost,
    SupportedAccessPatterns = InnerRandomAccessPattern
  };
};

template<typename _Scalar, int _Flags>
class SparseVector
  : public SparseMatrixBase<SparseVector<_Scalar, _Flags> >
{
  public:
    EIGEN_SPARSE_GENERIC_PUBLIC_INTERFACE(SparseVector)
    EIGEN_SPARSE_INHERIT_ASSIGNMENT_OPERATOR(SparseVector, +=)
    EIGEN_SPARSE_INHERIT_ASSIGNMENT_OPERATOR(SparseVector, -=)
//     EIGEN_SPARSE_INHERIT_ASSIGNMENT_OPERATOR(SparseVector, =)

  protected:
  public:

    typedef SparseMatrixBase<SparseVector> SparseBase;
    enum { IsColVector = ei_traits<SparseVector>::IsColVector };

    CompressedStorage<Scalar> m_data;
    int m_size;
    
    CompressedStorage<Scalar>& _data() { return m_data; }
    CompressedStorage<Scalar>& _data() const { return m_data; }

  public:

    EIGEN_STRONG_INLINE int rows() const { return IsColVector ? m_size : 1; }
    EIGEN_STRONG_INLINE int cols() const { return IsColVector ? 1 : m_size; }
    EIGEN_STRONG_INLINE int innerSize() const { return m_size; }
    EIGEN_STRONG_INLINE int outerSize() const { return 1; }
    EIGEN_STRONG_INLINE int innerNonZeros(int j) const { ei_assert(j==0); return m_size; }

    EIGEN_STRONG_INLINE const Scalar* _valuePtr() const { return &m_data.value(0); }
    EIGEN_STRONG_INLINE Scalar* _valuePtr() { return &m_data.value(0); }

    EIGEN_STRONG_INLINE const int* _innerIndexPtr() const { return &m_data.index(0); }
    EIGEN_STRONG_INLINE int* _innerIndexPtr() { return &m_data.index(0); }

    inline Scalar coeff(int row, int col) const
    {
      ei_assert((IsColVector ? col : row)==0);
      return coeff(IsColVector ? row : col);
    }
    inline Scalar coeff(int i) const { return m_data.at(i); }

    inline Scalar& coeffRef(int row, int col)
    {
      ei_assert((IsColVector ? col : row)==0);
      return coeff(IsColVector ? row : col);
    }

    /** \returns a reference to the coefficient value at given index \a i
      * This operation involes a log(rho*size) binary search. If the coefficient does not
      * exist yet, then a sorted insertion into a sequential buffer is performed.
      * 
      * This insertion might be very costly if the number of nonzeros above \a i is large.
      */
    inline Scalar& coeffRef(int i)
    {
      return m_data.atWithInsertion(i);
    }

  public:

    class InnerIterator;

    inline void setZero() { m_data.clear(); }

    /** \returns the number of non zero coefficients */
    inline int nonZeros() const  { return m_data.size(); }

    /**
      */
    inline void reserve(int reserveSize) { m_data.reserve(reserveSize); }
    
    inline void startFill(int reserve)
    {
      setZero();
      m_data.reserve(reserve);
    }

    /**
      */
    inline Scalar& fill(int r, int c)
    {
      ei_assert(r==0 || c==0);
      return fill(IsColVector ? r : c);
    }
    
    inline Scalar& fill(int i)
    {
      m_data.append(0, i);
      return m_data.value(m_data.size()-1);
    }
    
    inline Scalar& fillrand(int r, int c)
    {
      ei_assert(r==0 || c==0);
      return fillrand(IsColVector ? r : c);
    }

    /** Like fill() but with random coordinates.
      */
    inline Scalar& fillrand(int i)
    {
      int startId = 0;
      int id = m_data.size() - 1;
      m_data.resize(id+2,1);

      while ( (id >= startId) && (m_data.index(id) > i) )
      {
        m_data.index(id+1) = m_data.index(id);
        m_data.value(id+1) = m_data.value(id);
        --id;
      }
      m_data.index(id+1) = i;
      m_data.value(id+1) = 0;
      return m_data.value(id+1);
    }
    
    inline void endFill() {}
    
    void prune(Scalar reference, RealScalar epsilon = precision<RealScalar>())
    {
      m_data.prune(reference,epsilon);
    }
    
    void resize(int rows, int cols)
    {
      ei_assert(rows==1 || cols==1);
      resize(IsColVector ? rows : cols);
    }

    void resize(int newSize)
    {
      m_size = newSize;
      m_data.clear();
    }

    void resizeNonZeros(int size) { m_data.resize(size); }

    inline SparseVector() : m_size(0) { resize(0); }

    inline SparseVector(int size) : m_size(0) { resize(size); }
    
    inline SparseVector(int rows, int cols) : m_size(0) { resize(rows,cols); }

    template<typename OtherDerived>
    inline SparseVector(const MatrixBase<OtherDerived>& other)
      : m_size(0)
    {
      *this = other.derived();
    }
    
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
      return Base::operator=(other);
    }
    
//       const bool needToTranspose = (Flags & RowMajorBit) != (OtherDerived::Flags & RowMajorBit);
//       if (needToTranspose)
//       {
//         // two passes algorithm:
//         //  1 - compute the number of coeffs per dest inner vector
//         //  2 - do the actual copy/eval
//         // Since each coeff of the rhs has to be evaluated twice, let's evauluate it if needed
//         typedef typename ei_nested<OtherDerived,2>::type OtherCopy;
//         OtherCopy otherCopy(other.derived());
//         typedef typename ei_cleantype<OtherCopy>::type _OtherCopy;
//
//         resize(other.rows(), other.cols());
//         Eigen::Map<VectorXi>(m_outerIndex,outerSize()).setZero();
//         // pass 1
//         // FIXME the above copy could be merged with that pass
//         for (int j=0; j<otherCopy.outerSize(); ++j)
//           for (typename _OtherCopy::InnerIterator it(otherCopy, j); it; ++it)
//             ++m_outerIndex[it.index()];
//
//         // prefix sum
//         int count = 0;
//         VectorXi positions(outerSize());
//         for (int j=0; j<outerSize(); ++j)
//         {
//           int tmp = m_outerIndex[j];
//           m_outerIndex[j] = count;
//           positions[j] = count;
//           count += tmp;
//         }
//         m_outerIndex[outerSize()] = count;
//         // alloc
//         m_data.resize(count);
//         // pass 2
//         for (int j=0; j<otherCopy.outerSize(); ++j)
//           for (typename _OtherCopy::InnerIterator it(otherCopy, j); it; ++it)
//           {
//             int pos = positions[it.index()]++;
//             m_data.index(pos) = j;
//             m_data.value(pos) = it.value();
//           }
//
//         return *this;
//       }
//       else
//       {
//         // there is no special optimization
//         return SparseMatrixBase<SparseMatrix>::operator=(other.derived());
//       }
//     }

    friend std::ostream & operator << (std::ostream & s, const SparseVector& m)
    {
      for (unsigned int i=0; i<m.nonZeros(); ++i)
        s << "(" << m.m_data.value(i) << "," << m.m_data.index(i) << ") ";
      s << std::endl;
      return s;
    }

    // this specialized version does not seems to be faster
//     Scalar dot(const SparseVector& other) const
//     {
//       int i=0, j=0;
//       Scalar res = 0;
//       asm("#begindot");
//       while (i<nonZeros() && j<other.nonZeros())
//       {
//         if (m_data.index(i)==other.m_data.index(j))
//         {
//           res += m_data.value(i) * ei_conj(other.m_data.value(j));
//           ++i; ++j;
//         }
//         else if (m_data.index(i)<other.m_data.index(j))
//           ++i;
//         else
//           ++j;
//       }
//       asm("#enddot");
//       return res;
//     }

    /** Destructor */
    inline ~SparseVector() {}
};

template<typename Scalar, int _Flags>
class SparseVector<Scalar,_Flags>::InnerIterator
{
  public:
    InnerIterator(const SparseVector& vec, int outer=0)
      : m_data(vec.m_data), m_id(0), m_end(m_data.size())
    {
      ei_assert(outer==0);
    }
    
    InnerIterator(const CompressedStorage<Scalar>& data)
      : m_data(data), m_id(0), m_end(m_data.size())
    {}

    template<unsigned int Added, unsigned int Removed>
    InnerIterator(const Flagged<SparseVector,Added,Removed>& vec, int outer)
      : m_data(vec._expression().m_data), m_id(0), m_end(m_data.size())
    {}

    inline InnerIterator& operator++() { m_id++; return *this; }

    inline Scalar value() const { return m_data.value(m_id); }
    inline Scalar& valueRef() { return const_cast<Scalar&>(m_data.value(m_id)); }

    inline int index() const { return m_data.index(m_id); }
    inline int row() const { return IsColVector ? index() : 0; }
    inline int col() const { return IsColVector ? 0 : index(); }

    inline operator bool() const { return (m_id < m_end); }

  protected:
    const CompressedStorage<Scalar>& m_data;
    int m_id;
    const int m_end;

  private:
    InnerIterator& operator=(const InnerIterator&);
};

#endif // EIGEN_SPARSEVECTOR_H
