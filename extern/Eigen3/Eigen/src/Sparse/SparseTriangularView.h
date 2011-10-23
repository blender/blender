// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2009 Gael Guennebaud <gael.guennebaud@inria.fr>
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

#ifndef EIGEN_SPARSE_TRIANGULARVIEW_H
#define EIGEN_SPARSE_TRIANGULARVIEW_H

namespace internal {
  
template<typename MatrixType, int Mode>
struct traits<SparseTriangularView<MatrixType,Mode> >
: public traits<MatrixType>
{};

} // namespace internal

template<typename MatrixType, int Mode> class SparseTriangularView
  : public SparseMatrixBase<SparseTriangularView<MatrixType,Mode> >
{
    enum { SkipFirst = (Mode==Lower && !(MatrixType::Flags&RowMajorBit))
                    || (Mode==Upper &&  (MatrixType::Flags&RowMajorBit)) };
  public:
    
    EIGEN_SPARSE_PUBLIC_INTERFACE(SparseTriangularView)

    class InnerIterator;

    inline Index rows() const { return m_matrix.rows(); }
    inline Index cols() const { return m_matrix.cols(); }

    typedef typename internal::conditional<internal::must_nest_by_value<MatrixType>::ret,
        MatrixType, const MatrixType&>::type MatrixTypeNested;

    inline SparseTriangularView(const MatrixType& matrix) : m_matrix(matrix) {}

    /** \internal */
    inline const MatrixType& nestedExpression() const { return m_matrix; }

    template<typename OtherDerived>
    typename internal::plain_matrix_type_column_major<OtherDerived>::type
    solve(const MatrixBase<OtherDerived>& other) const;

    template<typename OtherDerived> void solveInPlace(MatrixBase<OtherDerived>& other) const;
    template<typename OtherDerived> void solveInPlace(SparseMatrixBase<OtherDerived>& other) const;

  protected:
    MatrixTypeNested m_matrix;
};

template<typename MatrixType, int Mode>
class SparseTriangularView<MatrixType,Mode>::InnerIterator : public MatrixType::InnerIterator
{
    typedef typename MatrixType::InnerIterator Base;
  public:

    EIGEN_STRONG_INLINE InnerIterator(const SparseTriangularView& view, Index outer)
      : Base(view.nestedExpression(), outer)
    {
      if(SkipFirst)
        while((*this) && this->index()<outer)
          ++(*this);
    }
    inline Index row() const { return Base::row(); }
    inline Index col() const { return Base::col(); }

    EIGEN_STRONG_INLINE operator bool() const
    {
      return SkipFirst ? Base::operator bool() : (Base::operator bool() && this->index() <= this->outer());
    }
};

template<typename Derived>
template<int Mode>
inline const SparseTriangularView<Derived, Mode>
SparseMatrixBase<Derived>::triangularView() const
{
  return derived();
}

#endif // EIGEN_SPARSE_TRIANGULARVIEW_H
