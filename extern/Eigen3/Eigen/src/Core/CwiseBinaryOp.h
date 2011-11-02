// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
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

#ifndef EIGEN_CWISE_BINARY_OP_H
#define EIGEN_CWISE_BINARY_OP_H

/** \class CwiseBinaryOp
  * \ingroup Core_Module
  *
  * \brief Generic expression where a coefficient-wise binary operator is applied to two expressions
  *
  * \param BinaryOp template functor implementing the operator
  * \param Lhs the type of the left-hand side
  * \param Rhs the type of the right-hand side
  *
  * This class represents an expression  where a coefficient-wise binary operator is applied to two expressions.
  * It is the return type of binary operators, by which we mean only those binary operators where
  * both the left-hand side and the right-hand side are Eigen expressions.
  * For example, the return type of matrix1+matrix2 is a CwiseBinaryOp.
  *
  * Most of the time, this is the only way that it is used, so you typically don't have to name
  * CwiseBinaryOp types explicitly.
  *
  * \sa MatrixBase::binaryExpr(const MatrixBase<OtherDerived> &,const CustomBinaryOp &) const, class CwiseUnaryOp, class CwiseNullaryOp
  */

namespace internal {
template<typename BinaryOp, typename Lhs, typename Rhs>
struct traits<CwiseBinaryOp<BinaryOp, Lhs, Rhs> >
{
  // we must not inherit from traits<Lhs> since it has
  // the potential to cause problems with MSVC
  typedef typename remove_all<Lhs>::type Ancestor;
  typedef typename traits<Ancestor>::XprKind XprKind;
  enum {
    RowsAtCompileTime = traits<Ancestor>::RowsAtCompileTime,
    ColsAtCompileTime = traits<Ancestor>::ColsAtCompileTime,
    MaxRowsAtCompileTime = traits<Ancestor>::MaxRowsAtCompileTime,
    MaxColsAtCompileTime = traits<Ancestor>::MaxColsAtCompileTime
  };

  // even though we require Lhs and Rhs to have the same scalar type (see CwiseBinaryOp constructor),
  // we still want to handle the case when the result type is different.
  typedef typename result_of<
                     BinaryOp(
                       typename Lhs::Scalar,
                       typename Rhs::Scalar
                     )
                   >::type Scalar;
  typedef typename promote_storage_type<typename traits<Lhs>::StorageKind,
                                           typename traits<Rhs>::StorageKind>::ret StorageKind;
  typedef typename promote_index_type<typename traits<Lhs>::Index,
                                         typename traits<Rhs>::Index>::type Index;
  typedef typename Lhs::Nested LhsNested;
  typedef typename Rhs::Nested RhsNested;
  typedef typename remove_reference<LhsNested>::type _LhsNested;
  typedef typename remove_reference<RhsNested>::type _RhsNested;
  enum {
    LhsCoeffReadCost = _LhsNested::CoeffReadCost,
    RhsCoeffReadCost = _RhsNested::CoeffReadCost,
    LhsFlags = _LhsNested::Flags,
    RhsFlags = _RhsNested::Flags,
    SameType = is_same<typename _LhsNested::Scalar,typename _RhsNested::Scalar>::value,
    StorageOrdersAgree = (int(Lhs::Flags)&RowMajorBit)==(int(Rhs::Flags)&RowMajorBit),
    Flags0 = (int(LhsFlags) | int(RhsFlags)) & (
        HereditaryBits
      | (int(LhsFlags) & int(RhsFlags) &
           ( AlignedBit
           | (StorageOrdersAgree ? LinearAccessBit : 0)
           | (functor_traits<BinaryOp>::PacketAccess && StorageOrdersAgree && SameType ? PacketAccessBit : 0)
           )
        )
     ),
    Flags = (Flags0 & ~RowMajorBit) | (LhsFlags & RowMajorBit),
    CoeffReadCost = LhsCoeffReadCost + RhsCoeffReadCost + functor_traits<BinaryOp>::Cost
  };
};
} // end namespace internal

// we require Lhs and Rhs to have the same scalar type. Currently there is no example of a binary functor
// that would take two operands of different types. If there were such an example, then this check should be
// moved to the BinaryOp functors, on a per-case basis. This would however require a change in the BinaryOp functors, as
// currently they take only one typename Scalar template parameter.
// It is tempting to always allow mixing different types but remember that this is often impossible in the vectorized paths.
// So allowing mixing different types gives very unexpected errors when enabling vectorization, when the user tries to
// add together a float matrix and a double matrix.
#define EIGEN_CHECK_BINARY_COMPATIBILIY(BINOP,LHS,RHS) \
  EIGEN_STATIC_ASSERT((internal::functor_allows_mixing_real_and_complex<BINOP>::ret \
                        ? int(internal::is_same<typename NumTraits<LHS>::Real, typename NumTraits<RHS>::Real>::value) \
                        : int(internal::is_same<LHS, RHS>::value)), \
    YOU_MIXED_DIFFERENT_NUMERIC_TYPES__YOU_NEED_TO_USE_THE_CAST_METHOD_OF_MATRIXBASE_TO_CAST_NUMERIC_TYPES_EXPLICITLY)

template<typename BinaryOp, typename Lhs, typename Rhs, typename StorageKind>
class CwiseBinaryOpImpl;

template<typename BinaryOp, typename Lhs, typename Rhs>
class CwiseBinaryOp : internal::no_assignment_operator,
  public CwiseBinaryOpImpl<
          BinaryOp, Lhs, Rhs,
          typename internal::promote_storage_type<typename internal::traits<Lhs>::StorageKind,
                                           typename internal::traits<Rhs>::StorageKind>::ret>
{
  public:

    typedef typename CwiseBinaryOpImpl<
        BinaryOp, Lhs, Rhs,
        typename internal::promote_storage_type<typename internal::traits<Lhs>::StorageKind,
                                         typename internal::traits<Rhs>::StorageKind>::ret>::Base Base;
    EIGEN_GENERIC_PUBLIC_INTERFACE(CwiseBinaryOp)

    typedef typename internal::nested<Lhs>::type LhsNested;
    typedef typename internal::nested<Rhs>::type RhsNested;
    typedef typename internal::remove_reference<LhsNested>::type _LhsNested;
    typedef typename internal::remove_reference<RhsNested>::type _RhsNested;

    EIGEN_STRONG_INLINE CwiseBinaryOp(const Lhs& lhs, const Rhs& rhs, const BinaryOp& func = BinaryOp())
      : m_lhs(lhs), m_rhs(rhs), m_functor(func)
    {
      EIGEN_CHECK_BINARY_COMPATIBILIY(BinaryOp,typename Lhs::Scalar,typename Rhs::Scalar);
      // require the sizes to match
      EIGEN_STATIC_ASSERT_SAME_MATRIX_SIZE(Lhs, Rhs)
      eigen_assert(lhs.rows() == rhs.rows() && lhs.cols() == rhs.cols());
    }

    EIGEN_STRONG_INLINE Index rows() const {
      // return the fixed size type if available to enable compile time optimizations
      if (internal::traits<typename internal::remove_all<LhsNested>::type>::RowsAtCompileTime==Dynamic)
        return m_rhs.rows();
      else
        return m_lhs.rows();
    }
    EIGEN_STRONG_INLINE Index cols() const {
      // return the fixed size type if available to enable compile time optimizations
      if (internal::traits<typename internal::remove_all<LhsNested>::type>::ColsAtCompileTime==Dynamic)
        return m_rhs.cols();
      else
        return m_lhs.cols();
    }

    /** \returns the left hand side nested expression */
    const _LhsNested& lhs() const { return m_lhs; }
    /** \returns the right hand side nested expression */
    const _RhsNested& rhs() const { return m_rhs; }
    /** \returns the functor representing the binary operation */
    const BinaryOp& functor() const { return m_functor; }

  protected:
    const LhsNested m_lhs;
    const RhsNested m_rhs;
    const BinaryOp m_functor;
};

template<typename BinaryOp, typename Lhs, typename Rhs>
class CwiseBinaryOpImpl<BinaryOp, Lhs, Rhs, Dense>
  : public internal::dense_xpr_base<CwiseBinaryOp<BinaryOp, Lhs, Rhs> >::type
{
    typedef CwiseBinaryOp<BinaryOp, Lhs, Rhs> Derived;
  public:

    typedef typename internal::dense_xpr_base<CwiseBinaryOp<BinaryOp, Lhs, Rhs> >::type Base;
    EIGEN_DENSE_PUBLIC_INTERFACE( Derived )

    EIGEN_STRONG_INLINE const Scalar coeff(Index row, Index col) const
    {
      return derived().functor()(derived().lhs().coeff(row, col),
                                 derived().rhs().coeff(row, col));
    }

    template<int LoadMode>
    EIGEN_STRONG_INLINE PacketScalar packet(Index row, Index col) const
    {
      return derived().functor().packetOp(derived().lhs().template packet<LoadMode>(row, col),
                                          derived().rhs().template packet<LoadMode>(row, col));
    }

    EIGEN_STRONG_INLINE const Scalar coeff(Index index) const
    {
      return derived().functor()(derived().lhs().coeff(index),
                                 derived().rhs().coeff(index));
    }

    template<int LoadMode>
    EIGEN_STRONG_INLINE PacketScalar packet(Index index) const
    {
      return derived().functor().packetOp(derived().lhs().template packet<LoadMode>(index),
                                          derived().rhs().template packet<LoadMode>(index));
    }
};

/** replaces \c *this by \c *this - \a other.
  *
  * \returns a reference to \c *this
  */
template<typename Derived>
template<typename OtherDerived>
EIGEN_STRONG_INLINE Derived &
MatrixBase<Derived>::operator-=(const MatrixBase<OtherDerived> &other)
{
  SelfCwiseBinaryOp<internal::scalar_difference_op<Scalar>, Derived, OtherDerived> tmp(derived());
  tmp = other.derived();
  return derived();
}

/** replaces \c *this by \c *this + \a other.
  *
  * \returns a reference to \c *this
  */
template<typename Derived>
template<typename OtherDerived>
EIGEN_STRONG_INLINE Derived &
MatrixBase<Derived>::operator+=(const MatrixBase<OtherDerived>& other)
{
  SelfCwiseBinaryOp<internal::scalar_sum_op<Scalar>, Derived, OtherDerived> tmp(derived());
  tmp = other.derived();
  return derived();
}

#endif // EIGEN_CWISE_BINARY_OP_H
