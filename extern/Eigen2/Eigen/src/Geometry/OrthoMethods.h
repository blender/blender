// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
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
  */
template<typename Derived>
template<typename OtherDerived>
inline typename MatrixBase<Derived>::PlainMatrixType
MatrixBase<Derived>::cross(const MatrixBase<OtherDerived>& other) const
{
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Derived,3)

  // Note that there is no need for an expression here since the compiler
  // optimize such a small temporary very well (even within a complex expression)
  const typename ei_nested<Derived,2>::type lhs(derived());
  const typename ei_nested<OtherDerived,2>::type rhs(other.derived());
  return typename ei_plain_matrix_type<Derived>::type(
    lhs.coeff(1) * rhs.coeff(2) - lhs.coeff(2) * rhs.coeff(1),
    lhs.coeff(2) * rhs.coeff(0) - lhs.coeff(0) * rhs.coeff(2),
    lhs.coeff(0) * rhs.coeff(1) - lhs.coeff(1) * rhs.coeff(0)
  );
}

template<typename Derived, int Size = Derived::SizeAtCompileTime>
struct ei_unitOrthogonal_selector
{
  typedef typename ei_plain_matrix_type<Derived>::type VectorType;
  typedef typename ei_traits<Derived>::Scalar Scalar;
  typedef typename NumTraits<Scalar>::Real RealScalar;
  inline static VectorType run(const Derived& src)
  {
    VectorType perp(src.size());
    /* Let us compute the crossed product of *this with a vector
     * that is not too close to being colinear to *this.
     */

    /* unless the x and y coords are both close to zero, we can
     * simply take ( -y, x, 0 ) and normalize it.
     */
    if((!ei_isMuchSmallerThan(src.x(), src.z()))
    || (!ei_isMuchSmallerThan(src.y(), src.z())))
    {
      RealScalar invnm = RealScalar(1)/src.template start<2>().norm();
      perp.coeffRef(0) = -ei_conj(src.y())*invnm;
      perp.coeffRef(1) = ei_conj(src.x())*invnm;
      perp.coeffRef(2) = 0;
    }
    /* if both x and y are close to zero, then the vector is close
     * to the z-axis, so it's far from colinear to the x-axis for instance.
     * So we take the crossed product with (1,0,0) and normalize it.
     */
    else
    {
      RealScalar invnm = RealScalar(1)/src.template end<2>().norm();
      perp.coeffRef(0) = 0;
      perp.coeffRef(1) = -ei_conj(src.z())*invnm;
      perp.coeffRef(2) = ei_conj(src.y())*invnm;
    }
    if( (Derived::SizeAtCompileTime!=Dynamic && Derived::SizeAtCompileTime>3)
     || (Derived::SizeAtCompileTime==Dynamic && src.size()>3) )
      perp.end(src.size()-3).setZero();

    return perp;
   }
};

template<typename Derived>
struct ei_unitOrthogonal_selector<Derived,2>
{
  typedef typename ei_plain_matrix_type<Derived>::type VectorType;
  inline static VectorType run(const Derived& src)
  { return VectorType(-ei_conj(src.y()), ei_conj(src.x())).normalized(); }
};

/** \returns a unit vector which is orthogonal to \c *this
  *
  * The size of \c *this must be at least 2. If the size is exactly 2,
  * then the returned vector is a counter clock wise rotation of \c *this, i.e., (-y,x).normalized().
  *
  * \sa cross()
  */
template<typename Derived>
typename MatrixBase<Derived>::PlainMatrixType
MatrixBase<Derived>::unitOrthogonal() const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return ei_unitOrthogonal_selector<Derived>::run(derived());
}

#endif // EIGEN_ORTHOMETHODS_H
