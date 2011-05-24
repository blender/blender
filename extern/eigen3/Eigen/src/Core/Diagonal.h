// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2007-2009 Benoit Jacob <jacob.benoit.1@gmail.com>
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

#ifndef EIGEN_DIAGONAL_H
#define EIGEN_DIAGONAL_H

/** \class Diagonal
  * \ingroup Core_Module
  *
  * \brief Expression of a diagonal/subdiagonal/superdiagonal in a matrix
  *
  * \param MatrixType the type of the object in which we are taking a sub/main/super diagonal
  * \param DiagIndex the index of the sub/super diagonal. The default is 0 and it means the main diagonal.
  *              A positive value means a superdiagonal, a negative value means a subdiagonal.
  *              You can also use Dynamic so the index can be set at runtime.
  *
  * The matrix is not required to be square.
  *
  * This class represents an expression of the main diagonal, or any sub/super diagonal
  * of a square matrix. It is the return type of MatrixBase::diagonal() and MatrixBase::diagonal(Index) and most of the
  * time this is the only way it is used.
  *
  * \sa MatrixBase::diagonal(), MatrixBase::diagonal(Index)
  */

namespace internal {
template<typename MatrixType, int DiagIndex>
struct traits<Diagonal<MatrixType,DiagIndex> >
 : traits<MatrixType>
{
  typedef typename nested<MatrixType>::type MatrixTypeNested;
  typedef typename remove_reference<MatrixTypeNested>::type _MatrixTypeNested;
  typedef typename MatrixType::StorageKind StorageKind;
  enum {
    AbsDiagIndex = DiagIndex<0 ? -DiagIndex : DiagIndex, // only used if DiagIndex != Dynamic
    // FIXME these computations are broken in the case where the matrix is rectangular and DiagIndex!=0
    RowsAtCompileTime = (int(DiagIndex) == Dynamic || int(MatrixType::SizeAtCompileTime) == Dynamic) ? Dynamic
                      : (EIGEN_SIZE_MIN_PREFER_DYNAMIC(MatrixType::RowsAtCompileTime,
                                        MatrixType::ColsAtCompileTime) - AbsDiagIndex),
    ColsAtCompileTime = 1,
    MaxRowsAtCompileTime = int(MatrixType::MaxSizeAtCompileTime) == Dynamic ? Dynamic
                         : DiagIndex == Dynamic ? EIGEN_SIZE_MIN_PREFER_FIXED(MatrixType::MaxRowsAtCompileTime,
                                                                    MatrixType::MaxColsAtCompileTime)
                         : (EIGEN_SIZE_MIN_PREFER_FIXED(MatrixType::MaxRowsAtCompileTime, MatrixType::MaxColsAtCompileTime) - AbsDiagIndex),
    MaxColsAtCompileTime = 1,
    MaskLvalueBit = is_lvalue<MatrixType>::value ? LvalueBit : 0,
    Flags = (unsigned int)_MatrixTypeNested::Flags & (HereditaryBits | LinearAccessBit | MaskLvalueBit | DirectAccessBit) & ~RowMajorBit,
    CoeffReadCost = _MatrixTypeNested::CoeffReadCost,
    MatrixTypeOuterStride = outer_stride_at_compile_time<MatrixType>::ret,
    InnerStrideAtCompileTime = MatrixTypeOuterStride == Dynamic ? Dynamic : MatrixTypeOuterStride+1,
    OuterStrideAtCompileTime = 0
  };
};
}

template<typename MatrixType, int DiagIndex> class Diagonal
   : public internal::dense_xpr_base< Diagonal<MatrixType,DiagIndex> >::type
{
  public:

    typedef typename internal::dense_xpr_base<Diagonal>::type Base;
    EIGEN_DENSE_PUBLIC_INTERFACE(Diagonal)

    inline Diagonal(MatrixType& matrix, Index index = DiagIndex) : m_matrix(matrix), m_index(index) {}

    EIGEN_INHERIT_ASSIGNMENT_OPERATORS(Diagonal)

    inline Index rows() const
    { return m_index.value()<0 ? std::min(m_matrix.cols(),m_matrix.rows()+m_index.value()) : std::min(m_matrix.rows(),m_matrix.cols()-m_index.value()); }

    inline Index cols() const { return 1; }

    inline Index innerStride() const
    {
      return m_matrix.outerStride() + 1;
    }

    inline Index outerStride() const
    {
      return 0;
    }

    inline Scalar& coeffRef(Index row, Index)
    {
      EIGEN_STATIC_ASSERT_LVALUE(MatrixType)
      return m_matrix.const_cast_derived().coeffRef(row+rowOffset(), row+colOffset());
    }

    inline const Scalar& coeffRef(Index row, Index) const
    {
      return m_matrix.const_cast_derived().coeffRef(row+rowOffset(), row+colOffset());
    }

    inline CoeffReturnType coeff(Index row, Index) const
    {
      return m_matrix.coeff(row+rowOffset(), row+colOffset());
    }

    inline Scalar& coeffRef(Index index)
    {
      EIGEN_STATIC_ASSERT_LVALUE(MatrixType)
      return m_matrix.const_cast_derived().coeffRef(index+rowOffset(), index+colOffset());
    }

    inline const Scalar& coeffRef(Index index) const
    {
      return m_matrix.const_cast_derived().coeffRef(index+rowOffset(), index+colOffset());
    }

    inline CoeffReturnType coeff(Index index) const
    {
      return m_matrix.coeff(index+rowOffset(), index+colOffset());
    }

  protected:
    const typename MatrixType::Nested m_matrix;
    const internal::variable_if_dynamic<Index, DiagIndex> m_index;

  private:
    // some compilers may fail to optimize std::max etc in case of compile-time constants...
    EIGEN_STRONG_INLINE Index absDiagIndex() const { return m_index.value()>0 ? m_index.value() : -m_index.value(); }
    EIGEN_STRONG_INLINE Index rowOffset() const { return m_index.value()>0 ? 0 : -m_index.value(); }
    EIGEN_STRONG_INLINE Index colOffset() const { return m_index.value()>0 ? m_index.value() : 0; }
    // triger a compile time error is someone try to call packet
    template<int LoadMode> typename MatrixType::PacketReturnType packet(Index) const;
    template<int LoadMode> typename MatrixType::PacketReturnType packet(Index,Index) const;
};

/** \returns an expression of the main diagonal of the matrix \c *this
  *
  * \c *this is not required to be square.
  *
  * Example: \include MatrixBase_diagonal.cpp
  * Output: \verbinclude MatrixBase_diagonal.out
  *
  * \sa class Diagonal */
template<typename Derived>
inline typename MatrixBase<Derived>::DiagonalReturnType
MatrixBase<Derived>::diagonal()
{
  return derived();
}

/** This is the const version of diagonal(). */
template<typename Derived>
inline const typename MatrixBase<Derived>::ConstDiagonalReturnType
MatrixBase<Derived>::diagonal() const
{
  return ConstDiagonalReturnType(derived());
}

/** \returns an expression of the \a DiagIndex-th sub or super diagonal of the matrix \c *this
  *
  * \c *this is not required to be square.
  *
  * The template parameter \a DiagIndex represent a super diagonal if \a DiagIndex > 0
  * and a sub diagonal otherwise. \a DiagIndex == 0 is equivalent to the main diagonal.
  *
  * Example: \include MatrixBase_diagonal_int.cpp
  * Output: \verbinclude MatrixBase_diagonal_int.out
  *
  * \sa MatrixBase::diagonal(), class Diagonal */
template<typename Derived>
inline typename MatrixBase<Derived>::template DiagonalIndexReturnType<Dynamic>::Type
MatrixBase<Derived>::diagonal(Index index)
{
  return typename DiagonalIndexReturnType<Dynamic>::Type(derived(), index);
}

/** This is the const version of diagonal(Index). */
template<typename Derived>
inline typename MatrixBase<Derived>::template ConstDiagonalIndexReturnType<Dynamic>::Type
MatrixBase<Derived>::diagonal(Index index) const
{
  return typename ConstDiagonalIndexReturnType<Dynamic>::Type(derived(), index);
}

/** \returns an expression of the \a DiagIndex-th sub or super diagonal of the matrix \c *this
  *
  * \c *this is not required to be square.
  *
  * The template parameter \a DiagIndex represent a super diagonal if \a DiagIndex > 0
  * and a sub diagonal otherwise. \a DiagIndex == 0 is equivalent to the main diagonal.
  *
  * Example: \include MatrixBase_diagonal_template_int.cpp
  * Output: \verbinclude MatrixBase_diagonal_template_int.out
  *
  * \sa MatrixBase::diagonal(), class Diagonal */
template<typename Derived>
template<int Index>
inline typename MatrixBase<Derived>::template DiagonalIndexReturnType<Index>::Type
MatrixBase<Derived>::diagonal()
{
  return derived();
}

/** This is the const version of diagonal<int>(). */
template<typename Derived>
template<int Index>
inline typename MatrixBase<Derived>::template ConstDiagonalIndexReturnType<Index>::Type
MatrixBase<Derived>::diagonal() const
{
  return derived();
}

#endif // EIGEN_DIAGONAL_H
