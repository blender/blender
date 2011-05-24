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

#ifndef EIGEN_ORTHOMETHODS_H
#define EIGEN_ORTHOMETHODS_H

/** \geometry_module
  *
  * \returns the cross product of \c *this and \a other
  *
  * Here is a very good explanation of cross-product: http://xkcd.com/199/
  * \sa MatrixBase::cross3()
  */
template<typename Derived>
template<typename OtherDerived>
inline typename MatrixBase<Derived>::template cross_product_return_type<OtherDerived>::type
MatrixBase<Derived>::cross(const MatrixBase<OtherDerived>& other) const
{
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Derived,3)
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(OtherDerived,3)

  // Note that there is no need for an expression here since the compiler
  // optimize such a small temporary very well (even within a complex expression)
  const typename internal::nested<Derived,2>::type lhs(derived());
  const typename internal::nested<OtherDerived,2>::type rhs(other.derived());
  return typename cross_product_return_type<OtherDerived>::type(
    internal::conj(lhs.coeff(1) * rhs.coeff(2) - lhs.coeff(2) * rhs.coeff(1)),
    internal::conj(lhs.coeff(2) * rhs.coeff(0) - lhs.coeff(0) * rhs.coeff(2)),
    internal::conj(lhs.coeff(0) * rhs.coeff(1) - lhs.coeff(1) * rhs.coeff(0))
  );
}

namespace internal {

template< int Arch,typename VectorLhs,typename VectorRhs,
          typename Scalar = typename VectorLhs::Scalar,
          bool Vectorizable = (VectorLhs::Flags&VectorRhs::Flags)&PacketAccessBit>
struct cross3_impl {
  inline static typename internal::plain_matrix_type<VectorLhs>::type
  run(const VectorLhs& lhs, const VectorRhs& rhs)
  {
    return typename internal::plain_matrix_type<VectorLhs>::type(
      internal::conj(lhs.coeff(1) * rhs.coeff(2) - lhs.coeff(2) * rhs.coeff(1)),
      internal::conj(lhs.coeff(2) * rhs.coeff(0) - lhs.coeff(0) * rhs.coeff(2)),
      internal::conj(lhs.coeff(0) * rhs.coeff(1) - lhs.coeff(1) * rhs.coeff(0)),
      0
    );
  }
};

}

/** \geometry_module
  *
  * \returns the cross product of \c *this and \a other using only the x, y, and z coefficients
  *
  * The size of \c *this and \a other must be four. This function is especially useful
  * when using 4D vectors instead of 3D ones to get advantage of SSE/AltiVec vectorization.
  *
  * \sa MatrixBase::cross()
  */
template<typename Derived>
template<typename OtherDerived>
inline typename MatrixBase<Derived>::PlainObject
MatrixBase<Derived>::cross3(const MatrixBase<OtherDerived>& other) const
{
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Derived,4)
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(OtherDerived,4)

  typedef typename internal::nested<Derived,2>::type DerivedNested;
  typedef typename internal::nested<OtherDerived,2>::type OtherDerivedNested;
  const DerivedNested lhs(derived());
  const OtherDerivedNested rhs(other.derived());

  return internal::cross3_impl<Architecture::Target,
                        typename internal::remove_all<DerivedNested>::type,
                        typename internal::remove_all<OtherDerivedNested>::type>::run(lhs,rhs);
}

/** \returns a matrix expression of the cross product of each column or row
  * of the referenced expression with the \a other vector.
  *
  * The referenced matrix must have one dimension equal to 3.
  * The result matrix has the same dimensions than the referenced one.
  *
  * \geometry_module
  *
  * \sa MatrixBase::cross() */
template<typename ExpressionType, int Direction>
template<typename OtherDerived>
const typename VectorwiseOp<ExpressionType,Direction>::CrossReturnType
VectorwiseOp<ExpressionType,Direction>::cross(const MatrixBase<OtherDerived>& other) const
{
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(OtherDerived,3)
  EIGEN_STATIC_ASSERT((internal::is_same<Scalar, typename OtherDerived::Scalar>::value),
    YOU_MIXED_DIFFERENT_NUMERIC_TYPES__YOU_NEED_TO_USE_THE_CAST_METHOD_OF_MATRIXBASE_TO_CAST_NUMERIC_TYPES_EXPLICITLY)

  CrossReturnType res(_expression().rows(),_expression().cols());
  if(Direction==Vertical)
  {
    eigen_assert(CrossReturnType::RowsAtCompileTime==3 && "the matrix must have exactly 3 rows");
    res.row(0) = (_expression().row(1) * other.coeff(2) - _expression().row(2) * other.coeff(1)).conjugate();
    res.row(1) = (_expression().row(2) * other.coeff(0) - _expression().row(0) * other.coeff(2)).conjugate();
    res.row(2) = (_expression().row(0) * other.coeff(1) - _expression().row(1) * other.coeff(0)).conjugate();
  }
  else
  {
    eigen_assert(CrossReturnType::ColsAtCompileTime==3 && "the matrix must have exactly 3 columns");
    res.col(0) = (_expression().col(1) * other.coeff(2) - _expression().col(2) * other.coeff(1)).conjugate();
    res.col(1) = (_expression().col(2) * other.coeff(0) - _expression().col(0) * other.coeff(2)).conjugate();
    res.col(2) = (_expression().col(0) * other.coeff(1) - _expression().col(1) * other.coeff(0)).conjugate();
  }
  return res;
}

namespace internal {

template<typename Derived, int Size = Derived::SizeAtCompileTime>
struct unitOrthogonal_selector
{
  typedef typename plain_matrix_type<Derived>::type VectorType;
  typedef typename traits<Derived>::Scalar Scalar;
  typedef typename NumTraits<Scalar>::Real RealScalar;
  typedef typename Derived::Index Index;
  typedef Matrix<Scalar,2,1> Vector2;
  inline static VectorType run(const Derived& src)
  {
    VectorType perp = VectorType::Zero(src.size());
    Index maxi = 0;
    Index sndi = 0;
    src.cwiseAbs().maxCoeff(&maxi);
    if (maxi==0)
      sndi = 1;
    RealScalar invnm = RealScalar(1)/(Vector2() << src.coeff(sndi),src.coeff(maxi)).finished().norm();
    perp.coeffRef(maxi) = -conj(src.coeff(sndi)) * invnm;
    perp.coeffRef(sndi) =  conj(src.coeff(maxi)) * invnm;

    return perp;
   }
};

template<typename Derived>
struct unitOrthogonal_selector<Derived,3>
{
  typedef typename plain_matrix_type<Derived>::type VectorType;
  typedef typename traits<Derived>::Scalar Scalar;
  typedef typename NumTraits<Scalar>::Real RealScalar;
  inline static VectorType run(const Derived& src)
  {
    VectorType perp;
    /* Let us compute the crossed product of *this with a vector
     * that is not too close to being colinear to *this.
     */

    /* unless the x and y coords are both close to zero, we can
     * simply take ( -y, x, 0 ) and normalize it.
     */
    if((!isMuchSmallerThan(src.x(), src.z()))
    || (!isMuchSmallerThan(src.y(), src.z())))
    {
      RealScalar invnm = RealScalar(1)/src.template head<2>().norm();
      perp.coeffRef(0) = -conj(src.y())*invnm;
      perp.coeffRef(1) = conj(src.x())*invnm;
      perp.coeffRef(2) = 0;
    }
    /* if both x and y are close to zero, then the vector is close
     * to the z-axis, so it's far from colinear to the x-axis for instance.
     * So we take the crossed product with (1,0,0) and normalize it.
     */
    else
    {
      RealScalar invnm = RealScalar(1)/src.template tail<2>().norm();
      perp.coeffRef(0) = 0;
      perp.coeffRef(1) = -conj(src.z())*invnm;
      perp.coeffRef(2) = conj(src.y())*invnm;
    }

    return perp;
   }
};

template<typename Derived>
struct unitOrthogonal_selector<Derived,2>
{
  typedef typename plain_matrix_type<Derived>::type VectorType;
  inline static VectorType run(const Derived& src)
  { return VectorType(-conj(src.y()), conj(src.x())).normalized(); }
};

} // end namespace internal

/** \returns a unit vector which is orthogonal to \c *this
  *
  * The size of \c *this must be at least 2. If the size is exactly 2,
  * then the returned vector is a counter clock wise rotation of \c *this, i.e., (-y,x).normalized().
  *
  * \sa cross()
  */
template<typename Derived>
typename MatrixBase<Derived>::PlainObject
MatrixBase<Derived>::unitOrthogonal() const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return internal::unitOrthogonal_selector<Derived>::run(derived());
}

#endif // EIGEN_ORTHOMETHODS_H
