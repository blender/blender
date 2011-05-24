// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2010 Daniel Lowengrub <lowdanie@gmail.com>
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

#ifndef EIGEN_SPARSEVIEW_H
#define EIGEN_SPARSEVIEW_H

namespace internal {

template<typename MatrixType>
struct traits<SparseView<MatrixType> > : traits<MatrixType>
{
  typedef int Index;
  typedef Sparse StorageKind;
  enum {
    Flags = int(traits<MatrixType>::Flags) & (RowMajorBit)
  };
};

} // end namespace internal

template<typename MatrixType>
class SparseView : public SparseMatrixBase<SparseView<MatrixType> >
{
  typedef typename MatrixType::Nested MatrixTypeNested;
  typedef typename internal::remove_all<MatrixTypeNested>::type _MatrixTypeNested;
public:
  EIGEN_SPARSE_PUBLIC_INTERFACE(SparseView)

  SparseView(const MatrixType& mat, const Scalar& m_reference = Scalar(0),
             typename NumTraits<Scalar>::Real m_epsilon = NumTraits<Scalar>::dummy_precision()) : 
    m_matrix(mat), m_reference(m_reference), m_epsilon(m_epsilon) {}

  class InnerIterator;

  inline Index rows() const { return m_matrix.rows(); }
  inline Index cols() const { return m_matrix.cols(); }

  inline Index innerSize() const { return m_matrix.innerSize(); }
  inline Index outerSize() const { return m_matrix.outerSize(); }

protected:
  const MatrixTypeNested m_matrix;
  Scalar m_reference;
  typename NumTraits<Scalar>::Real m_epsilon;
};

template<typename MatrixType>
class SparseView<MatrixType>::InnerIterator : public _MatrixTypeNested::InnerIterator
{
public:
  typedef typename _MatrixTypeNested::InnerIterator IterBase;
  InnerIterator(const SparseView& view, Index outer) :
  IterBase(view.m_matrix, outer), m_view(view)
  {
    incrementToNonZero();
  }

  EIGEN_STRONG_INLINE InnerIterator& operator++()
  {
    IterBase::operator++();
    incrementToNonZero();
    return *this;
  }

  using IterBase::value;

protected:
  const SparseView& m_view;

private:
  void incrementToNonZero()
  {
    while(internal::isMuchSmallerThan(value(), m_view.m_reference, m_view.m_epsilon) && (bool(*this)))
      {
        IterBase::operator++();
      }
  }
};

template<typename Derived>
const SparseView<Derived> MatrixBase<Derived>::sparseView(const Scalar& m_reference,
                                                          typename NumTraits<Scalar>::Real m_epsilon) const
{
  return SparseView<Derived>(derived(), m_reference, m_epsilon);
}

#endif
