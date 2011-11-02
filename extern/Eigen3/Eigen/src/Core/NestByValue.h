// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
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

#ifndef EIGEN_NESTBYVALUE_H
#define EIGEN_NESTBYVALUE_H

/** \class NestByValue
  * \ingroup Core_Module
  *
  * \brief Expression which must be nested by value
  *
  * \param ExpressionType the type of the object of which we are requiring nesting-by-value
  *
  * This class is the return type of MatrixBase::nestByValue()
  * and most of the time this is the only way it is used.
  *
  * \sa MatrixBase::nestByValue()
  */

namespace internal {
template<typename ExpressionType>
struct traits<NestByValue<ExpressionType> > : public traits<ExpressionType>
{};
}

template<typename ExpressionType> class NestByValue
  : public internal::dense_xpr_base< NestByValue<ExpressionType> >::type
{
  public:

    typedef typename internal::dense_xpr_base<NestByValue>::type Base;
    EIGEN_DENSE_PUBLIC_INTERFACE(NestByValue)

    inline NestByValue(const ExpressionType& matrix) : m_expression(matrix) {}

    inline Index rows() const { return m_expression.rows(); }
    inline Index cols() const { return m_expression.cols(); }
    inline Index outerStride() const { return m_expression.outerStride(); }
    inline Index innerStride() const { return m_expression.innerStride(); }

    inline const CoeffReturnType coeff(Index row, Index col) const
    {
      return m_expression.coeff(row, col);
    }

    inline Scalar& coeffRef(Index row, Index col)
    {
      return m_expression.const_cast_derived().coeffRef(row, col);
    }

    inline const CoeffReturnType coeff(Index index) const
    {
      return m_expression.coeff(index);
    }

    inline Scalar& coeffRef(Index index)
    {
      return m_expression.const_cast_derived().coeffRef(index);
    }

    template<int LoadMode>
    inline const PacketScalar packet(Index row, Index col) const
    {
      return m_expression.template packet<LoadMode>(row, col);
    }

    template<int LoadMode>
    inline void writePacket(Index row, Index col, const PacketScalar& x)
    {
      m_expression.const_cast_derived().template writePacket<LoadMode>(row, col, x);
    }

    template<int LoadMode>
    inline const PacketScalar packet(Index index) const
    {
      return m_expression.template packet<LoadMode>(index);
    }

    template<int LoadMode>
    inline void writePacket(Index index, const PacketScalar& x)
    {
      m_expression.const_cast_derived().template writePacket<LoadMode>(index, x);
    }

    operator const ExpressionType&() const { return m_expression; }

  protected:
    const ExpressionType m_expression;
};

/** \returns an expression of the temporary version of *this.
  */
template<typename Derived>
inline const NestByValue<Derived>
DenseBase<Derived>::nestByValue() const
{
  return NestByValue<Derived>(derived());
}

#endif // EIGEN_NESTBYVALUE_H
