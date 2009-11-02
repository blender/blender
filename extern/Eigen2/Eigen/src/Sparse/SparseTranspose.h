// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
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

#ifndef EIGEN_SPARSETRANSPOSE_H
#define EIGEN_SPARSETRANSPOSE_H

template<typename MatrixType>
struct ei_traits<SparseTranspose<MatrixType> > : ei_traits<Transpose<MatrixType> >
{};

template<typename MatrixType> class SparseTranspose
  : public SparseMatrixBase<SparseTranspose<MatrixType> >
{
  public:

    EIGEN_GENERIC_PUBLIC_INTERFACE(SparseTranspose)

    class InnerIterator;
    class ReverseInnerIterator;

    inline SparseTranspose(const MatrixType& matrix) : m_matrix(matrix) {}

    //EIGEN_INHERIT_ASSIGNMENT_OPERATORS(SparseTranspose)

    inline int rows() const { return m_matrix.cols(); }
    inline int cols() const { return m_matrix.rows(); }
    inline int nonZeros() const { return m_matrix.nonZeros(); }

    // FIXME should be keep them ?
    inline Scalar& coeffRef(int row, int col)
    { return m_matrix.const_cast_derived().coeffRef(col, row); }

    inline const Scalar coeff(int row, int col) const
    { return m_matrix.coeff(col, row); }

    inline const Scalar coeff(int index) const
    { return m_matrix.coeff(index); }

    inline Scalar& coeffRef(int index)
    { return m_matrix.const_cast_derived().coeffRef(index); }

  protected:
    const typename MatrixType::Nested m_matrix;

  private:
    SparseTranspose& operator=(const SparseTranspose&);
};

template<typename MatrixType> class SparseTranspose<MatrixType>::InnerIterator : public MatrixType::InnerIterator
{
  public:
    EIGEN_STRONG_INLINE InnerIterator(const SparseTranspose& trans, int outer)
      : MatrixType::InnerIterator(trans.m_matrix, outer)
    {}

  private:
    InnerIterator& operator=(const InnerIterator&);
};

template<typename MatrixType> class SparseTranspose<MatrixType>::ReverseInnerIterator : public MatrixType::ReverseInnerIterator
{
  public:

    EIGEN_STRONG_INLINE ReverseInnerIterator(const SparseTranspose& xpr, int outer)
      : MatrixType::ReverseInnerIterator(xpr.m_matrix, outer)
    {}
};

#endif // EIGEN_SPARSETRANSPOSE_H
