// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
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

#ifndef EIGEN_TRANSPOSE_H
#define EIGEN_TRANSPOSE_H

/** \class Transpose
  *
  * \brief Expression of the transpose of a matrix
  *
  * \param MatrixType the type of the object of which we are taking the transpose
  *
  * This class represents an expression of the transpose of a matrix.
  * It is the return type of MatrixBase::transpose() and MatrixBase::adjoint()
  * and most of the time this is the only way it is used.
  *
  * \sa MatrixBase::transpose(), MatrixBase::adjoint()
  */
template<typename MatrixType>
struct ei_traits<Transpose<MatrixType> >
{
  typedef typename MatrixType::Scalar Scalar;
  typedef typename ei_nested<MatrixType>::type MatrixTypeNested;
  typedef typename ei_unref<MatrixTypeNested>::type _MatrixTypeNested;
  enum {
    RowsAtCompileTime = MatrixType::ColsAtCompileTime,
    ColsAtCompileTime = MatrixType::RowsAtCompileTime,
    MaxRowsAtCompileTime = MatrixType::MaxColsAtCompileTime,
    MaxColsAtCompileTime = MatrixType::MaxRowsAtCompileTime,
    Flags = ((int(_MatrixTypeNested::Flags) ^ RowMajorBit)
          & ~(LowerTriangularBit | UpperTriangularBit))
          | (int(_MatrixTypeNested::Flags)&UpperTriangularBit ? LowerTriangularBit : 0)
          | (int(_MatrixTypeNested::Flags)&LowerTriangularBit ? UpperTriangularBit : 0),
    CoeffReadCost = _MatrixTypeNested::CoeffReadCost
  };
};

template<typename MatrixType> class Transpose
  : public MatrixBase<Transpose<MatrixType> >
{
  public:

    EIGEN_GENERIC_PUBLIC_INTERFACE(Transpose)

    inline Transpose(const MatrixType& matrix) : m_matrix(matrix) {}

    EIGEN_INHERIT_ASSIGNMENT_OPERATORS(Transpose)

    inline int rows() const { return m_matrix.cols(); }
    inline int cols() const { return m_matrix.rows(); }
    inline int nonZeros() const { return m_matrix.nonZeros(); }
    inline int stride(void) const { return m_matrix.stride(); }

    inline Scalar& coeffRef(int row, int col)
    {
      return m_matrix.const_cast_derived().coeffRef(col, row);
    }

    inline const Scalar coeff(int row, int col) const
    {
      return m_matrix.coeff(col, row);
    }

    inline const Scalar coeff(int index) const
    {
      return m_matrix.coeff(index);
    }

    inline Scalar& coeffRef(int index)
    {
      return m_matrix.const_cast_derived().coeffRef(index);
    }

    template<int LoadMode>
    inline const PacketScalar packet(int row, int col) const
    {
      return m_matrix.template packet<LoadMode>(col, row);
    }

    template<int LoadMode>
    inline void writePacket(int row, int col, const PacketScalar& x)
    {
      m_matrix.const_cast_derived().template writePacket<LoadMode>(col, row, x);
    }

    template<int LoadMode>
    inline const PacketScalar packet(int index) const
    {
      return m_matrix.template packet<LoadMode>(index);
    }

    template<int LoadMode>
    inline void writePacket(int index, const PacketScalar& x)
    {
      m_matrix.const_cast_derived().template writePacket<LoadMode>(index, x);
    }

  protected:
    const typename MatrixType::Nested m_matrix;
};

/** \returns an expression of the transpose of *this.
  *
  * Example: \include MatrixBase_transpose.cpp
  * Output: \verbinclude MatrixBase_transpose.out
  *
  * \sa adjoint(), class DiagonalCoeffs */
template<typename Derived>
inline Transpose<Derived>
MatrixBase<Derived>::transpose()
{
  return derived();
}

/** This is the const version of transpose(). \sa adjoint() */
template<typename Derived>
inline const Transpose<Derived>
MatrixBase<Derived>::transpose() const
{
  return derived();
}

/** \returns an expression of the adjoint (i.e. conjugate transpose) of *this.
  *
  * Example: \include MatrixBase_adjoint.cpp
  * Output: \verbinclude MatrixBase_adjoint.out
  *
  * \sa transpose(), conjugate(), class Transpose, class ei_scalar_conjugate_op */
template<typename Derived>
inline const typename MatrixBase<Derived>::AdjointReturnType
MatrixBase<Derived>::adjoint() const
{
  return conjugate().nestByValue();
}

/***************************************************************************
* "in place" transpose implementation
***************************************************************************/

template<typename MatrixType,
  bool IsSquare = (MatrixType::RowsAtCompileTime == MatrixType::ColsAtCompileTime) && MatrixType::RowsAtCompileTime!=Dynamic>
struct ei_inplace_transpose_selector;

template<typename MatrixType>
struct ei_inplace_transpose_selector<MatrixType,true> { // square matrix
  static void run(MatrixType& m) {
    m.template part<StrictlyUpperTriangular>().swap(m.transpose());
  }
};

template<typename MatrixType>
struct ei_inplace_transpose_selector<MatrixType,false> { // non square matrix
  static void run(MatrixType& m) {
    if (m.rows()==m.cols())
      m.template part<StrictlyUpperTriangular>().swap(m.transpose());
    else
      m = m.transpose().eval();
  }
};

/** This is the "in place" version of transpose: it transposes \c *this.
  *
  * In most cases it is probably better to simply use the transposed expression
  * of a matrix. However, when transposing the matrix data itself is really needed,
  * then this "in-place" version is probably the right choice because it provides
  * the following additional features:
  *  - less error prone: doing the same operation with .transpose() requires special care:
  *    \code m = m.transpose().eval(); \endcode
  *  - no temporary object is created (currently only for squared matrices)
  *  - it allows future optimizations (cache friendliness, etc.)
  *
  * \note if the matrix is not square, then \c *this must be a resizable matrix.
  *
  * \sa transpose(), adjoint() */
template<typename Derived>
inline void MatrixBase<Derived>::transposeInPlace()
{
  ei_inplace_transpose_selector<Derived>::run(derived());
}

#endif // EIGEN_TRANSPOSE_H
