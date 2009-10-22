// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
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

#ifndef EIGEN_MATHFUNCTIONS_H
#define EIGEN_MATHFUNCTIONS_H

template<typename T> inline typename NumTraits<T>::Real precision();
template<typename T> inline typename NumTraits<T>::Real machine_epsilon();
template<typename T> inline T ei_random(T a, T b);
template<typename T> inline T ei_random();
template<typename T> inline T ei_random_amplitude()
{
  if(NumTraits<T>::HasFloatingPoint) return static_cast<T>(1);
  else return static_cast<T>(10);
}

template<typename T> inline T ei_hypot(T x, T y)
{
  T _x = ei_abs(x);
  T _y = ei_abs(y);
  T p = std::max(_x, _y);
  T q = std::min(_x, _y);
  T qp = q/p;
  return p * ei_sqrt(T(1) + qp*qp);
}

/**************
***   int   ***
**************/

template<> inline int precision<int>() { return 0; }
template<> inline int machine_epsilon<int>() { return 0; }
inline int ei_real(int x)  { return x; }
inline int ei_imag(int)    { return 0; }
inline int ei_conj(int x)  { return x; }
inline int ei_abs(int x)   { return abs(x); }
inline int ei_abs2(int x)  { return x*x; }
inline int ei_sqrt(int)  { ei_assert(false); return 0; }
inline int ei_exp(int)  { ei_assert(false); return 0; }
inline int ei_log(int)  { ei_assert(false); return 0; }
inline int ei_sin(int)  { ei_assert(false); return 0; }
inline int ei_cos(int)  { ei_assert(false); return 0; }
inline int ei_atan2(int, int)  { ei_assert(false); return 0; }
inline int ei_pow(int x, int y) { return int(std::pow(double(x), y)); }

template<> inline int ei_random(int a, int b)
{
  // We can't just do rand()%n as only the high-order bits are really random
  return a + static_cast<int>((b-a+1) * (rand() / (RAND_MAX + 1.0)));
}
template<> inline int ei_random()
{
  return ei_random<int>(-ei_random_amplitude<int>(), ei_random_amplitude<int>());
}
inline bool ei_isMuchSmallerThan(int a, int, int = precision<int>())
{
  return a == 0;
}
inline bool ei_isApprox(int a, int b, int = precision<int>())
{
  return a == b;
}
inline bool ei_isApproxOrLessThan(int a, int b, int = precision<int>())
{
  return a <= b;
}

/**************
*** float   ***
**************/

template<> inline float precision<float>() { return 1e-5f; }
template<> inline float machine_epsilon<float>() { return 1.192e-07f; }
inline float ei_real(float x)  { return x; }
inline float ei_imag(float)    { return 0.f; }
inline float ei_conj(float x)  { return x; }
inline float ei_abs(float x)   { return std::abs(x); }
inline float ei_abs2(float x)  { return x*x; }
inline float ei_sqrt(float x)  { return std::sqrt(x); }
inline float ei_exp(float x)   { return std::exp(x); }
inline float ei_log(float x)   { return std::log(x); }
inline float ei_sin(float x)   { return std::sin(x); }
inline float ei_cos(float x)   { return std::cos(x); }
inline float ei_atan2(float y, float x) { return std::atan2(y,x); }
inline float ei_pow(float x, float y)  { return std::pow(x, y); }

template<> inline float ei_random(float a, float b)
{
#ifdef EIGEN_NICE_RANDOM
  int i;
  do { i = ei_random<int>(256*int(a),256*int(b));
  } while(i==0);
  return float(i)/256.f;
#else
  return a + (b-a) * float(std::rand()) / float(RAND_MAX);
#endif
}
template<> inline float ei_random()
{
  return ei_random<float>(-ei_random_amplitude<float>(), ei_random_amplitude<float>());
}
inline bool ei_isMuchSmallerThan(float a, float b, float prec = precision<float>())
{
  return ei_abs(a) <= ei_abs(b) * prec;
}
inline bool ei_isApprox(float a, float b, float prec = precision<float>())
{
  return ei_abs(a - b) <= std::min(ei_abs(a), ei_abs(b)) * prec;
}
inline bool ei_isApproxOrLessThan(float a, float b, float prec = precision<float>())
{
  return a <= b || ei_isApprox(a, b, prec);
}

/**************
*** double  ***
**************/

template<> inline double precision<double>() { return 1e-11; }
template<> inline double machine_epsilon<double>() { return 2.220e-16; }

inline double ei_real(double x)  { return x; }
inline double ei_imag(double)    { return 0.; }
inline double ei_conj(double x)  { return x; }
inline double ei_abs(double x)   { return std::abs(x); }
inline double ei_abs2(double x)  { return x*x; }
inline double ei_sqrt(double x)  { return std::sqrt(x); }
inline double ei_exp(double x)   { return std::exp(x); }
inline double ei_log(double x)   { return std::log(x); }
inline double ei_sin(double x)   { return std::sin(x); }
inline double ei_cos(double x)   { return std::cos(x); }
inline double ei_atan2(double y, double x) { return std::atan2(y,x); }
inline double ei_pow(double x, double y) { return std::pow(x, y); }

template<> inline double ei_random(double a, double b)
{
#ifdef EIGEN_NICE_RANDOM
  int i;
  do { i= ei_random<int>(256*int(a),256*int(b));
  } while(i==0);
  return i/256.;
#else
  return a + (b-a) * std::rand() / RAND_MAX;
#endif
}
template<> inline double ei_random()
{
  return ei_random<double>(-ei_random_amplitude<double>(), ei_random_amplitude<double>());
}
inline bool ei_isMuchSmallerThan(double a, double b, double prec = precision<double>())
{
  return ei_abs(a) <= ei_abs(b) * prec;
}
inline bool ei_isApprox(double a, double b, double prec = precision<double>())
{
  return ei_abs(a - b) <= std::min(ei_abs(a), ei_abs(b)) * prec;
}
inline bool ei_isApproxOrLessThan(double a, double b, double prec = precision<double>())
{
  return a <= b || ei_isApprox(a, b, prec);
}

/*********************
*** complex<float> ***
*********************/

template<> inline float precision<std::complex<float> >() { return precision<float>(); }
template<> inline float machine_epsilon<std::complex<float> >() { return machine_epsilon<float>(); }
inline float ei_real(const std::complex<float>& x) { return std::real(x); }
inline float ei_imag(const std::complex<float>& x) { return std::imag(x); }
inline std::complex<float> ei_conj(const std::complex<float>& x) { return std::conj(x); }
inline float ei_abs(const std::complex<float>& x) { return std::abs(x); }
inline float ei_abs2(const std::complex<float>& x) { return std::norm(x); }
inline std::complex<float> ei_exp(std::complex<float> x)  { return std::exp(x); }
inline std::complex<float> ei_sin(std::complex<float> x)  { return std::sin(x); }
inline std::complex<float> ei_cos(std::complex<float> x)  { return std::cos(x); }
inline std::complex<float> ei_atan2(std::complex<float>, std::complex<float> )  { ei_assert(false); return 0; }

template<> inline std::complex<float> ei_random()
{
  return std::complex<float>(ei_random<float>(), ei_random<float>());
}
inline bool ei_isMuchSmallerThan(const std::complex<float>& a, const std::complex<float>& b, float prec = precision<float>())
{
  return ei_abs2(a) <= ei_abs2(b) * prec * prec;
}
inline bool ei_isMuchSmallerThan(const std::complex<float>& a, float b, float prec = precision<float>())
{
  return ei_abs2(a) <= ei_abs2(b) * prec * prec;
}
inline bool ei_isApprox(const std::complex<float>& a, const std::complex<float>& b, float prec = precision<float>())
{
  return ei_isApprox(ei_real(a), ei_real(b), prec)
      && ei_isApprox(ei_imag(a), ei_imag(b), prec);
}
// ei_isApproxOrLessThan wouldn't make sense for complex numbers

/**********************
*** complex<double> ***
**********************/

template<> inline double precision<std::complex<double> >() { return precision<double>(); }
template<> inline double machine_epsilon<std::complex<double> >() { return machine_epsilon<double>(); }
inline double ei_real(const std::complex<double>& x) { return std::real(x); }
inline double ei_imag(const std::complex<double>& x) { return std::imag(x); }
inline std::complex<double> ei_conj(const std::complex<double>& x) { return std::conj(x); }
inline double ei_abs(const std::complex<double>& x) { return std::abs(x); }
inline double ei_abs2(const std::complex<double>& x) { return std::norm(x); }
inline std::complex<double> ei_exp(std::complex<double> x)  { return std::exp(x); }
inline std::complex<double> ei_sin(std::complex<double> x)  { return std::sin(x); }
inline std::complex<double> ei_cos(std::complex<double> x)  { return std::cos(x); }
inline std::complex<double> ei_atan2(std::complex<double>, std::complex<double>)  { ei_assert(false); return 0; }

template<> inline std::complex<double> ei_random()
{
  return std::complex<double>(ei_random<double>(), ei_random<double>());
}
inline bool ei_isMuchSmallerThan(const std::complex<double>& a, const std::complex<double>& b, double prec = precision<double>())
{
  return ei_abs2(a) <= ei_abs2(b) * prec * prec;
}
inline bool ei_isMuchSmallerThan(const std::complex<double>& a, double b, double prec = precision<double>())
{
  return ei_abs2(a) <= ei_abs2(b) * prec * prec;
}
inline bool ei_isApprox(const std::complex<double>& a, const std::complex<double>& b, double prec = precision<double>())
{
  return ei_isApprox(ei_real(a), ei_real(b), prec)
      && ei_isApprox(ei_imag(a), ei_imag(b), prec);
}
// ei_isApproxOrLessThan wouldn't make sense for complex numbers


/******************
*** long double ***
******************/

template<> inline long double precision<long double>() { return precision<double>(); }
template<> inline long double machine_epsilon<long double>() { return 1.084e-19l; }
inline long double ei_real(long double x)  { return x; }
inline long double ei_imag(long double)    { return 0.; }
inline long double ei_conj(long double x)  { return x; }
inline long double ei_abs(long double x)   { return std::abs(x); }
inline long double ei_abs2(long double x)  { return x*x; }
inline long double ei_sqrt(long double x)  { return std::sqrt(x); }
inline long double ei_exp(long double x)   { return std::exp(x); }
inline long double ei_log(long double x)   { return std::log(x); }
inline long double ei_sin(long double x)   { return std::sin(x); }
inline long double ei_cos(long double x)   { return std::cos(x); }
inline long double ei_atan2(long double y, long double x) { return std::atan2(y,x); }
inline long double ei_pow(long double x, long double y)  { return std::pow(x, y); }

template<> inline long double ei_random(long double a, long double b)
{
  return ei_random<double>(static_cast<double>(a),static_cast<double>(b));
}
template<> inline long double ei_random()
{
  return ei_random<double>(-ei_random_amplitude<double>(), ei_random_amplitude<double>());
}
inline bool ei_isMuchSmallerThan(long double a, long double b, long double prec = precision<long double>())
{
  return ei_abs(a) <= ei_abs(b) * prec;
}
inline bool ei_isApprox(long double a, long double b, long double prec = precision<long double>())
{
  return ei_abs(a - b) <= std::min(ei_abs(a), ei_abs(b)) * prec;
}
inline bool ei_isApproxOrLessThan(long double a, long double b, long double prec = precision<long double>())
{
  return a <= b || ei_isApprox(a, b, prec);
}

#endif // EIGEN_MATHFUNCTIONS_H
