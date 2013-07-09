// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef EIGEN_SWAP_H
#define EIGEN_SWAP_H

namespace Eigen { 

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
    
    typedef typename internal::conditional<
                       internal::is_lvalue<ExpressionType>::value,
                       Scalar,
                       const Scalar
                     >::type ScalarWithConstIfNotLvalue;
                     
    inline ScalarWithConstIfNotLvalue* data() { return m_expression.data(); }
    inline const Scalar* data() const { return m_expression.data(); }

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

    ExpressionType& expression() const { return m_expression; }

  protected:
    ExpressionType& m_expression;
};

} // end namespace Eigen

#endif // EIGEN_SWAP_H
