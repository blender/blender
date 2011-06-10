// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
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

#ifndef EIGEN_MAPPED_SPARSEMATRIX_H
#define EIGEN_MAPPED_SPARSEMATRIX_H

/** \class MappedSparseMatrix
  *
  * \brief Sparse matrix
  *
  * \param _Scalar the scalar type, i.e. the type of the coefficients
  *
  * See http://www.netlib.org/linalg/html_templates/node91.html for details on the storage scheme.
  *
  */
namespace internal {
template<typename _Scalar, int _Flags, typename _Index>
struct traits<MappedSparseMatrix<_Scalar, _Flags, _Index> > : traits<SparseMatrix<_Scalar, _Flags, _Index> >
{};
}

template<typename _Scalar, int _Flags, typename _Index>
class MappedSparseMatrix
  : public SparseMatrixBase<MappedSparseMatrix<_Scalar, _Flags, _Index> >
{
  public:
    EIGEN_SPARSE_PUBLIC_INTERFACE(MappedSparseMatrix)

  protected:
    enum { IsRowMajor = Base::IsRowMajor };

    Index   m_outerSize;
    Index   m_innerSize;
    Index   m_nnz;
    Index*  m_outerIndex;
    Index*  m_innerIndices;
    Scalar* m_values;

  public:

    inline Index rows() const { return IsRowMajor ? m_outerSize : m_innerSize; }
    inline Index cols() const { return IsRowMajor ? m_innerSize : m_outerSize; }
    inline Index innerSize() const { return m_innerSize; }
    inline Index outerSize() const { return m_outerSize; }
    inline Index innerNonZeros(Index j) const { return m_outerIndex[j+1]-m_outerIndex[j]; }

    //----------------------------------------
    // direct access interface
    inline const Scalar* _valuePtr() const { return m_values; }
    inline Scalar* _valuePtr() { return m_values; }

    inline const Index* _innerIndexPtr() const { return m_innerIndices; }
    inline Index* _innerIndexPtr() { return m_innerIndices; }

    inline const Index* _outerIndexPtr() const { return m_outerIndex; }
    inline Index* _outerIndexPtr() { return m_outerIndex; }
    //----------------------------------------

    inline Scalar coeff(Index row, Index col) const
    {
      const Index outer = IsRowMajor ? row : col;
      const Index inner = IsRowMajor ? col : row;

      Index start = m_outerIndex[outer];
      Index end = m_outerIndex[outer+1];
      if (start==end)
        return Scalar(0);
      else if (end>0 && inner==m_innerIndices[end-1])
        return m_values[end-1];
      // ^^  optimization: let's first check if it is the last coefficient
      // (very common in high level algorithms)

      const Index* r = std::lower_bound(&m_innerIndices[start],&m_innerIndices[end-1],inner);
      const Index id = r-&m_innerIndices[0];
      return ((*r==inner) && (id<end)) ? m_values[id] : Scalar(0);
    }

    inline Scalar& coeffRef(Index row, Index col)
    {
      const Index outer = IsRowMajor ? row : col;
      const Index inner = IsRowMajor ? col : row;

      Index start = m_outerIndex[outer];
      Index end = m_outerIndex[outer+1];
      eigen_assert(end>=start && "you probably called coeffRef on a non finalized matrix");
      eigen_assert(end>start && "coeffRef cannot be called on a zero coefficient");
      Index* r = std::lower_bound(&m_innerIndices[start],&m_innerIndices[end],inner);
      const Index id = r-&m_innerIndices[0];
      eigen_assert((*r==inner) && (id<end) && "coeffRef cannot be called on a zero coefficient");
      return m_values[id];
    }

    class InnerIterator;

    /** \returns the number of non zero coefficients */
    inline Index nonZeros() const  { return m_nnz; }

    inline MappedSparseMatrix(Index rows, Index cols, Index nnz, Index* outerIndexPtr, Index* innerIndexPtr, Scalar* valuePtr)
      : m_outerSize(IsRowMajor?rows:cols), m_innerSize(IsRowMajor?cols:rows), m_nnz(nnz), m_outerIndex(outerIndexPtr),
        m_innerIndices(innerIndexPtr), m_values(valuePtr)
    {}

    /** Empty destructor */
    inline ~MappedSparseMatrix() {}
};

template<typename Scalar, int _Flags, typename _Index>
class MappedSparseMatrix<Scalar,_Flags,_Index>::InnerIterator
{
  public:
    InnerIterator(const MappedSparseMatrix& mat, Index outer)
      : m_matrix(mat),
        m_outer(outer),
        m_id(mat._outerIndexPtr()[outer]),
        m_start(m_id),
        m_end(mat._outerIndexPtr()[outer+1])
    {}

    template<unsigned int Added, unsigned int Removed>
    InnerIterator(const Flagged<MappedSparseMatrix,Added,Removed>& mat, Index outer)
      : m_matrix(mat._expression()), m_id(m_matrix._outerIndexPtr()[outer]),
        m_start(m_id), m_end(m_matrix._outerIndexPtr()[outer+1])
    {}

    inline InnerIterator& operator++() { m_id++; return *this; }

    inline Scalar value() const { return m_matrix._valuePtr()[m_id]; }
    inline Scalar& valueRef() { return const_cast<Scalar&>(m_matrix._valuePtr()[m_id]); }

    inline Index index() const { return m_matrix._innerIndexPtr()[m_id]; }
    inline Index row() const { return IsRowMajor ? m_outer : index(); }
    inline Index col() const { return IsRowMajor ? index() : m_outer; }

    inline operator bool() const { return (m_id < m_end) && (m_id>=m_start); }

  protected:
    const MappedSparseMatrix& m_matrix;
    const Index m_outer;
    Index m_id;
    const Index m_start;
    const Index m_end;
};

#endif // EIGEN_MAPPED_SPARSEMATRIX_H
