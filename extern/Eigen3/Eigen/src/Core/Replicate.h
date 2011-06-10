// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2009-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
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

#ifndef EIGEN_REPLICATE_H
#define EIGEN_REPLICATE_H

/**
  * \class Replicate
  * \ingroup Core_Module
  *
  * \brief Expression of the multiple replication of a matrix or vector
  *
  * \param MatrixType the type of the object we are replicating
  *
  * This class represents an expression of the multiple replication of a matrix or vector.
  * It is the return type of DenseBase::replicate() and most of the time
  * this is the only way it is used.
  *
  * \sa DenseBase::replicate()
  */

namespace internal {
template<typename MatrixType,int RowFactor,int ColFactor>
struct traits<Replicate<MatrixType,RowFactor,ColFactor> >
 : traits<MatrixType>
{
  typedef typename MatrixType::Scalar Scalar;
  typedef typename traits<MatrixType>::StorageKind StorageKind;
  typedef typename traits<MatrixType>::XprKind XprKind;
  typedef typename nested<MatrixType>::type MatrixTypeNested;
  typedef typename remove_reference<MatrixTypeNested>::type _MatrixTypeNested;
  enum {
    RowsAtCompileTime = RowFactor==Dynamic || int(MatrixType::RowsAtCompileTime)==Dynamic
                      ? Dynamic
                      : RowFactor * MatrixType::RowsAtCompileTime,
    ColsAtCompileTime = ColFactor==Dynamic || int(MatrixType::ColsAtCompileTime)==Dynamic
                      ? Dynamic
                      : ColFactor * MatrixType::ColsAtCompileTime,
   //FIXME we don't propagate the max sizes !!!
    MaxRowsAtCompileTime = RowsAtCompileTime,
    MaxColsAtCompileTime = ColsAtCompileTime,
    IsRowMajor = MaxRowsAtCompileTime==1 && MaxColsAtCompileTime!=1 ? 1
               : MaxColsAtCompileTime==1 && MaxRowsAtCompileTime!=1 ? 0
               : (MatrixType::Flags & RowMajorBit) ? 1 : 0,
    Flags = (_MatrixTypeNested::Flags & HereditaryBits & ~RowMajorBit) | (IsRowMajor ? RowMajorBit : 0),
    CoeffReadCost = _MatrixTypeNested::CoeffReadCost
  };
};
}

template<typename MatrixType,int RowFactor,int ColFactor> class Replicate
  : public internal::dense_xpr_base< Replicate<MatrixType,RowFactor,ColFactor> >::type
{
  public:

    typedef typename internal::dense_xpr_base<Replicate>::type Base;
    EIGEN_DENSE_PUBLIC_INTERFACE(Replicate)

    template<typename OriginalMatrixType>
    inline explicit Replicate(const OriginalMatrixType& matrix)
      : m_matrix(matrix), m_rowFactor(RowFactor), m_colFactor(ColFactor)
    {
      EIGEN_STATIC_ASSERT((internal::is_same<typename internal::remove_const<MatrixType>::type,OriginalMatrixType>::value),
                          THE_MATRIX_OR_EXPRESSION_THAT_YOU_PASSED_DOES_NOT_HAVE_THE_EXPECTED_TYPE)
      eigen_assert(RowFactor!=Dynamic && ColFactor!=Dynamic);
    }

    template<typename OriginalMatrixType>
    inline Replicate(const OriginalMatrixType& matrix, int rowFactor, int colFactor)
      : m_matrix(matrix), m_rowFactor(rowFactor), m_colFactor(colFactor)
    {
      EIGEN_STATIC_ASSERT((internal::is_same<typename internal::remove_const<MatrixType>::type,OriginalMatrixType>::value),
                          THE_MATRIX_OR_EXPRESSION_THAT_YOU_PASSED_DOES_NOT_HAVE_THE_EXPECTED_TYPE)
    }

    inline Index rows() const { return m_matrix.rows() * m_rowFactor.value(); }
    inline Index cols() const { return m_matrix.cols() * m_colFactor.value(); }

    inline Scalar coeff(Index row, Index col) const
    {
      // try to avoid using modulo; this is a pure optimization strategy
      const Index actual_row  = internal::traits<MatrixType>::RowsAtCompileTime==1 ? 0
                            : RowFactor==1 ? row
                            : row%m_matrix.rows();
      const Index actual_col  = internal::traits<MatrixType>::ColsAtCompileTime==1 ? 0
                            : ColFactor==1 ? col
                            : col%m_matrix.cols();

      return m_matrix.coeff(actual_row, actual_col);
    }
    template<int LoadMode>
    inline PacketScalar packet(Index row, Index col) const
    {
      const Index actual_row  = internal::traits<MatrixType>::RowsAtCompileTime==1 ? 0
                            : RowFactor==1 ? row
                            : row%m_matrix.rows();
      const Index actual_col  = internal::traits<MatrixType>::ColsAtCompileTime==1 ? 0
                            : ColFactor==1 ? col
                            : col%m_matrix.cols();

      return m_matrix.template packet<LoadMode>(actual_row, actual_col);
    }


  protected:
    const typename MatrixType::Nested m_matrix;
    const internal::variable_if_dynamic<Index, RowFactor> m_rowFactor;
    const internal::variable_if_dynamic<Index, ColFactor> m_colFactor;
};

/**
  * \return an expression of the replication of \c *this
  *
  * Example: \include MatrixBase_replicate.cpp
  * Output: \verbinclude MatrixBase_replicate.out
  *
  * \sa VectorwiseOp::replicate(), DenseBase::replicate(Index,Index), class Replicate
  */
template<typename Derived>
template<int RowFactor, int ColFactor>
inline const Replicate<Derived,RowFactor,ColFactor>
DenseBase<Derived>::replicate() const
{
  return Replicate<Derived,RowFactor,ColFactor>(derived());
}

/**
  * \return an expression of the replication of \c *this
  *
  * Example: \include MatrixBase_replicate_int_int.cpp
  * Output: \verbinclude MatrixBase_replicate_int_int.out
  *
  * \sa VectorwiseOp::replicate(), DenseBase::replicate<int,int>(), class Replicate
  */
template<typename Derived>
inline const Replicate<Derived,Dynamic,Dynamic>
DenseBase<Derived>::replicate(Index rowFactor,Index colFactor) const
{
  return Replicate<Derived,Dynamic,Dynamic>(derived(),rowFactor,colFactor);
}

/**
  * \return an expression of the replication of each column (or row) of \c *this
  *
  * Example: \include DirectionWise_replicate_int.cpp
  * Output: \verbinclude DirectionWise_replicate_int.out
  *
  * \sa VectorwiseOp::replicate(), DenseBase::replicate(), class Replicate
  */
template<typename ExpressionType, int Direction>
const typename VectorwiseOp<ExpressionType,Direction>::ReplicateReturnType
VectorwiseOp<ExpressionType,Direction>::replicate(Index factor) const
{
  return typename VectorwiseOp<ExpressionType,Direction>::ReplicateReturnType
          (_expression(),Direction==Vertical?factor:1,Direction==Horizontal?factor:1);
}

#endif // EIGEN_REPLICATE_H
