// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
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

#ifndef EIGEN_ARRAY_FUNCTORS_H
#define EIGEN_ARRAY_FUNCTORS_H

/** \internal
  * \array_module
  *
  * \brief Template functor to add a scalar to a fixed other one
  *
  * \sa class CwiseUnaryOp, Array::operator+
  */
/* If you wonder why doing the ei_pset1() in packetOp() is an optimization check ei_scalar_multiple_op */
template<typename Scalar>
struct ei_scalar_add_op {
  typedef typename ei_packet_traits<Scalar>::type PacketScalar;
  // FIXME default copy constructors seems bugged with std::complex<>
  inline ei_scalar_add_op(const ei_scalar_add_op& other) : m_other(other.m_other) { }
  inline ei_scalar_add_op(const Scalar& other) : m_other(other) { }
  inline Scalar operator() (const Scalar& a) const { return a + m_other; }
  inline const PacketScalar packetOp(const PacketScalar& a) const
  { return ei_padd(a, ei_pset1(m_other)); }
  const Scalar m_other;
private:
  ei_scalar_add_op& operator=(const ei_scalar_add_op&);
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_add_op<Scalar> >
{ enum { Cost = NumTraits<Scalar>::AddCost, PacketAccess = ei_packet_traits<Scalar>::size>1 }; };

/** \internal
  *
  * \array_module
  *
  * \brief Template functor to compute the square root of a scalar
  *
  * \sa class CwiseUnaryOp, Cwise::sqrt()
  */
template<typename Scalar> struct ei_scalar_sqrt_op EIGEN_EMPTY_STRUCT {
  inline const Scalar operator() (const Scalar& a) const { return ei_sqrt(a); }
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_sqrt_op<Scalar> >
{ enum { Cost = 5 * NumTraits<Scalar>::MulCost, PacketAccess = false }; };

/** \internal
  *
  * \array_module
  *
  * \brief Template functor to compute the exponential of a scalar
  *
  * \sa class CwiseUnaryOp, Cwise::exp()
  */
template<typename Scalar> struct ei_scalar_exp_op EIGEN_EMPTY_STRUCT {
  inline const Scalar operator() (const Scalar& a) const { return ei_exp(a); }
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_exp_op<Scalar> >
{ enum { Cost = 5 * NumTraits<Scalar>::MulCost, PacketAccess = false }; };

/** \internal
  *
  * \array_module
  *
  * \brief Template functor to compute the logarithm of a scalar
  *
  * \sa class CwiseUnaryOp, Cwise::log()
  */
template<typename Scalar> struct ei_scalar_log_op EIGEN_EMPTY_STRUCT {
  inline const Scalar operator() (const Scalar& a) const { return ei_log(a); }
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_log_op<Scalar> >
{ enum { Cost = 5 * NumTraits<Scalar>::MulCost, PacketAccess = false }; };

/** \internal
  *
  * \array_module
  *
  * \brief Template functor to compute the cosine of a scalar
  *
  * \sa class CwiseUnaryOp, Cwise::cos()
  */
template<typename Scalar> struct ei_scalar_cos_op EIGEN_EMPTY_STRUCT {
  inline const Scalar operator() (const Scalar& a) const { return ei_cos(a); }
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_cos_op<Scalar> >
{ enum { Cost = 5 * NumTraits<Scalar>::MulCost, PacketAccess = false }; };

/** \internal
  *
  * \array_module
  *
  * \brief Template functor to compute the sine of a scalar
  *
  * \sa class CwiseUnaryOp, Cwise::sin()
  */
template<typename Scalar> struct ei_scalar_sin_op EIGEN_EMPTY_STRUCT {
  inline const Scalar operator() (const Scalar& a) const { return ei_sin(a); }
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_sin_op<Scalar> >
{ enum { Cost = 5 * NumTraits<Scalar>::MulCost, PacketAccess = false }; };

/** \internal
  *
  * \array_module
  *
  * \brief Template functor to raise a scalar to a power
  *
  * \sa class CwiseUnaryOp, Cwise::pow
  */
template<typename Scalar>
struct ei_scalar_pow_op {
  // FIXME default copy constructors seems bugged with std::complex<>
  inline ei_scalar_pow_op(const ei_scalar_pow_op& other) : m_exponent(other.m_exponent) { }
  inline ei_scalar_pow_op(const Scalar& exponent) : m_exponent(exponent) {}
  inline Scalar operator() (const Scalar& a) const { return ei_pow(a, m_exponent); }
  const Scalar m_exponent;
private:
  ei_scalar_pow_op& operator=(const ei_scalar_pow_op&);
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_pow_op<Scalar> >
{ enum { Cost = 5 * NumTraits<Scalar>::MulCost, PacketAccess = false }; };

/** \internal
  *
  * \array_module
  *
  * \brief Template functor to compute the inverse of a scalar
  *
  * \sa class CwiseUnaryOp, Cwise::inverse()
  */
template<typename Scalar>
struct ei_scalar_inverse_op {
  inline Scalar operator() (const Scalar& a) const { return Scalar(1)/a; }
  template<typename PacketScalar>
  inline const PacketScalar packetOp(const PacketScalar& a) const
  { return ei_pdiv(ei_pset1(Scalar(1)),a); }
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_inverse_op<Scalar> >
{ enum { Cost = NumTraits<Scalar>::MulCost, PacketAccess = int(ei_packet_traits<Scalar>::size)>1 }; };

/** \internal
  *
  * \array_module
  *
  * \brief Template functor to compute the square of a scalar
  *
  * \sa class CwiseUnaryOp, Cwise::square()
  */
template<typename Scalar>
struct ei_scalar_square_op {
  inline Scalar operator() (const Scalar& a) const { return a*a; }
  template<typename PacketScalar>
  inline const PacketScalar packetOp(const PacketScalar& a) const
  { return ei_pmul(a,a); }
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_square_op<Scalar> >
{ enum { Cost = NumTraits<Scalar>::MulCost, PacketAccess = int(ei_packet_traits<Scalar>::size)>1 }; };

/** \internal
  *
  * \array_module
  *
  * \brief Template functor to compute the cube of a scalar
  *
  * \sa class CwiseUnaryOp, Cwise::cube()
  */
template<typename Scalar>
struct ei_scalar_cube_op {
  inline Scalar operator() (const Scalar& a) const { return a*a*a; }
  template<typename PacketScalar>
  inline const PacketScalar packetOp(const PacketScalar& a) const
  { return ei_pmul(a,ei_pmul(a,a)); }
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_cube_op<Scalar> >
{ enum { Cost = 2*NumTraits<Scalar>::MulCost, PacketAccess = int(ei_packet_traits<Scalar>::size)>1 }; };

// default ei_functor_traits for STL functors:

template<typename T>
struct ei_functor_traits<std::multiplies<T> >
{ enum { Cost = NumTraits<T>::MulCost, PacketAccess = false }; };

template<typename T>
struct ei_functor_traits<std::divides<T> >
{ enum { Cost = NumTraits<T>::MulCost, PacketAccess = false }; };

template<typename T>
struct ei_functor_traits<std::plus<T> >
{ enum { Cost = NumTraits<T>::AddCost, PacketAccess = false }; };

template<typename T>
struct ei_functor_traits<std::minus<T> >
{ enum { Cost = NumTraits<T>::AddCost, PacketAccess = false }; };

template<typename T>
struct ei_functor_traits<std::negate<T> >
{ enum { Cost = NumTraits<T>::AddCost, PacketAccess = false }; };

template<typename T>
struct ei_functor_traits<std::logical_or<T> >
{ enum { Cost = 1, PacketAccess = false }; };

template<typename T>
struct ei_functor_traits<std::logical_and<T> >
{ enum { Cost = 1, PacketAccess = false }; };

template<typename T>
struct ei_functor_traits<std::logical_not<T> >
{ enum { Cost = 1, PacketAccess = false }; };

template<typename T>
struct ei_functor_traits<std::greater<T> >
{ enum { Cost = 1, PacketAccess = false }; };

template<typename T>
struct ei_functor_traits<std::less<T> >
{ enum { Cost = 1, PacketAccess = false }; };

template<typename T>
struct ei_functor_traits<std::greater_equal<T> >
{ enum { Cost = 1, PacketAccess = false }; };

template<typename T>
struct ei_functor_traits<std::less_equal<T> >
{ enum { Cost = 1, PacketAccess = false }; };

template<typename T>
struct ei_functor_traits<std::equal_to<T> >
{ enum { Cost = 1, PacketAccess = false }; };

template<typename T>
struct ei_functor_traits<std::not_equal_to<T> >
{ enum { Cost = 1, PacketAccess = false }; };

template<typename T>
struct ei_functor_traits<std::binder2nd<T> >
{ enum { Cost = ei_functor_traits<T>::Cost, PacketAccess = false }; };

template<typename T>
struct ei_functor_traits<std::binder1st<T> >
{ enum { Cost = ei_functor_traits<T>::Cost, PacketAccess = false }; };

template<typename T>
struct ei_functor_traits<std::unary_negate<T> >
{ enum { Cost = 1 + ei_functor_traits<T>::Cost, PacketAccess = false }; };

template<typename T>
struct ei_functor_traits<std::binary_negate<T> >
{ enum { Cost = 1 + ei_functor_traits<T>::Cost, PacketAccess = false }; };

#ifdef EIGEN_STDEXT_SUPPORT

template<typename T0,typename T1>
struct ei_functor_traits<std::project1st<T0,T1> >
{ enum { Cost = 0, PacketAccess = false }; };

template<typename T0,typename T1>
struct ei_functor_traits<std::project2nd<T0,T1> >
{ enum { Cost = 0, PacketAccess = false }; };

template<typename T0,typename T1>
struct ei_functor_traits<std::select2nd<std::pair<T0,T1> > >
{ enum { Cost = 0, PacketAccess = false }; };

template<typename T0,typename T1>
struct ei_functor_traits<std::select1st<std::pair<T0,T1> > >
{ enum { Cost = 0, PacketAccess = false }; };

template<typename T0,typename T1>
struct ei_functor_traits<std::unary_compose<T0,T1> >
{ enum { Cost = ei_functor_traits<T0>::Cost + ei_functor_traits<T1>::Cost, PacketAccess = false }; };

template<typename T0,typename T1,typename T2>
struct ei_functor_traits<std::binary_compose<T0,T1,T2> >
{ enum { Cost = ei_functor_traits<T0>::Cost + ei_functor_traits<T1>::Cost + ei_functor_traits<T2>::Cost, PacketAccess = false }; };

#endif // EIGEN_STDEXT_SUPPORT

#endif // EIGEN_ARRAY_FUNCTORS_H
