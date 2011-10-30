// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2010 Gael Guennebaud <gael.guennebaud@inria.fr>
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

#ifndef EIGEN2_MATH_FUNCTIONS_H
#define EIGEN2_MATH_FUNCTIONS_H

template<typename T> inline typename NumTraits<T>::Real ei_real(const T& x) { return internal::real(x); }
template<typename T> inline typename NumTraits<T>::Real ei_imag(const T& x) { return internal::imag(x); }
template<typename T> inline T ei_conj(const T& x) { return internal::conj(x); }
template<typename T> inline typename NumTraits<T>::Real ei_abs (const T& x) { return internal::abs(x); }
template<typename T> inline typename NumTraits<T>::Real ei_abs2(const T& x) { return internal::abs2(x); }
template<typename T> inline T ei_sqrt(const T& x) { return internal::sqrt(x); }
template<typename T> inline T ei_exp (const T& x) { return internal::exp(x); }
template<typename T> inline T ei_log (const T& x) { return internal::log(x); }
template<typename T> inline T ei_sin (const T& x) { return internal::sin(x); }
template<typename T> inline T ei_cos (const T& x) { return internal::cos(x); }
template<typename T> inline T ei_atan2(const T& x,const T& y) { return internal::atan2(x,y); }
template<typename T> inline T ei_pow (const T& x,const T& y) { return internal::pow(x,y); }
template<typename T> inline T ei_random () { return internal::random<T>(); }
template<typename T> inline T ei_random (const T& x, const T& y) { return internal::random(x, y); }

template<typename T> inline T precision () { return NumTraits<T>::dummy_precision(); }
template<typename T> inline T machine_epsilon () { return NumTraits<T>::epsilon(); }


template<typename Scalar, typename OtherScalar>
inline bool ei_isMuchSmallerThan(const Scalar& x, const OtherScalar& y,
                                   typename NumTraits<Scalar>::Real precision = NumTraits<Scalar>::dummy_precision())
{
  return internal::isMuchSmallerThan(x, y, precision);
}

template<typename Scalar>
inline bool ei_isApprox(const Scalar& x, const Scalar& y,
                          typename NumTraits<Scalar>::Real precision = NumTraits<Scalar>::dummy_precision())
{
  return internal::isApprox(x, y, precision);
}

template<typename Scalar>
inline bool ei_isApproxOrLessThan(const Scalar& x, const Scalar& y,
                                    typename NumTraits<Scalar>::Real precision = NumTraits<Scalar>::dummy_precision())
{
  return internal::isApproxOrLessThan(x, y, precision);
}

#endif // EIGEN2_MATH_FUNCTIONS_H
