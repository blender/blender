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

#ifndef EIGEN_FUNCTORS_H
#define EIGEN_FUNCTORS_H

// associative functors:

/** \internal
  * \brief Template functor to compute the sum of two scalars
  *
  * \sa class CwiseBinaryOp, MatrixBase::operator+, class PartialRedux, MatrixBase::sum()
  */
template<typename Scalar> struct ei_scalar_sum_op EIGEN_EMPTY_STRUCT {
  EIGEN_STRONG_INLINE const Scalar operator() (const Scalar& a, const Scalar& b) const { return a + b; }
  template<typename PacketScalar>
  EIGEN_STRONG_INLINE const PacketScalar packetOp(const PacketScalar& a, const PacketScalar& b) const
  { return ei_padd(a,b); }
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_sum_op<Scalar> > {
  enum {
    Cost = NumTraits<Scalar>::AddCost,
    PacketAccess = ei_packet_traits<Scalar>::size>1
  };
};

/** \internal
  * \brief Template functor to compute the product of two scalars
  *
  * \sa class CwiseBinaryOp, Cwise::operator*(), class PartialRedux, MatrixBase::redux()
  */
template<typename Scalar> struct ei_scalar_product_op EIGEN_EMPTY_STRUCT {
  EIGEN_STRONG_INLINE const Scalar operator() (const Scalar& a, const Scalar& b) const { return a * b; }
  template<typename PacketScalar>
  EIGEN_STRONG_INLINE const PacketScalar packetOp(const PacketScalar& a, const PacketScalar& b) const
  { return ei_pmul(a,b); }
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_product_op<Scalar> > {
  enum {
    Cost = NumTraits<Scalar>::MulCost,
    PacketAccess = ei_packet_traits<Scalar>::size>1
  };
};

/** \internal
  * \brief Template functor to compute the min of two scalars
  *
  * \sa class CwiseBinaryOp, MatrixBase::cwiseMin, class PartialRedux, MatrixBase::minCoeff()
  */
template<typename Scalar> struct ei_scalar_min_op EIGEN_EMPTY_STRUCT {
  EIGEN_STRONG_INLINE const Scalar operator() (const Scalar& a, const Scalar& b) const { return std::min(a, b); }
  template<typename PacketScalar>
  EIGEN_STRONG_INLINE const PacketScalar packetOp(const PacketScalar& a, const PacketScalar& b) const
  { return ei_pmin(a,b); }
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_min_op<Scalar> > {
  enum {
    Cost = NumTraits<Scalar>::AddCost,
    PacketAccess = ei_packet_traits<Scalar>::size>1
  };
};

/** \internal
  * \brief Template functor to compute the max of two scalars
  *
  * \sa class CwiseBinaryOp, MatrixBase::cwiseMax, class PartialRedux, MatrixBase::maxCoeff()
  */
template<typename Scalar> struct ei_scalar_max_op EIGEN_EMPTY_STRUCT {
  EIGEN_STRONG_INLINE const Scalar operator() (const Scalar& a, const Scalar& b) const { return std::max(a, b); }
  template<typename PacketScalar>
  EIGEN_STRONG_INLINE const PacketScalar packetOp(const PacketScalar& a, const PacketScalar& b) const
  { return ei_pmax(a,b); }
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_max_op<Scalar> > {
  enum {
    Cost = NumTraits<Scalar>::AddCost,
    PacketAccess = ei_packet_traits<Scalar>::size>1
  };
};


// other binary functors:

/** \internal
  * \brief Template functor to compute the difference of two scalars
  *
  * \sa class CwiseBinaryOp, MatrixBase::operator-
  */
template<typename Scalar> struct ei_scalar_difference_op EIGEN_EMPTY_STRUCT {
  EIGEN_STRONG_INLINE const Scalar operator() (const Scalar& a, const Scalar& b) const { return a - b; }
  template<typename PacketScalar>
  EIGEN_STRONG_INLINE const PacketScalar packetOp(const PacketScalar& a, const PacketScalar& b) const
  { return ei_psub(a,b); }
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_difference_op<Scalar> > {
  enum {
    Cost = NumTraits<Scalar>::AddCost,
    PacketAccess = ei_packet_traits<Scalar>::size>1
  };
};

/** \internal
  * \brief Template functor to compute the quotient of two scalars
  *
  * \sa class CwiseBinaryOp, Cwise::operator/()
  */
template<typename Scalar> struct ei_scalar_quotient_op EIGEN_EMPTY_STRUCT {
  EIGEN_STRONG_INLINE const Scalar operator() (const Scalar& a, const Scalar& b) const { return a / b; }
  template<typename PacketScalar>
  EIGEN_STRONG_INLINE const PacketScalar packetOp(const PacketScalar& a, const PacketScalar& b) const
  { return ei_pdiv(a,b); }
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_quotient_op<Scalar> > {
  enum {
    Cost = 2 * NumTraits<Scalar>::MulCost,
    PacketAccess = ei_packet_traits<Scalar>::size>1
                  #if (defined EIGEN_VECTORIZE_SSE)
                  && NumTraits<Scalar>::HasFloatingPoint
                  #endif
  };
};

// unary functors:

/** \internal
  * \brief Template functor to compute the opposite of a scalar
  *
  * \sa class CwiseUnaryOp, MatrixBase::operator-
  */
template<typename Scalar> struct ei_scalar_opposite_op EIGEN_EMPTY_STRUCT {
  EIGEN_STRONG_INLINE const Scalar operator() (const Scalar& a) const { return -a; }
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_opposite_op<Scalar> >
{ enum { Cost = NumTraits<Scalar>::AddCost, PacketAccess = false }; };

/** \internal
  * \brief Template functor to compute the absolute value of a scalar
  *
  * \sa class CwiseUnaryOp, Cwise::abs
  */
template<typename Scalar> struct ei_scalar_abs_op EIGEN_EMPTY_STRUCT {
  typedef typename NumTraits<Scalar>::Real result_type;
  EIGEN_STRONG_INLINE const result_type operator() (const Scalar& a) const { return ei_abs(a); }
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_abs_op<Scalar> >
{
  enum {
    Cost = NumTraits<Scalar>::AddCost,
    PacketAccess = false // this could actually be vectorized with SSSE3.
  };
};

/** \internal
  * \brief Template functor to compute the squared absolute value of a scalar
  *
  * \sa class CwiseUnaryOp, Cwise::abs2
  */
template<typename Scalar> struct ei_scalar_abs2_op EIGEN_EMPTY_STRUCT {
  typedef typename NumTraits<Scalar>::Real result_type;
  EIGEN_STRONG_INLINE const result_type operator() (const Scalar& a) const { return ei_abs2(a); }
  template<typename PacketScalar>
  EIGEN_STRONG_INLINE const PacketScalar packetOp(const PacketScalar& a) const
  { return ei_pmul(a,a); }
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_abs2_op<Scalar> >
{ enum { Cost = NumTraits<Scalar>::MulCost, PacketAccess = int(ei_packet_traits<Scalar>::size)>1 }; };

/** \internal
  * \brief Template functor to compute the conjugate of a complex value
  *
  * \sa class CwiseUnaryOp, MatrixBase::conjugate()
  */
template<typename Scalar> struct ei_scalar_conjugate_op EIGEN_EMPTY_STRUCT {
  EIGEN_STRONG_INLINE const Scalar operator() (const Scalar& a) const { return ei_conj(a); }
  template<typename PacketScalar>
  EIGEN_STRONG_INLINE const PacketScalar packetOp(const PacketScalar& a) const { return a; }
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_conjugate_op<Scalar> >
{
  enum {
    Cost = NumTraits<Scalar>::IsComplex ? NumTraits<Scalar>::AddCost : 0,
    PacketAccess = int(ei_packet_traits<Scalar>::size)>1
  };
};

/** \internal
  * \brief Template functor to cast a scalar to another type
  *
  * \sa class CwiseUnaryOp, MatrixBase::cast()
  */
template<typename Scalar, typename NewType>
struct ei_scalar_cast_op EIGEN_EMPTY_STRUCT {
  typedef NewType result_type;
  EIGEN_STRONG_INLINE const NewType operator() (const Scalar& a) const { return static_cast<NewType>(a); }
};
template<typename Scalar, typename NewType>
struct ei_functor_traits<ei_scalar_cast_op<Scalar,NewType> >
{ enum { Cost = ei_is_same_type<Scalar, NewType>::ret ? 0 : NumTraits<NewType>::AddCost, PacketAccess = false }; };

/** \internal
  * \brief Template functor to extract the real part of a complex
  *
  * \sa class CwiseUnaryOp, MatrixBase::real()
  */
template<typename Scalar>
struct ei_scalar_real_op EIGEN_EMPTY_STRUCT {
  typedef typename NumTraits<Scalar>::Real result_type;
  EIGEN_STRONG_INLINE result_type operator() (const Scalar& a) const { return ei_real(a); }
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_real_op<Scalar> >
{ enum { Cost = 0, PacketAccess = false }; };

/** \internal
  * \brief Template functor to extract the imaginary part of a complex
  *
  * \sa class CwiseUnaryOp, MatrixBase::imag()
  */
template<typename Scalar>
struct ei_scalar_imag_op EIGEN_EMPTY_STRUCT {
  typedef typename NumTraits<Scalar>::Real result_type;
  EIGEN_STRONG_INLINE result_type operator() (const Scalar& a) const { return ei_imag(a); }
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_imag_op<Scalar> >
{ enum { Cost = 0, PacketAccess = false }; };

/** \internal
  * \brief Template functor to multiply a scalar by a fixed other one
  *
  * \sa class CwiseUnaryOp, MatrixBase::operator*, MatrixBase::operator/
  */
/* NOTE why doing the ei_pset1() in packetOp *is* an optimization ?
 * indeed it seems better to declare m_other as a PacketScalar and do the ei_pset1() once
 * in the constructor. However, in practice:
 *  - GCC does not like m_other as a PacketScalar and generate a load every time it needs it
 *  - one the other hand GCC is able to moves the ei_pset1() away the loop :)
 *  - simpler code ;)
 * (ICC and gcc 4.4 seems to perform well in both cases, the issue is visible with y = a*x + b*y)
 */
template<typename Scalar>
struct ei_scalar_multiple_op {
  typedef typename ei_packet_traits<Scalar>::type PacketScalar;
  // FIXME default copy constructors seems bugged with std::complex<>
  EIGEN_STRONG_INLINE ei_scalar_multiple_op(const ei_scalar_multiple_op& other) : m_other(other.m_other) { }
  EIGEN_STRONG_INLINE ei_scalar_multiple_op(const Scalar& other) : m_other(other) { }
  EIGEN_STRONG_INLINE Scalar operator() (const Scalar& a) const { return a * m_other; }
  EIGEN_STRONG_INLINE const PacketScalar packetOp(const PacketScalar& a) const
  { return ei_pmul(a, ei_pset1(m_other)); }
  const Scalar m_other;
private:
  ei_scalar_multiple_op& operator=(const ei_scalar_multiple_op&);
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_multiple_op<Scalar> >
{ enum { Cost = NumTraits<Scalar>::MulCost, PacketAccess = ei_packet_traits<Scalar>::size>1 }; };

template<typename Scalar, bool HasFloatingPoint>
struct ei_scalar_quotient1_impl {
  typedef typename ei_packet_traits<Scalar>::type PacketScalar;
  // FIXME default copy constructors seems bugged with std::complex<>
  EIGEN_STRONG_INLINE ei_scalar_quotient1_impl(const ei_scalar_quotient1_impl& other) : m_other(other.m_other) { }
  EIGEN_STRONG_INLINE ei_scalar_quotient1_impl(const Scalar& other) : m_other(static_cast<Scalar>(1) / other) {}
  EIGEN_STRONG_INLINE Scalar operator() (const Scalar& a) const { return a * m_other; }
  EIGEN_STRONG_INLINE const PacketScalar packetOp(const PacketScalar& a) const
  { return ei_pmul(a, ei_pset1(m_other)); }
  const Scalar m_other;
private:
  ei_scalar_quotient1_impl& operator=(const ei_scalar_quotient1_impl&);
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_quotient1_impl<Scalar,true> >
{ enum { Cost = NumTraits<Scalar>::MulCost, PacketAccess = ei_packet_traits<Scalar>::size>1 }; };

template<typename Scalar>
struct ei_scalar_quotient1_impl<Scalar,false> {
  // FIXME default copy constructors seems bugged with std::complex<>
  EIGEN_STRONG_INLINE ei_scalar_quotient1_impl(const ei_scalar_quotient1_impl& other) : m_other(other.m_other) { }
  EIGEN_STRONG_INLINE ei_scalar_quotient1_impl(const Scalar& other) : m_other(other) {}
  EIGEN_STRONG_INLINE Scalar operator() (const Scalar& a) const { return a / m_other; }
  const Scalar m_other;
private:
  ei_scalar_quotient1_impl& operator=(const ei_scalar_quotient1_impl&);
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_quotient1_impl<Scalar,false> >
{ enum { Cost = 2 * NumTraits<Scalar>::MulCost, PacketAccess = false }; };

/** \internal
  * \brief Template functor to divide a scalar by a fixed other one
  *
  * This functor is used to implement the quotient of a matrix by
  * a scalar where the scalar type is not necessarily a floating point type.
  *
  * \sa class CwiseUnaryOp, MatrixBase::operator/
  */
template<typename Scalar>
struct ei_scalar_quotient1_op : ei_scalar_quotient1_impl<Scalar, NumTraits<Scalar>::HasFloatingPoint > {
  EIGEN_STRONG_INLINE ei_scalar_quotient1_op(const Scalar& other)
    : ei_scalar_quotient1_impl<Scalar, NumTraits<Scalar>::HasFloatingPoint >(other) {}
private:
  ei_scalar_quotient1_op& operator=(const ei_scalar_quotient1_op&);
};

// nullary functors

template<typename Scalar>
struct ei_scalar_constant_op {
  typedef typename ei_packet_traits<Scalar>::type PacketScalar;
  EIGEN_STRONG_INLINE ei_scalar_constant_op(const ei_scalar_constant_op& other) : m_other(other.m_other) { }
  EIGEN_STRONG_INLINE ei_scalar_constant_op(const Scalar& other) : m_other(other) { }
  EIGEN_STRONG_INLINE const Scalar operator() (int, int = 0) const { return m_other; }
  EIGEN_STRONG_INLINE const PacketScalar packetOp() const { return ei_pset1(m_other); }
  const Scalar m_other;
private:
  ei_scalar_constant_op& operator=(const ei_scalar_constant_op&);
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_constant_op<Scalar> >
{ enum { Cost = 1, PacketAccess = ei_packet_traits<Scalar>::size>1, IsRepeatable = true }; };

template<typename Scalar> struct ei_scalar_identity_op EIGEN_EMPTY_STRUCT {
  EIGEN_STRONG_INLINE ei_scalar_identity_op(void) {}
  EIGEN_STRONG_INLINE const Scalar operator() (int row, int col) const { return row==col ? Scalar(1) : Scalar(0); }
};
template<typename Scalar>
struct ei_functor_traits<ei_scalar_identity_op<Scalar> >
{ enum { Cost = NumTraits<Scalar>::AddCost, PacketAccess = false, IsRepeatable = true }; };

// allow to add new functors and specializations of ei_functor_traits from outside Eigen.
// this macro is really needed because ei_functor_traits must be specialized after it is declared but before it is used...
#ifdef EIGEN_FUNCTORS_PLUGIN
#include EIGEN_FUNCTORS_PLUGIN
#endif

// all functors allow linear access, except ei_scalar_identity_op. So we fix here a quick meta
// to indicate whether a functor allows linear access, just always answering 'yes' except for
// ei_scalar_identity_op.
template<typename Functor> struct ei_functor_has_linear_access { enum { ret = 1 }; };
template<typename Scalar> struct ei_functor_has_linear_access<ei_scalar_identity_op<Scalar> > { enum { ret = 0 }; };

// in CwiseBinaryOp, we require the Lhs and Rhs to have the same scalar type, except for multiplication
// where we only require them to have the same _real_ scalar type so one may multiply, say, float by complex<float>.
template<typename Functor> struct ei_functor_allows_mixing_real_and_complex { enum { ret = 0 }; };
template<typename Scalar> struct ei_functor_allows_mixing_real_and_complex<ei_scalar_product_op<Scalar> > { enum { ret = 1 }; };

#endif // EIGEN_FUNCTORS_H
