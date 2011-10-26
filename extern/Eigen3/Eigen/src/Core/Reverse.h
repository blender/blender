// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Copyright (C) 2009 Ricard Marxer <email@ricardmarxer.com>
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

#ifndef EIGEN_REVERSE_H
#define EIGEN_REVERSE_H

/** \class Reverse
  * \ingroup Core_Module
  *
  * \brief Expression of the reverse of a vector or matrix
  *
  * \param MatrixType the type of the object of which we are taking the reverse
  *
  * This class represents an expression of the reverse of a vector.
  * It is the return type of MatrixBase::reverse() and VectorwiseOp::reverse()
  * and most of the time this is the only way it is used.
  *
  * \sa MatrixBase::reverse(), VectorwiseOp::reverse()
  */

namespace internal {

template<typename MatrixType, int Direction>
struct traits<Reverse<MatrixType, Direction> >
 : traits<MatrixType>
{
  typedef typename MatrixType::Scalar Scalar;
  typedef typename traits<MatrixType>::StorageKind StorageKind;
  typedef typename traits<MatrixType>::XprKind XprKind;
  typedef typename nested<MatrixType>::type MatrixTypeNested;
  typedef typename remove_reference<MatrixTypeNested>::type _MatrixTypeNested;
  enum {
    RowsAtCompileTime = MatrixType::RowsAtCompileTime,
    ColsAtCompileTime = MatrixType::ColsAtCompileTime,
    MaxRowsAtCompileTime = MatrixType::MaxRowsAtCompileTime,
    MaxColsAtCompileTime = MatrixType::MaxColsAtCompileTime,

    // let's enable LinearAccess only with vectorization because of the product overhead
    LinearAccess = ( (Direction==BothDirections) && (int(_MatrixTypeNested::Flags)&PacketAccessBit) )
                 ? LinearAccessBit : 0,

    Flags = int(_MatrixTypeNested::Flags) & (HereditaryBits | LvalueBit | PacketAccessBit | LinearAccess),

    CoeffReadCost = _MatrixTypeNested::CoeffReadCost
  };
};

template<typename PacketScalar, bool ReversePacket> struct reverse_packet_cond
{
  static inline PacketScalar run(const PacketScalar& x) { return preverse(x); }
};

template<typename PacketScalar> struct reverse_packet_cond<PacketScalar,false>
{
  static inline PacketScalar run(const PacketScalar& x) { return x; }
};

} // end namespace internal 

template<typename MatrixType, int Direction> class Reverse
  : public internal::dense_xpr_base< Reverse<MatrixType, Direction> >::type
{
  public:

    typedef typename internal::dense_xpr_base<Reverse>::type Base;
    EIGEN_DENSE_PUBLIC_INTERFACE(Reverse)
    using Base::IsRowMajor;

    // next line is necessary because otherwise const version of operator()
    // is hidden by non-const version defined in this file
    using Base::operator(); 

  protected:
    enum {
      PacketSize = internal::packet_traits<Scalar>::size,
      IsColMajor = !IsRowMajor,
      ReverseRow = (Direction == Vertical)   || (Direction == BothDirections),
      ReverseCol = (Direction == Horizontal) || (Direction == BothDirections),
      OffsetRow  = ReverseRow && IsColMajor ? PacketSize : 1,
      OffsetCol  = ReverseCol && IsRowMajor ? PacketSize : 1,
      ReversePacket = (Direction == BothDirections)
                    || ((Direction == Vertical)   && IsColMajor)
                    || ((Direction == Horizontal) && IsRowMajor)
    };
    typedef internal::reverse_packet_cond<PacketScalar,ReversePacket> reverse_packet;
  public:

    inline Reverse(const MatrixType& matrix) : m_matrix(matrix) { }

    EIGEN_INHERIT_ASSIGNMENT_OPERATORS(Reverse)

    inline Index rows() const { return m_matrix.rows(); }
    inline Index cols() const { return m_matrix.cols(); }

    inline Index innerStride() const
    {
      return -m_matrix.innerStride();
    }

    inline Scalar& operator()(Index row, Index col)
    {
      eigen_assert(row >= 0 && row < rows() && col >= 0 && col < cols());
      return coeffRef(row, col);
    }

    inline Scalar& coeffRef(Index row, Index col)
    {
      return m_matrix.const_cast_derived().coeffRef(ReverseRow ? m_matrix.rows() - row - 1 : row,
                                                    ReverseCol ? m_matrix.cols() - col - 1 : col);
    }

    inline CoeffReturnType coeff(Index row, Index col) const
    {
      return m_matrix.coeff(ReverseRow ? m_matrix.rows() - row - 1 : row,
                            ReverseCol ? m_matrix.cols() - col - 1 : col);
    }

    inline CoeffReturnType coeff(Index index) const
    {
      return m_matrix.coeff(m_matrix.size() - index - 1);
    }

    inline Scalar& coeffRef(Index index)
    {
      return m_matrix.const_cast_derived().coeffRef(m_matrix.size() - index - 1);
    }

    inline Scalar& operator()(Index index)
    {
      eigen_assert(index >= 0 && index < m_matrix.size());
      return coeffRef(index);
    }

    template<int LoadMode>
    inline const PacketScalar packet(Index row, Index col) const
    {
      return reverse_packet::run(m_matrix.template packet<LoadMode>(
                                    ReverseRow ? m_matrix.rows() - row - OffsetRow : row,
                                    ReverseCol ? m_matrix.cols() - col - OffsetCol : col));
    }

    template<int LoadMode>
    inline void writePacket(Index row, Index col, const PacketScalar& x)
    {
      m_matrix.const_cast_derived().template writePacket<LoadMode>(
                                      ReverseRow ? m_matrix.rows() - row - OffsetRow : row,
                                      ReverseCol ? m_matrix.cols() - col - OffsetCol : col,
                                      reverse_packet::run(x));
    }

    template<int LoadMode>
    inline const PacketScalar packet(Index index) const
    {
      return internal::preverse(m_matrix.template packet<LoadMode>( m_matrix.size() - index - PacketSize ));
    }

    template<int LoadMode>
    inline void writePacket(Index index, const PacketScalar& x)
    {
      m_matrix.const_cast_derived().template writePacket<LoadMode>(m_matrix.size() - index - PacketSize, internal::preverse(x));
    }

  protected:
    const typename MatrixType::Nested m_matrix;
};

/** \returns an expression of the reverse of *this.
  *
  * Example: \include MatrixBase_reverse.cpp
  * Output: \verbinclude MatrixBase_reverse.out
  *
  */
template<typename Derived>
inline typename DenseBase<Derived>::ReverseReturnType
DenseBase<Derived>::reverse()
{
  return derived();
}

/** This is the const version of reverse(). */
template<typename Derived>
inline const typename DenseBase<Derived>::ConstReverseReturnType
DenseBase<Derived>::reverse() const
{
  return derived();
}

/** This is the "in place" version of reverse: it reverses \c *this.
  *
  * In most cases it is probably better to simply use the reversed expression
  * of a matrix. However, when reversing the matrix data itself is really needed,
  * then this "in-place" version is probably the right choice because it provides
  * the following additional features:
  *  - less error prone: doing the same operation with .reverse() requires special care:
  *    \code m = m.reverse().eval(); \endcode
  *  - this API allows to avoid creating a temporary (the current implementation creates a temporary, but that could be avoided using swap)
  *  - it allows future optimizations (cache friendliness, etc.)
  *
  * \sa reverse() */
template<typename Derived>
inline void DenseBase<Derived>::reverseInPlace()
{
  derived() = derived().reverse().eval();
}


#endif // EIGEN_REVERSE_H
