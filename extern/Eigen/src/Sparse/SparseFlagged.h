// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2008 Benoit Jacob <jacob.benoit.1@gmail.com>
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

#ifndef EIGEN_SPARSE_FLAGGED_H
#define EIGEN_SPARSE_FLAGGED_H

template<typename ExpressionType, unsigned int Added, unsigned int Removed>
struct ei_traits<SparseFlagged<ExpressionType, Added, Removed> > : ei_traits<ExpressionType>
{
  enum { Flags = (ExpressionType::Flags | Added) & ~Removed };
};

template<typename ExpressionType, unsigned int Added, unsigned int Removed> class SparseFlagged
  : public SparseMatrixBase<SparseFlagged<ExpressionType, Added, Removed> >
{
  public:

    EIGEN_SPARSE_GENERIC_PUBLIC_INTERFACE(SparseFlagged)
    class InnerIterator;
    class ReverseInnerIterator;
    
    typedef typename ei_meta_if<ei_must_nest_by_value<ExpressionType>::ret,
        ExpressionType, const ExpressionType&>::ret ExpressionTypeNested;

    inline SparseFlagged(const ExpressionType& matrix) : m_matrix(matrix) {}

    inline int rows() const { return m_matrix.rows(); }
    inline int cols() const { return m_matrix.cols(); }
    
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
    ExpressionTypeNested m_matrix;
};

template<typename ExpressionType, unsigned int Added, unsigned int Removed>
  class SparseFlagged<ExpressionType,Added,Removed>::InnerIterator : public ExpressionType::InnerIterator
{
  public:

    EIGEN_STRONG_INLINE InnerIterator(const SparseFlagged& xpr, int outer)
      : ExpressionType::InnerIterator(xpr.m_matrix, outer)
    {}
};

template<typename ExpressionType, unsigned int Added, unsigned int Removed>
  class SparseFlagged<ExpressionType,Added,Removed>::ReverseInnerIterator : public ExpressionType::ReverseInnerIterator
{
  public:

    EIGEN_STRONG_INLINE ReverseInnerIterator(const SparseFlagged& xpr, int outer)
      : ExpressionType::ReverseInnerIterator(xpr.m_matrix, outer)
    {}
};

template<typename Derived>
template<unsigned int Added>
inline const SparseFlagged<Derived, Added, 0>
SparseMatrixBase<Derived>::marked() const
{
  return derived();
}

#endif // EIGEN_SPARSE_FLAGGED_H
