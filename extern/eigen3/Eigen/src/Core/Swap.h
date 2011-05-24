// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
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

#ifndef EIGEN_SWAP_H
#define EIGEN_SWAP_H

/** \class SwapWrapper
  * \ingroup Core_Module
  *
  * \internal
  *
  * \brief Internal helper class for swapping two expressions
  */
namespace internal {
template<typename ExpressionType>
struct traits<SwapWrapper<ExpressionType> > : traits<ExpressionType> {};
}

template<typename ExpressionType> class SwapWrapper
  : public internal::dense_xpr_base<SwapWrapper<ExpressionType> >::type
{
  public:

    typedef typename internal::dense_xpr_base<SwapWrapper>::type Base;
    EIGEN_DENSE_PUBLIC_INTERFACE(SwapWrapper)
    typedef typename internal::packet_traits<Scalar>::type Packet;

    inline SwapWrapper(ExpressionType& xpr) : m_expression(xpr) {}

    inline Index rows() const { return m_expression.rows(); }
    inline Index cols() const { return m_expression.cols(); }
    inline Index outerStride() const { return m_expression.outerStride(); }
    inline Index innerStride() const { return m_expression.innerStride(); }

    inline Scalar& coeffRef(Index row, Index col)
    {
      return m_expression.const_cast_derived().coeffRef(row, col);
    }

    inline Scalar& coeffRef(Index index)
    {
      return m_expression.const_cast_derived().coeffRef(index);
    }

    inline Scalar& coeffRef(Index row, Index col) const
    {
      return m_expression.coeffRef(row, col);
    }

    inline Scalar& coeffRef(Index index) const
    {
      return m_expression.coeffRef(index);
    }

    template<typename OtherDerived>
    void copyCoeff(Index row, Index col, const DenseBase<OtherDerived>& other)
    {
      OtherDerived& _other = other.const_cast_derived();
      eigen_internal_assert(row >= 0 && row < rows()
                         && col >= 0 && col < cols());
      Scalar tmp = m_expression.coeff(row, col);
      m_expression.coeffRef(row, col) = _other.coeff(row, col);
      _other.coeffRef(row, col) = tmp;
    }

    template<typename OtherDerived>
    void copyCoeff(Index index, const DenseBase<OtherDerived>& other)
    {
      OtherDerived& _other = other.const_cast_derived();
      eigen_internal_assert(index >= 0 && index < m_expression.size());
      Scalar tmp = m_expression.coeff(index);
      m_expression.coeffRef(index) = _other.coeff(index);
      _other.coeffRef(index) = tmp;
    }

    template<typename OtherDerived, int StoreMode, int LoadMode>
    void copyPacket(Index row, Index col, const DenseBase<OtherDerived>& other)
    {
      OtherDerived& _other = other.const_cast_derived();
      eigen_internal_assert(row >= 0 && row < rows()
                        && col >= 0 && col < cols());
      Packet tmp = m_expression.template packet<StoreMode>(row, col);
      m_expression.template writePacket<StoreMode>(row, col,
        _other.template packet<LoadMode>(row, col)
      );
      _other.template writePacket<LoadMode>(row, col, tmp);
    }

    template<typename OtherDerived, int StoreMode, int LoadMode>
    void copyPacket(Index index, const DenseBase<OtherDerived>& other)
    {
      OtherDerived& _other = other.const_cast_derived();
      eigen_internal_assert(index >= 0 && index < m_expression.size());
      Packet tmp = m_expression.template packet<StoreMode>(index);
      m_expression.template writePacket<StoreMode>(index,
        _other.template packet<LoadMode>(index)
      );
      _other.template writePacket<LoadMode>(index, tmp);
    }

  protected:
    ExpressionType& m_expression;
};

#endif // EIGEN_SWAP_H
