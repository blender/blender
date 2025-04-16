// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2023 Google Inc. All rights reserved.
// http://ceres-solver.org/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors may be
//   used to endorse or promote products derived from this software without
//   specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Author: keir@google.com (Keir Mierle)
//
// A simple implementation of N-dimensional dual numbers, for automatically
// computing exact derivatives of functions.
//
// While a complete treatment of the mechanics of automatic differentiation is
// beyond the scope of this header (see
// http://en.wikipedia.org/wiki/Automatic_differentiation for details), the
// basic idea is to extend normal arithmetic with an extra element, "e," often
// denoted with the greek symbol epsilon, such that e != 0 but e^2 = 0. Dual
// numbers are extensions of the real numbers analogous to complex numbers:
// whereas complex numbers augment the reals by introducing an imaginary unit i
// such that i^2 = -1, dual numbers introduce an "infinitesimal" unit e such
// that e^2 = 0. Dual numbers have two components: the "real" component and the
// "infinitesimal" component, generally written as x + y*e. Surprisingly, this
// leads to a convenient method for computing exact derivatives without needing
// to manipulate complicated symbolic expressions.
//
// For example, consider the function
//
//   f(x) = x^2 ,
//
// evaluated at 10. Using normal arithmetic, f(10) = 100, and df/dx(10) = 20.
// Next, argument 10 with an infinitesimal to get:
//
//   f(10 + e) = (10 + e)^2
//             = 100 + 2 * 10 * e + e^2
//             = 100 + 20 * e       -+-
//                     --            |
//                     |             +--- This is zero, since e^2 = 0
//                     |
//                     +----------------- This is df/dx!
//
// Note that the derivative of f with respect to x is simply the infinitesimal
// component of the value of f(x + e). So, in order to take the derivative of
// any function, it is only necessary to replace the numeric "object" used in
// the function with one extended with infinitesimals. The class Jet, defined in
// this header, is one such example of this, where substitution is done with
// templates.
//
// To handle derivatives of functions taking multiple arguments, different
// infinitesimals are used, one for each variable to take the derivative of. For
// example, consider a scalar function of two scalar parameters x and y:
//
//   f(x, y) = x^2 + x * y
//
// Following the technique above, to compute the derivatives df/dx and df/dy for
// f(1, 3) involves doing two evaluations of f, the first time replacing x with
// x + e, the second time replacing y with y + e.
//
// For df/dx:
//
//   f(1 + e, y) = (1 + e)^2 + (1 + e) * 3
//               = 1 + 2 * e + 3 + 3 * e
//               = 4 + 5 * e
//
//               --> df/dx = 5
//
// For df/dy:
//
//   f(1, 3 + e) = 1^2 + 1 * (3 + e)
//               = 1 + 3 + e
//               = 4 + e
//
//               --> df/dy = 1
//
// To take the gradient of f with the implementation of dual numbers ("jets") in
// this file, it is necessary to create a single jet type which has components
// for the derivative in x and y, and passing them to a templated version of f:
//
//   template<typename T>
//   T f(const T &x, const T &y) {
//     return x * x + x * y;
//   }
//
//   // The "2" means there should be 2 dual number components.
//   // It computes the partial derivative at x=10, y=20.
//   Jet<double, 2> x(10, 0);  // Pick the 0th dual number for x.
//   Jet<double, 2> y(20, 1);  // Pick the 1st dual number for y.
//   Jet<double, 2> z = f(x, y);
//
//   LOG(INFO) << "df/dx = " << z.v[0]
//             << "df/dy = " << z.v[1];
//
// Most users should not use Jet objects directly; a wrapper around Jet objects,
// which makes computing the derivative, gradient, or jacobian of templated
// functors simple, is in autodiff.h. Even autodiff.h should not be used
// directly; instead autodiff_cost_function.h is typically the file of interest.
//
// For the more mathematically inclined, this file implements first-order
// "jets". A 1st order jet is an element of the ring
//
//   T[N] = T[t_1, ..., t_N] / (t_1, ..., t_N)^2
//
// which essentially means that each jet consists of a "scalar" value 'a' from T
// and a 1st order perturbation vector 'v' of length N:
//
//   x = a + \sum_i v[i] t_i
//
// A shorthand is to write an element as x = a + u, where u is the perturbation.
// Then, the main point about the arithmetic of jets is that the product of
// perturbations is zero:
//
//   (a + u) * (b + v) = ab + av + bu + uv
//                     = ab + (av + bu) + 0
//
// which is what operator* implements below. Addition is simpler:
//
//   (a + u) + (b + v) = (a + b) + (u + v).
//
// The only remaining question is how to evaluate the function of a jet, for
// which we use the chain rule:
//
//   f(a + u) = f(a) + f'(a) u
//
// where f'(a) is the (scalar) derivative of f at a.
//
// By pushing these things through sufficiently and suitably templated
// functions, we can do automatic differentiation. Just be sure to turn on
// function inlining and common-subexpression elimination, or it will be very
// slow!
//
// WARNING: Most Ceres users should not directly include this file or know the
// details of how jets work. Instead the suggested method for automatic
// derivatives is to use autodiff_cost_function.h, which is a wrapper around
// both jets.h and autodiff.h to make taking derivatives of cost functions for
// use in Ceres easier.

#ifndef CERES_PUBLIC_JET_H_
#define CERES_PUBLIC_JET_H_

#include <cmath>
#include <complex>
#include <iosfwd>
#include <iostream>  // NOLINT
#include <limits>
#include <numeric>
#include <string>
#include <type_traits>

#include "Eigen/Core"
#include "ceres/internal/jet_traits.h"
#include "ceres/internal/port.h"
#include "ceres/jet_fwd.h"

// Here we provide partial specializations of std::common_type for the Jet class
// to allow determining a Jet type with a common underlying arithmetic type.
// Such an arithmetic type can be either a scalar or an another Jet. An example
// for a common type, say, between a float and a Jet<double, N> is a Jet<double,
// N> (i.e., std::common_type_t<float, ceres::Jet<double, N>> and
// ceres::Jet<double, N> refer to the same type.)
//
// The partial specialization are also used for determining compatible types by
// means of SFINAE and thus allow such types to be expressed as operands of
// logical comparison operators. Missing (partial) specialization of
// std::common_type for a particular (custom) type will therefore disable the
// use of comparison operators defined by Ceres.
//
// Since these partial specializations are used as SFINAE constraints, they
// enable standard promotion rules between various scalar types and consequently
// their use in comparison against a Jet without providing implicit
// conversions from a scalar, such as an int, to a Jet (see the implementation
// of logical comparison operators below).

template <typename T, int N, typename U>
struct std::common_type<T, ceres::Jet<U, N>> {
  using type = ceres::Jet<common_type_t<T, U>, N>;
};

template <typename T, int N, typename U>
struct std::common_type<ceres::Jet<T, N>, U> {
  using type = ceres::Jet<common_type_t<T, U>, N>;
};

template <typename T, int N, typename U>
struct std::common_type<ceres::Jet<T, N>, ceres::Jet<U, N>> {
  using type = ceres::Jet<common_type_t<T, U>, N>;
};

namespace ceres {

template <typename T, int N>
struct Jet {
  enum { DIMENSION = N };
  using Scalar = T;

  // Default-construct "a" because otherwise this can lead to false errors about
  // uninitialized uses when other classes relying on default constructed T
  // (where T is a Jet<T, N>). This usually only happens in opt mode. Note that
  // the C++ standard mandates that e.g. default constructed doubles are
  // initialized to 0.0; see sections 8.5 of the C++03 standard.
  Jet() : a() { v.setConstant(Scalar()); }

  // Constructor from scalar: a + 0.
  explicit Jet(const T& value) {
    a = value;
    v.setConstant(Scalar());
  }

  // Constructor from scalar plus variable: a + t_i.
  Jet(const T& value, int k) {
    a = value;
    v.setConstant(Scalar());
    v[k] = T(1.0);
  }

  // Constructor from scalar and vector part
  // The use of Eigen::DenseBase allows Eigen expressions
  // to be passed in without being fully evaluated until
  // they are assigned to v
  template <typename Derived>
  EIGEN_STRONG_INLINE Jet(const T& a, const Eigen::DenseBase<Derived>& v)
      : a(a), v(v) {}

  // Compound operators
  Jet<T, N>& operator+=(const Jet<T, N>& y) {
    *this = *this + y;
    return *this;
  }

  Jet<T, N>& operator-=(const Jet<T, N>& y) {
    *this = *this - y;
    return *this;
  }

  Jet<T, N>& operator*=(const Jet<T, N>& y) {
    *this = *this * y;
    return *this;
  }

  Jet<T, N>& operator/=(const Jet<T, N>& y) {
    *this = *this / y;
    return *this;
  }

  // Compound with scalar operators.
  Jet<T, N>& operator+=(const T& s) {
    *this = *this + s;
    return *this;
  }

  Jet<T, N>& operator-=(const T& s) {
    *this = *this - s;
    return *this;
  }

  Jet<T, N>& operator*=(const T& s) {
    *this = *this * s;
    return *this;
  }

  Jet<T, N>& operator/=(const T& s) {
    *this = *this / s;
    return *this;
  }

  // The scalar part.
  T a;

  // The infinitesimal part.
  Eigen::Matrix<T, N, 1> v;

  // This struct needs to have an Eigen aligned operator new as it contains
  // fixed-size Eigen types.
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

// Unary +
template <typename T, int N>
inline Jet<T, N> const& operator+(const Jet<T, N>& f) {
  return f;
}

// TODO(keir): Try adding __attribute__((always_inline)) to these functions to
// see if it causes a performance increase.

// Unary -
template <typename T, int N>
inline Jet<T, N> operator-(const Jet<T, N>& f) {
  return Jet<T, N>(-f.a, -f.v);
}

// Binary +
template <typename T, int N>
inline Jet<T, N> operator+(const Jet<T, N>& f, const Jet<T, N>& g) {
  return Jet<T, N>(f.a + g.a, f.v + g.v);
}

// Binary + with a scalar: x + s
template <typename T, int N>
inline Jet<T, N> operator+(const Jet<T, N>& f, T s) {
  return Jet<T, N>(f.a + s, f.v);
}

// Binary + with a scalar: s + x
template <typename T, int N>
inline Jet<T, N> operator+(T s, const Jet<T, N>& f) {
  return Jet<T, N>(f.a + s, f.v);
}

// Binary -
template <typename T, int N>
inline Jet<T, N> operator-(const Jet<T, N>& f, const Jet<T, N>& g) {
  return Jet<T, N>(f.a - g.a, f.v - g.v);
}

// Binary - with a scalar: x - s
template <typename T, int N>
inline Jet<T, N> operator-(const Jet<T, N>& f, T s) {
  return Jet<T, N>(f.a - s, f.v);
}

// Binary - with a scalar: s - x
template <typename T, int N>
inline Jet<T, N> operator-(T s, const Jet<T, N>& f) {
  return Jet<T, N>(s - f.a, -f.v);
}

// Binary *
template <typename T, int N>
inline Jet<T, N> operator*(const Jet<T, N>& f, const Jet<T, N>& g) {
  return Jet<T, N>(f.a * g.a, f.a * g.v + f.v * g.a);
}

// Binary * with a scalar: x * s
template <typename T, int N>
inline Jet<T, N> operator*(const Jet<T, N>& f, T s) {
  return Jet<T, N>(f.a * s, f.v * s);
}

// Binary * with a scalar: s * x
template <typename T, int N>
inline Jet<T, N> operator*(T s, const Jet<T, N>& f) {
  return Jet<T, N>(f.a * s, f.v * s);
}

// Binary /
template <typename T, int N>
inline Jet<T, N> operator/(const Jet<T, N>& f, const Jet<T, N>& g) {
  // This uses:
  //
  //   a + u   (a + u)(b - v)   (a + u)(b - v)
  //   ----- = -------------- = --------------
  //   b + v   (b + v)(b - v)        b^2
  //
  // which holds because v*v = 0.
  const T g_a_inverse = T(1.0) / g.a;
  const T f_a_by_g_a = f.a * g_a_inverse;
  return Jet<T, N>(f_a_by_g_a, (f.v - f_a_by_g_a * g.v) * g_a_inverse);
}

// Binary / with a scalar: s / x
template <typename T, int N>
inline Jet<T, N> operator/(T s, const Jet<T, N>& g) {
  const T minus_s_g_a_inverse2 = -s / (g.a * g.a);
  return Jet<T, N>(s / g.a, g.v * minus_s_g_a_inverse2);
}

// Binary / with a scalar: x / s
template <typename T, int N>
inline Jet<T, N> operator/(const Jet<T, N>& f, T s) {
  const T s_inverse = T(1.0) / s;
  return Jet<T, N>(f.a * s_inverse, f.v * s_inverse);
}

// Binary comparison operators for both scalars and jets. At least one of the
// operands must be a Jet. Promotable scalars (e.g., int, float, double etc.)
// can appear on either side of the operator. std::common_type_t is used as an
// SFINAE constraint to selectively enable compatible operand types. This allows
// comparison, for instance, against int literals without implicit conversion.
// In case the Jet arithmetic type is a Jet itself, a recursive expansion of Jet
// value is performed.
#define CERES_DEFINE_JET_COMPARISON_OPERATOR(op)                            \
  template <typename Lhs,                                                   \
            typename Rhs,                                                   \
            std::enable_if_t<PromotableJetOperands_v<Lhs, Rhs>>* = nullptr> \
  constexpr bool operator op(const Lhs& f, const Rhs& g) noexcept(          \
      noexcept(internal::AsScalar(f) op internal::AsScalar(g))) {           \
    using internal::AsScalar;                                               \
    return AsScalar(f) op AsScalar(g);                                      \
  }
CERES_DEFINE_JET_COMPARISON_OPERATOR(<)   // NOLINT
CERES_DEFINE_JET_COMPARISON_OPERATOR(<=)  // NOLINT
CERES_DEFINE_JET_COMPARISON_OPERATOR(>)   // NOLINT
CERES_DEFINE_JET_COMPARISON_OPERATOR(>=)  // NOLINT
CERES_DEFINE_JET_COMPARISON_OPERATOR(==)  // NOLINT
CERES_DEFINE_JET_COMPARISON_OPERATOR(!=)  // NOLINT
#undef CERES_DEFINE_JET_COMPARISON_OPERATOR

// Pull some functions from namespace std.
//
// This is necessary because we want to use the same name (e.g. 'sqrt') for
// double-valued and Jet-valued functions, but we are not allowed to put
// Jet-valued functions inside namespace std.
using std::abs;
using std::acos;
using std::asin;
using std::atan;
using std::atan2;
using std::cbrt;
using std::ceil;
using std::copysign;
using std::cos;
using std::cosh;
using std::erf;
using std::erfc;
using std::exp;
using std::exp2;
using std::expm1;
using std::fdim;
using std::floor;
using std::fma;
using std::fmax;
using std::fmin;
using std::fpclassify;
using std::hypot;
using std::isfinite;
using std::isinf;
using std::isnan;
using std::isnormal;
using std::log;
using std::log10;
using std::log1p;
using std::log2;
using std::norm;
using std::pow;
using std::signbit;
using std::sin;
using std::sinh;
using std::sqrt;
using std::tan;
using std::tanh;

// MSVC (up to 1930) defines quiet comparison functions as template functions
// which causes compilation errors due to ambiguity in the template parameter
// type resolution for using declarations in the ceres namespace. Workaround the
// issue by defining specific overload and bypass MSVC standard library
// definitions.
#if defined(_MSC_VER)
inline bool isgreater(double lhs,
                      double rhs) noexcept(noexcept(std::isgreater(lhs, rhs))) {
  return std::isgreater(lhs, rhs);
}
inline bool isless(double lhs,
                   double rhs) noexcept(noexcept(std::isless(lhs, rhs))) {
  return std::isless(lhs, rhs);
}
inline bool islessequal(double lhs,
                        double rhs) noexcept(noexcept(std::islessequal(lhs,
                                                                       rhs))) {
  return std::islessequal(lhs, rhs);
}
inline bool isgreaterequal(double lhs, double rhs) noexcept(
    noexcept(std::isgreaterequal(lhs, rhs))) {
  return std::isgreaterequal(lhs, rhs);
}
inline bool islessgreater(double lhs, double rhs) noexcept(
    noexcept(std::islessgreater(lhs, rhs))) {
  return std::islessgreater(lhs, rhs);
}
inline bool isunordered(double lhs,
                        double rhs) noexcept(noexcept(std::isunordered(lhs,
                                                                       rhs))) {
  return std::isunordered(lhs, rhs);
}
#else
using std::isgreater;
using std::isgreaterequal;
using std::isless;
using std::islessequal;
using std::islessgreater;
using std::isunordered;
#endif

#ifdef CERES_HAS_CPP20
using std::lerp;
using std::midpoint;
#endif  // defined(CERES_HAS_CPP20)

// Legacy names from pre-C++11 days.
// clang-format off
CERES_DEPRECATED_WITH_MSG("ceres::IsFinite will be removed in a future Ceres Solver release. Please use ceres::isfinite.")
inline bool IsFinite(double x)   { return std::isfinite(x); }
CERES_DEPRECATED_WITH_MSG("ceres::IsInfinite will be removed in a future Ceres Solver release. Please use ceres::isinf.")
inline bool IsInfinite(double x) { return std::isinf(x);    }
CERES_DEPRECATED_WITH_MSG("ceres::IsNaN will be removed in a future Ceres Solver release. Please use ceres::isnan.")
inline bool IsNaN(double x)      { return std::isnan(x);    }
CERES_DEPRECATED_WITH_MSG("ceres::IsNormal will be removed in a future Ceres Solver release. Please use ceres::isnormal.")
inline bool IsNormal(double x)   { return std::isnormal(x); }
// clang-format on

// In general, f(a + h) ~= f(a) + f'(a) h, via the chain rule.

// abs(x + h) ~= abs(x) + sgn(x)h
template <typename T, int N>
inline Jet<T, N> abs(const Jet<T, N>& f) {
  return Jet<T, N>(abs(f.a), copysign(T(1), f.a) * f.v);
}

// copysign(a, b) composes a float with the magnitude of a and the sign of b.
// Therefore, the function can be formally defined as
//
//   copysign(a, b) = sgn(b)|a|
//
// where
//
//   d/dx |x| = sgn(x)
//   d/dx sgn(x) = 2δ(x)
//
// sgn(x) being the signum function. Differentiating copysign(a, b) with respect
// to a and b gives:
//
//   d/da sgn(b)|a| = sgn(a) sgn(b)
//   d/db sgn(b)|a| = 2|a|δ(b)
//
// with the dual representation given by
//
//   copysign(a + da, b + db) ~= sgn(b)|a| + (sgn(a)sgn(b) da + 2|a|δ(b) db)
//
// where δ(b) is the Dirac delta function.
template <typename T, int N>
inline Jet<T, N> copysign(const Jet<T, N>& f, const Jet<T, N> g) {
  // The Dirac delta function  δ(b) is undefined at b=0 (here it's
  // infinite) and 0 everywhere else.
  T d = fpclassify(g) == FP_ZERO ? std::numeric_limits<T>::infinity() : T(0);
  T sa = copysign(T(1), f.a);  // sgn(a)
  T sb = copysign(T(1), g.a);  // sgn(b)
  // The second part of the infinitesimal is 2|a|δ(b) which is either infinity
  // or 0 unless a or any of the values of the b infinitesimal are 0. In the
  // latter case, the corresponding values become NaNs (multiplying 0 by
  // infinity gives NaN). We drop the constant factor 2 since it does not change
  // the result (its values will still be either 0, infinity or NaN).
  return Jet<T, N>(copysign(f.a, g.a), sa * sb * f.v + abs(f.a) * d * g.v);
}

// log(a + h) ~= log(a) + h / a
template <typename T, int N>
inline Jet<T, N> log(const Jet<T, N>& f) {
  const T a_inverse = T(1.0) / f.a;
  return Jet<T, N>(log(f.a), f.v * a_inverse);
}

// log10(a + h) ~= log10(a) + h / (a log(10))
template <typename T, int N>
inline Jet<T, N> log10(const Jet<T, N>& f) {
  // Most compilers will expand log(10) to a constant.
  const T a_inverse = T(1.0) / (f.a * log(T(10.0)));
  return Jet<T, N>(log10(f.a), f.v * a_inverse);
}

// log1p(a + h) ~= log1p(a) + h / (1 + a)
template <typename T, int N>
inline Jet<T, N> log1p(const Jet<T, N>& f) {
  const T a_inverse = T(1.0) / (T(1.0) + f.a);
  return Jet<T, N>(log1p(f.a), f.v * a_inverse);
}

// exp(a + h) ~= exp(a) + exp(a) h
template <typename T, int N>
inline Jet<T, N> exp(const Jet<T, N>& f) {
  const T tmp = exp(f.a);
  return Jet<T, N>(tmp, tmp * f.v);
}

// expm1(a + h) ~= expm1(a) + exp(a) h
template <typename T, int N>
inline Jet<T, N> expm1(const Jet<T, N>& f) {
  const T tmp = expm1(f.a);
  const T expa = tmp + T(1.0);  // exp(a) = expm1(a) + 1
  return Jet<T, N>(tmp, expa * f.v);
}

// sqrt(a + h) ~= sqrt(a) + h / (2 sqrt(a))
template <typename T, int N>
inline Jet<T, N> sqrt(const Jet<T, N>& f) {
  const T tmp = sqrt(f.a);
  const T two_a_inverse = T(1.0) / (T(2.0) * tmp);
  return Jet<T, N>(tmp, f.v * two_a_inverse);
}

// cos(a + h) ~= cos(a) - sin(a) h
template <typename T, int N>
inline Jet<T, N> cos(const Jet<T, N>& f) {
  return Jet<T, N>(cos(f.a), -sin(f.a) * f.v);
}

// acos(a + h) ~= acos(a) - 1 / sqrt(1 - a^2) h
template <typename T, int N>
inline Jet<T, N> acos(const Jet<T, N>& f) {
  const T tmp = -T(1.0) / sqrt(T(1.0) - f.a * f.a);
  return Jet<T, N>(acos(f.a), tmp * f.v);
}

// sin(a + h) ~= sin(a) + cos(a) h
template <typename T, int N>
inline Jet<T, N> sin(const Jet<T, N>& f) {
  return Jet<T, N>(sin(f.a), cos(f.a) * f.v);
}

// asin(a + h) ~= asin(a) + 1 / sqrt(1 - a^2) h
template <typename T, int N>
inline Jet<T, N> asin(const Jet<T, N>& f) {
  const T tmp = T(1.0) / sqrt(T(1.0) - f.a * f.a);
  return Jet<T, N>(asin(f.a), tmp * f.v);
}

// tan(a + h) ~= tan(a) + (1 + tan(a)^2) h
template <typename T, int N>
inline Jet<T, N> tan(const Jet<T, N>& f) {
  const T tan_a = tan(f.a);
  const T tmp = T(1.0) + tan_a * tan_a;
  return Jet<T, N>(tan_a, tmp * f.v);
}

// atan(a + h) ~= atan(a) + 1 / (1 + a^2) h
template <typename T, int N>
inline Jet<T, N> atan(const Jet<T, N>& f) {
  const T tmp = T(1.0) / (T(1.0) + f.a * f.a);
  return Jet<T, N>(atan(f.a), tmp * f.v);
}

// sinh(a + h) ~= sinh(a) + cosh(a) h
template <typename T, int N>
inline Jet<T, N> sinh(const Jet<T, N>& f) {
  return Jet<T, N>(sinh(f.a), cosh(f.a) * f.v);
}

// cosh(a + h) ~= cosh(a) + sinh(a) h
template <typename T, int N>
inline Jet<T, N> cosh(const Jet<T, N>& f) {
  return Jet<T, N>(cosh(f.a), sinh(f.a) * f.v);
}

// tanh(a + h) ~= tanh(a) + (1 - tanh(a)^2) h
template <typename T, int N>
inline Jet<T, N> tanh(const Jet<T, N>& f) {
  const T tanh_a = tanh(f.a);
  const T tmp = T(1.0) - tanh_a * tanh_a;
  return Jet<T, N>(tanh_a, tmp * f.v);
}

// The floor function should be used with extreme care as this operation will
// result in a zero derivative which provides no information to the solver.
//
// floor(a + h) ~= floor(a) + 0
template <typename T, int N>
inline Jet<T, N> floor(const Jet<T, N>& f) {
  return Jet<T, N>(floor(f.a));
}

// The ceil function should be used with extreme care as this operation will
// result in a zero derivative which provides no information to the solver.
//
// ceil(a + h) ~= ceil(a) + 0
template <typename T, int N>
inline Jet<T, N> ceil(const Jet<T, N>& f) {
  return Jet<T, N>(ceil(f.a));
}

// Some new additions to C++11:

// cbrt(a + h) ~= cbrt(a) + h / (3 a ^ (2/3))
template <typename T, int N>
inline Jet<T, N> cbrt(const Jet<T, N>& f) {
  const T derivative = T(1.0) / (T(3.0) * cbrt(f.a * f.a));
  return Jet<T, N>(cbrt(f.a), f.v * derivative);
}

// exp2(x + h) = 2^(x+h) ~= 2^x + h*2^x*log(2)
template <typename T, int N>
inline Jet<T, N> exp2(const Jet<T, N>& f) {
  const T tmp = exp2(f.a);
  const T derivative = tmp * log(T(2));
  return Jet<T, N>(tmp, f.v * derivative);
}

// log2(x + h) ~= log2(x) + h / (x * log(2))
template <typename T, int N>
inline Jet<T, N> log2(const Jet<T, N>& f) {
  const T derivative = T(1.0) / (f.a * log(T(2)));
  return Jet<T, N>(log2(f.a), f.v * derivative);
}

// Like sqrt(x^2 + y^2),
// but acts to prevent underflow/overflow for small/large x/y.
// Note that the function is non-smooth at x=y=0,
// so the derivative is undefined there.
template <typename T, int N>
inline Jet<T, N> hypot(const Jet<T, N>& x, const Jet<T, N>& y) {
  // d/da sqrt(a) = 0.5 / sqrt(a)
  // d/dx x^2 + y^2 = 2x
  // So by the chain rule:
  // d/dx sqrt(x^2 + y^2) = 0.5 / sqrt(x^2 + y^2) * 2x = x / sqrt(x^2 + y^2)
  // d/dy sqrt(x^2 + y^2) = y / sqrt(x^2 + y^2)
  const T tmp = hypot(x.a, y.a);
  return Jet<T, N>(tmp, x.a / tmp * x.v + y.a / tmp * y.v);
}

// Like sqrt(x^2 + y^2 + z^2),
// but acts to prevent underflow/overflow for small/large x/y/z.
// Note that the function is non-smooth at x=y=z=0,
// so the derivative is undefined there.
template <typename T, int N>
inline Jet<T, N> hypot(const Jet<T, N>& x,
                       const Jet<T, N>& y,
                       const Jet<T, N>& z) {
  // d/da sqrt(a) = 0.5 / sqrt(a)
  // d/dx x^2 + y^2 + z^2 = 2x
  // So by the chain rule:
  // d/dx sqrt(x^2 + y^2 + z^2)
  //    = 0.5 / sqrt(x^2 + y^2 + z^2) * 2x
  //    = x / sqrt(x^2 + y^2 + z^2)
  // d/dy sqrt(x^2 + y^2 + z^2) = y / sqrt(x^2 + y^2 + z^2)
  // d/dz sqrt(x^2 + y^2 + z^2) = z / sqrt(x^2 + y^2 + z^2)
  const T tmp = hypot(x.a, y.a, z.a);
  return Jet<T, N>(tmp, x.a / tmp * x.v + y.a / tmp * y.v + z.a / tmp * z.v);
}

// Like x * y + z but rounded only once.
template <typename T, int N>
inline Jet<T, N> fma(const Jet<T, N>& x,
                     const Jet<T, N>& y,
                     const Jet<T, N>& z) {
  // d/dx fma(x, y, z) = y
  // d/dy fma(x, y, z) = x
  // d/dz fma(x, y, z) = 1
  return Jet<T, N>(fma(x.a, y.a, z.a), y.a * x.v + x.a * y.v + z.v);
}

// Return value of fmax() and fmin() on equality
// ---------------------------------------------
//
// There is arguably no good answer to what fmax() & fmin() should return on
// equality, which for Jets by definition ONLY compares the scalar parts. We
// choose what we think is the least worst option (averaging as Jets) which
// minimises undesirable/unexpected behaviour as used, and also supports client
// code written against Ceres versions prior to type promotion being supported
// in Jet comparisons (< v2.1).
//
// The std::max() convention of returning the first argument on equality is
// problematic, as it means that the derivative component may or may not be
// preserved (when comparing a Jet with a scalar) depending upon the ordering.
//
// Always returning the Jet in {Jet, scalar} cases on equality is problematic
// as it is inconsistent with the behaviour that would be obtained if the scalar
// was first cast to Jet and the {Jet, Jet} case was used. Prior to type
// promotion (Ceres v2.1) client code would typically cast constants to Jets
// e.g: fmax(x, T(2.0)) which means the {Jet, Jet} case predominates, and we
// still want the result to be order independent.
//
// Our intuition is that preserving a non-zero derivative is best, even if
// its value does not match either of the inputs. Averaging achieves this
// whilst ensuring argument ordering independence. This is also the approach
// used by the Jax library, and TensorFlow's reduce_max().

// Returns the larger of the two arguments, with Jet averaging on equality.
// NaNs are treated as missing data.
//
// NOTE: This function is NOT subject to any of the error conditions specified
//       in `math_errhandling`.
template <typename Lhs,
          typename Rhs,
          std::enable_if_t<CompatibleJetOperands_v<Lhs, Rhs>>* = nullptr>
inline decltype(auto) fmax(const Lhs& x, const Rhs& y) {
  using J = std::common_type_t<Lhs, Rhs>;
  // As x == y may set FP exceptions in the presence of NaNs when used with
  // non-default compiler options so we avoid its use here.
  if (isnan(x) || isnan(y) || islessgreater(x, y)) {
    return isnan(x) || isless(x, y) ? J{y} : J{x};
  }
  // x == y (scalar parts) return the average of their Jet representations.
#if defined(CERES_HAS_CPP20)
  return midpoint(J{x}, J{y});
#else
  return (J{x} + J{y}) * typename J::Scalar(0.5);
#endif  // defined(CERES_HAS_CPP20)
}

// Returns the smaller of the two arguments, with Jet averaging on equality.
// NaNs are treated as missing data.
//
// NOTE: This function is NOT subject to any of the error conditions specified
//       in `math_errhandling`.
template <typename Lhs,
          typename Rhs,
          std::enable_if_t<CompatibleJetOperands_v<Lhs, Rhs>>* = nullptr>
inline decltype(auto) fmin(const Lhs& x, const Rhs& y) {
  using J = std::common_type_t<Lhs, Rhs>;
  // As x == y may set FP exceptions in the presence of NaNs when used with
  // non-default compiler options so we avoid its use here.
  if (isnan(x) || isnan(y) || islessgreater(x, y)) {
    return isnan(x) || isgreater(x, y) ? J{y} : J{x};
  }
  // x == y (scalar parts) return the average of their Jet representations.
#if defined(CERES_HAS_CPP20)
  return midpoint(J{x}, J{y});
#else
  return (J{x} + J{y}) * typename J::Scalar(0.5);
#endif  // defined(CERES_HAS_CPP20)
}

// Returns the positive difference (f - g) of two arguments and zero if f <= g.
// If at least one argument is NaN, a NaN is return.
//
// NOTE At least one of the argument types must be a Jet, the other one can be a
// scalar. In case both arguments are Jets, their dimensionality must match.
template <typename Lhs,
          typename Rhs,
          std::enable_if_t<CompatibleJetOperands_v<Lhs, Rhs>>* = nullptr>
inline decltype(auto) fdim(const Lhs& f, const Rhs& g) {
  using J = std::common_type_t<Lhs, Rhs>;
  if (isnan(f) || isnan(g)) {
    return std::numeric_limits<J>::quiet_NaN();
  }
  return isgreater(f, g) ? J{f - g} : J{};
}

// erf is defined as an integral that cannot be expressed analytically
// however, the derivative is trivial to compute
// erf(x + h) = erf(x) + h * 2*exp(-x^2)/sqrt(pi)
template <typename T, int N>
inline Jet<T, N> erf(const Jet<T, N>& x) {
  // We evaluate the constant as follows:
  //   2 / sqrt(pi) = 1 / sqrt(atan(1.))
  // On POSIX systems it is defined as M_2_SQRTPI, but this is not
  // portable and the type may not be T.  The above expression
  // evaluates to full precision with IEEE arithmetic and, since it's
  // constant, the compiler can generate exactly the same code.  gcc
  // does so even at -O0.
  return Jet<T, N>(erf(x.a), x.v * exp(-x.a * x.a) * (T(1) / sqrt(atan(T(1)))));
}

// erfc(x) = 1-erf(x)
// erfc(x + h) = erfc(x) + h * (-2*exp(-x^2)/sqrt(pi))
template <typename T, int N>
inline Jet<T, N> erfc(const Jet<T, N>& x) {
  // See in erf() above for the evaluation of the constant in the derivative.
  return Jet<T, N>(erfc(x.a),
                   -x.v * exp(-x.a * x.a) * (T(1) / sqrt(atan(T(1)))));
}

// Bessel functions of the first kind with integer order equal to 0, 1, n.
//
// Microsoft has deprecated the j[0,1,n]() POSIX Bessel functions in favour of
// _j[0,1,n]().  Where available on MSVC, use _j[0,1,n]() to avoid deprecated
// function errors in client code (the specific warning is suppressed when
// Ceres itself is built).
inline double BesselJ0(double x) {
  CERES_DISABLE_DEPRECATED_WARNING
  return j0(x);
  CERES_RESTORE_DEPRECATED_WARNING
}
inline double BesselJ1(double x) {
  CERES_DISABLE_DEPRECATED_WARNING
  return j1(x);
  CERES_RESTORE_DEPRECATED_WARNING
}
inline double BesselJn(int n, double x) {
  CERES_DISABLE_DEPRECATED_WARNING
  return jn(n, x);
  CERES_RESTORE_DEPRECATED_WARNING
}

// For the formulae of the derivatives of the Bessel functions see the book:
// Olver, Lozier, Boisvert, Clark, NIST Handbook of Mathematical Functions,
// Cambridge University Press 2010.
//
// Formulae are also available at http://dlmf.nist.gov

// See formula http://dlmf.nist.gov/10.6#E3
// j0(a + h) ~= j0(a) - j1(a) h
template <typename T, int N>
inline Jet<T, N> BesselJ0(const Jet<T, N>& f) {
  return Jet<T, N>(BesselJ0(f.a), -BesselJ1(f.a) * f.v);
}

// See formula http://dlmf.nist.gov/10.6#E1
// j1(a + h) ~= j1(a) + 0.5 ( j0(a) - j2(a) ) h
template <typename T, int N>
inline Jet<T, N> BesselJ1(const Jet<T, N>& f) {
  return Jet<T, N>(BesselJ1(f.a),
                   T(0.5) * (BesselJ0(f.a) - BesselJn(2, f.a)) * f.v);
}

// See formula http://dlmf.nist.gov/10.6#E1
// j_n(a + h) ~= j_n(a) + 0.5 ( j_{n-1}(a) - j_{n+1}(a) ) h
template <typename T, int N>
inline Jet<T, N> BesselJn(int n, const Jet<T, N>& f) {
  return Jet<T, N>(
      BesselJn(n, f.a),
      T(0.5) * (BesselJn(n - 1, f.a) - BesselJn(n + 1, f.a)) * f.v);
}

// Classification and comparison functionality referencing only the scalar part
// of a Jet. To classify the derivatives (e.g., for sanity checks), the dual
// part should be referenced explicitly. For instance, to check whether the
// derivatives of a Jet 'f' are reasonable, one can use
//
//  isfinite(f.v.array()).all()
//  !isnan(f.v.array()).any()
//
// etc., depending on the desired semantics.
//
// NOTE: Floating-point classification and comparison functions and operators
// should be used with care as no derivatives can be propagated by such
// functions directly but only by expressions resulting from corresponding
// conditional statements. At the same time, conditional statements can possibly
// introduce a discontinuity in the cost function making it impossible to
// evaluate its derivative and thus the optimization problem intractable.

// Determines whether the scalar part of the Jet is finite.
template <typename T, int N>
inline bool isfinite(const Jet<T, N>& f) {
  return isfinite(f.a);
}

// Determines whether the scalar part of the Jet is infinite.
template <typename T, int N>
inline bool isinf(const Jet<T, N>& f) {
  return isinf(f.a);
}

// Determines whether the scalar part of the Jet is NaN.
template <typename T, int N>
inline bool isnan(const Jet<T, N>& f) {
  return isnan(f.a);
}

// Determines whether the scalar part of the Jet is neither zero, subnormal,
// infinite, nor NaN.
template <typename T, int N>
inline bool isnormal(const Jet<T, N>& f) {
  return isnormal(f.a);
}

// Determines whether the scalar part of the Jet f is less than the scalar
// part of g.
//
// NOTE: This function does NOT set any floating-point exceptions.
template <typename Lhs,
          typename Rhs,
          std::enable_if_t<CompatibleJetOperands_v<Lhs, Rhs>>* = nullptr>
inline bool isless(const Lhs& f, const Rhs& g) {
  using internal::AsScalar;
  return isless(AsScalar(f), AsScalar(g));
}

// Determines whether the scalar part of the Jet f is greater than the scalar
// part of g.
//
// NOTE: This function does NOT set any floating-point exceptions.
template <typename Lhs,
          typename Rhs,
          std::enable_if_t<CompatibleJetOperands_v<Lhs, Rhs>>* = nullptr>
inline bool isgreater(const Lhs& f, const Rhs& g) {
  using internal::AsScalar;
  return isgreater(AsScalar(f), AsScalar(g));
}

// Determines whether the scalar part of the Jet f is less than or equal to the
// scalar part of g.
//
// NOTE: This function does NOT set any floating-point exceptions.
template <typename Lhs,
          typename Rhs,
          std::enable_if_t<CompatibleJetOperands_v<Lhs, Rhs>>* = nullptr>
inline bool islessequal(const Lhs& f, const Rhs& g) {
  using internal::AsScalar;
  return islessequal(AsScalar(f), AsScalar(g));
}

// Determines whether the scalar part of the Jet f is less than or greater than
// (f < g || f > g) the scalar part of g.
//
// NOTE: This function does NOT set any floating-point exceptions.
template <typename Lhs,
          typename Rhs,
          std::enable_if_t<CompatibleJetOperands_v<Lhs, Rhs>>* = nullptr>
inline bool islessgreater(const Lhs& f, const Rhs& g) {
  using internal::AsScalar;
  return islessgreater(AsScalar(f), AsScalar(g));
}

// Determines whether the scalar part of the Jet f is greater than or equal to
// the scalar part of g.
//
// NOTE: This function does NOT set any floating-point exceptions.
template <typename Lhs,
          typename Rhs,
          std::enable_if_t<CompatibleJetOperands_v<Lhs, Rhs>>* = nullptr>
inline bool isgreaterequal(const Lhs& f, const Rhs& g) {
  using internal::AsScalar;
  return isgreaterequal(AsScalar(f), AsScalar(g));
}

// Determines if either of the scalar parts of the arguments are NaN and
// thus cannot be ordered with respect to each other.
template <typename Lhs,
          typename Rhs,
          std::enable_if_t<CompatibleJetOperands_v<Lhs, Rhs>>* = nullptr>
inline bool isunordered(const Lhs& f, const Rhs& g) {
  using internal::AsScalar;
  return isunordered(AsScalar(f), AsScalar(g));
}

// Categorize scalar part as zero, subnormal, normal, infinite, NaN, or
// implementation-defined.
template <typename T, int N>
inline int fpclassify(const Jet<T, N>& f) {
  return fpclassify(f.a);
}

// Determines whether the scalar part of the argument is negative.
template <typename T, int N>
inline bool signbit(const Jet<T, N>& f) {
  return signbit(f.a);
}

// Legacy functions from the pre-C++11 days.
template <typename T, int N>
CERES_DEPRECATED_WITH_MSG(
    "ceres::IsFinite will be removed in a future Ceres Solver release. Please "
    "use ceres::isfinite.")
inline bool IsFinite(const Jet<T, N>& f) {
  return isfinite(f);
}

template <typename T, int N>
CERES_DEPRECATED_WITH_MSG(
    "ceres::IsNaN will be removed in a future Ceres Solver release. Please use "
    "ceres::isnan.")
inline bool IsNaN(const Jet<T, N>& f) {
  return isnan(f);
}

template <typename T, int N>
CERES_DEPRECATED_WITH_MSG(
    "ceres::IsNormal will be removed in a future Ceres Solver release. Please "
    "use ceres::isnormal.")
inline bool IsNormal(const Jet<T, N>& f) {
  return isnormal(f);
}

// The jet is infinite if any part of the jet is infinite.
template <typename T, int N>
CERES_DEPRECATED_WITH_MSG(
    "ceres::IsInfinite will be removed in a future Ceres Solver release. "
    "Please use ceres::isinf.")
inline bool IsInfinite(const Jet<T, N>& f) {
  return isinf(f);
}

#ifdef CERES_HAS_CPP20
// Computes the linear interpolation a + t(b - a) between a and b at the value
// t. For arguments outside of the range 0 <= t <= 1, the values are
// extrapolated.
//
// Differentiating lerp(a, b, t) with respect to a, b, and t gives:
//
//   d/da lerp(a, b, t) = 1 - t
//   d/db lerp(a, b, t) = t
//   d/dt lerp(a, b, t) = b - a
//
// with the dual representation given by
//
//   lerp(a + da, b + db, t + dt)
//      ~= lerp(a, b, t) + (1 - t) da + t db + (b - a) dt .
template <typename T, int N>
inline Jet<T, N> lerp(const Jet<T, N>& a,
                      const Jet<T, N>& b,
                      const Jet<T, N>& t) {
  return Jet<T, N>{lerp(a.a, b.a, t.a),
                   (T(1) - t.a) * a.v + t.a * b.v + (b.a - a.a) * t.v};
}

// Computes the midpoint a + (b - a) / 2.
//
// Differentiating midpoint(a, b) with respect to a and b gives:
//
//   d/da midpoint(a, b) = 1/2
//   d/db midpoint(a, b) = 1/2
//
// with the dual representation given by
//
//   midpoint(a + da, b + db) ~= midpoint(a, b) + (da + db) / 2 .
template <typename T, int N>
inline Jet<T, N> midpoint(const Jet<T, N>& a, const Jet<T, N>& b) {
  Jet<T, N> result{midpoint(a.a, b.a)};
  // To avoid overflow in the differential, compute
  // (da + db) / 2 using midpoint.
  for (int i = 0; i < N; ++i) {
    result.v[i] = midpoint(a.v[i], b.v[i]);
  }
  return result;
}
#endif  // defined(CERES_HAS_CPP20)

// atan2(b + db, a + da) ~= atan2(b, a) + (- b da + a db) / (a^2 + b^2)
//
// In words: the rate of change of theta is 1/r times the rate of
// change of (x, y) in the positive angular direction.
template <typename T, int N>
inline Jet<T, N> atan2(const Jet<T, N>& g, const Jet<T, N>& f) {
  // Note order of arguments:
  //
  //   f = a + da
  //   g = b + db

  T const tmp = T(1.0) / (f.a * f.a + g.a * g.a);
  return Jet<T, N>(atan2(g.a, f.a), tmp * (-g.a * f.v + f.a * g.v));
}

// Computes the square x^2 of a real number x (not the Euclidean L^2 norm as
// the name might suggest).
//
// NOTE: While std::norm is primarily intended for computing the squared
// magnitude of a std::complex<> number, the current Jet implementation does not
// support mixing a scalar T in its real part and std::complex<T> and in the
// infinitesimal. Mixed Jet support is necessary for the type decay from
// std::complex<T> to T (the squared magnitude of a complex number is always
// real) performed by std::norm.
//
// norm(x + h) ~= norm(x) + 2x h
template <typename T, int N>
inline Jet<T, N> norm(const Jet<T, N>& f) {
  return Jet<T, N>(norm(f.a), T(2) * f.a * f.v);
}

// pow -- base is a differentiable function, exponent is a constant.
// (a+da)^p ~= a^p + p*a^(p-1) da
template <typename T, int N>
inline Jet<T, N> pow(const Jet<T, N>& f, double g) {
  T const tmp = g * pow(f.a, g - T(1.0));
  return Jet<T, N>(pow(f.a, g), tmp * f.v);
}

// pow -- base is a constant, exponent is a differentiable function.
// We have various special cases, see the comment for pow(Jet, Jet) for
// analysis:
//
// 1. For f > 0 we have: (f)^(g + dg) ~= f^g + f^g log(f) dg
//
// 2. For f == 0 and g > 0 we have: (f)^(g + dg) ~= f^g
//
// 3. For f < 0 and integer g we have: (f)^(g + dg) ~= f^g but if dg
// != 0, the derivatives are not defined and we return NaN.

template <typename T, int N>
inline Jet<T, N> pow(T f, const Jet<T, N>& g) {
  Jet<T, N> result;

  if (fpclassify(f) == FP_ZERO && g > 0) {
    // Handle case 2.
    result = Jet<T, N>(T(0.0));
  } else {
    if (f < 0 && g == floor(g.a)) {  // Handle case 3.
      result = Jet<T, N>(pow(f, g.a));
      for (int i = 0; i < N; i++) {
        if (fpclassify(g.v[i]) != FP_ZERO) {
          // Return a NaN when g.v != 0.
          result.v[i] = std::numeric_limits<T>::quiet_NaN();
        }
      }
    } else {
      // Handle case 1.
      T const tmp = pow(f, g.a);
      result = Jet<T, N>(tmp, log(f) * tmp * g.v);
    }
  }

  return result;
}

// pow -- both base and exponent are differentiable functions. This has a
// variety of special cases that require careful handling.
//
// 1. For f > 0:
//    (f + df)^(g + dg) ~= f^g + f^(g - 1) * (g * df + f * log(f) * dg)
//    The numerical evaluation of f * log(f) for f > 0 is well behaved, even for
//    extremely small values (e.g. 1e-99).
//
// 2. For f == 0 and g > 1: (f + df)^(g + dg) ~= 0
//    This cases is needed because log(0) can not be evaluated in the f > 0
//    expression. However the function f*log(f) is well behaved around f == 0
//    and its limit as f-->0 is zero.
//
// 3. For f == 0 and g == 1: (f + df)^(g + dg) ~= 0 + df
//
// 4. For f == 0 and 0 < g < 1: The value is finite but the derivatives are not.
//
// 5. For f == 0 and g < 0: The value and derivatives of f^g are not finite.
//
// 6. For f == 0 and g == 0: The C standard incorrectly defines 0^0 to be 1
//    "because there are applications that can exploit this definition". We
//    (arbitrarily) decree that derivatives here will be nonfinite, since that
//    is consistent with the behavior for f == 0, g < 0 and 0 < g < 1.
//    Practically any definition could have been justified because mathematical
//    consistency has been lost at this point.
//
// 7. For f < 0, g integer, dg == 0: (f + df)^(g + dg) ~= f^g + g * f^(g - 1) df
//    This is equivalent to the case where f is a differentiable function and g
//    is a constant (to first order).
//
// 8. For f < 0, g integer, dg != 0: The value is finite but the derivatives are
//    not, because any change in the value of g moves us away from the point
//    with a real-valued answer into the region with complex-valued answers.
//
// 9. For f < 0, g noninteger: The value and derivatives of f^g are not finite.

template <typename T, int N>
inline Jet<T, N> pow(const Jet<T, N>& f, const Jet<T, N>& g) {
  Jet<T, N> result;

  if (fpclassify(f) == FP_ZERO && g >= 1) {
    // Handle cases 2 and 3.
    if (g > 1) {
      result = Jet<T, N>(T(0.0));
    } else {
      result = f;
    }

  } else {
    if (f < 0 && g == floor(g.a)) {
      // Handle cases 7 and 8.
      T const tmp = g.a * pow(f.a, g.a - T(1.0));
      result = Jet<T, N>(pow(f.a, g.a), tmp * f.v);
      for (int i = 0; i < N; i++) {
        if (fpclassify(g.v[i]) != FP_ZERO) {
          // Return a NaN when g.v != 0.
          result.v[i] = T(std::numeric_limits<double>::quiet_NaN());
        }
      }
    } else {
      // Handle the remaining cases. For cases 4,5,6,9 we allow the log()
      // function to generate -HUGE_VAL or NaN, since those cases result in a
      // nonfinite derivative.
      T const tmp1 = pow(f.a, g.a);
      T const tmp2 = g.a * pow(f.a, g.a - T(1.0));
      T const tmp3 = tmp1 * log(f.a);
      result = Jet<T, N>(tmp1, tmp2 * f.v + tmp3 * g.v);
    }
  }

  return result;
}

// Note: This has to be in the ceres namespace for argument dependent lookup to
// function correctly. Otherwise statements like CHECK_LE(x, 2.0) fail with
// strange compile errors.
template <typename T, int N>
inline std::ostream& operator<<(std::ostream& s, const Jet<T, N>& z) {
  s << "[" << z.a << " ; ";
  for (int i = 0; i < N; ++i) {
    s << z.v[i];
    if (i != N - 1) {
      s << ", ";
    }
  }
  s << "]";
  return s;
}
}  // namespace ceres

namespace std {
template <typename T, int N>
struct numeric_limits<ceres::Jet<T, N>> {
  static constexpr bool is_specialized = true;
  static constexpr bool is_signed = std::numeric_limits<T>::is_signed;
  static constexpr bool is_integer = std::numeric_limits<T>::is_integer;
  static constexpr bool is_exact = std::numeric_limits<T>::is_exact;
  static constexpr bool has_infinity = std::numeric_limits<T>::has_infinity;
  static constexpr bool has_quiet_NaN = std::numeric_limits<T>::has_quiet_NaN;
  static constexpr bool has_signaling_NaN =
      std::numeric_limits<T>::has_signaling_NaN;
  static constexpr bool is_iec559 = std::numeric_limits<T>::is_iec559;
  static constexpr bool is_bounded = std::numeric_limits<T>::is_bounded;
  static constexpr bool is_modulo = std::numeric_limits<T>::is_modulo;

  // has_denorm (and has_denorm_loss, not defined for Jet) has been deprecated
  // in C++23. However, without an intent to remove the declaration. Disable
  // deprecation warnings temporarily just for the corresponding symbols.
  CERES_DISABLE_DEPRECATED_WARNING
  static constexpr std::float_denorm_style has_denorm =
      std::numeric_limits<T>::has_denorm;
  CERES_RESTORE_DEPRECATED_WARNING
  static constexpr std::float_round_style round_style =
      std::numeric_limits<T>::round_style;

  static constexpr int digits = std::numeric_limits<T>::digits;
  static constexpr int digits10 = std::numeric_limits<T>::digits10;
  static constexpr int max_digits10 = std::numeric_limits<T>::max_digits10;
  static constexpr int radix = std::numeric_limits<T>::radix;
  static constexpr int min_exponent = std::numeric_limits<T>::min_exponent;
  static constexpr int min_exponent10 = std::numeric_limits<T>::max_exponent10;
  static constexpr int max_exponent = std::numeric_limits<T>::max_exponent;
  static constexpr int max_exponent10 = std::numeric_limits<T>::max_exponent10;
  static constexpr bool traps = std::numeric_limits<T>::traps;
  static constexpr bool tinyness_before =
      std::numeric_limits<T>::tinyness_before;

  static constexpr ceres::Jet<T, N> min
  CERES_PREVENT_MACRO_SUBSTITUTION() noexcept {
    return ceres::Jet<T, N>((std::numeric_limits<T>::min)());
  }
  static constexpr ceres::Jet<T, N> lowest() noexcept {
    return ceres::Jet<T, N>(std::numeric_limits<T>::lowest());
  }
  static constexpr ceres::Jet<T, N> epsilon() noexcept {
    return ceres::Jet<T, N>(std::numeric_limits<T>::epsilon());
  }
  static constexpr ceres::Jet<T, N> round_error() noexcept {
    return ceres::Jet<T, N>(std::numeric_limits<T>::round_error());
  }
  static constexpr ceres::Jet<T, N> infinity() noexcept {
    return ceres::Jet<T, N>(std::numeric_limits<T>::infinity());
  }
  static constexpr ceres::Jet<T, N> quiet_NaN() noexcept {
    return ceres::Jet<T, N>(std::numeric_limits<T>::quiet_NaN());
  }
  static constexpr ceres::Jet<T, N> signaling_NaN() noexcept {
    return ceres::Jet<T, N>(std::numeric_limits<T>::signaling_NaN());
  }
  static constexpr ceres::Jet<T, N> denorm_min() noexcept {
    return ceres::Jet<T, N>(std::numeric_limits<T>::denorm_min());
  }

  static constexpr ceres::Jet<T, N> max
  CERES_PREVENT_MACRO_SUBSTITUTION() noexcept {
    return ceres::Jet<T, N>((std::numeric_limits<T>::max)());
  }
};

}  // namespace std

namespace Eigen {

// Creating a specialization of NumTraits enables placing Jet objects inside
// Eigen arrays, getting all the goodness of Eigen combined with autodiff.
template <typename T, int N>
struct NumTraits<ceres::Jet<T, N>> {
  using Real = ceres::Jet<T, N>;
  using NonInteger = ceres::Jet<T, N>;
  using Nested = ceres::Jet<T, N>;
  using Literal = ceres::Jet<T, N>;

  static typename ceres::Jet<T, N> dummy_precision() {
    return ceres::Jet<T, N>(1e-12);
  }

  static inline Real epsilon() {
    return Real(std::numeric_limits<T>::epsilon());
  }

  static inline int digits10() { return NumTraits<T>::digits10(); }
  static inline int max_digits10() { return NumTraits<T>::max_digits10(); }

  enum {
    IsComplex = 0,
    IsInteger = 0,
    IsSigned,
    ReadCost = 1,
    AddCost = 1,
    // For Jet types, multiplication is more expensive than addition.
    MulCost = 3,
    HasFloatingPoint = 1,
    RequireInitialization = 1
  };

  template <bool Vectorized>
  struct Div {
    enum {
#if defined(EIGEN_VECTORIZE_AVX)
      AVX = true,
#else
      AVX = false,
#endif

      // Assuming that for Jets, division is as expensive as
      // multiplication.
      Cost = 3
    };
  };

  static inline Real highest() { return Real((std::numeric_limits<T>::max)()); }
  static inline Real lowest() { return Real(-(std::numeric_limits<T>::max)()); }
};

// Specifying the return type of binary operations between Jets and scalar types
// allows you to perform matrix/array operations with Eigen matrices and arrays
// such as addition, subtraction, multiplication, and division where one Eigen
// matrix/array is of type Jet and the other is a scalar type. This improves
// performance by using the optimized scalar-to-Jet binary operations but
// is only available on Eigen versions >= 3.3
template <typename BinaryOp, typename T, int N>
struct ScalarBinaryOpTraits<ceres::Jet<T, N>, T, BinaryOp> {
  using ReturnType = ceres::Jet<T, N>;
};
template <typename BinaryOp, typename T, int N>
struct ScalarBinaryOpTraits<T, ceres::Jet<T, N>, BinaryOp> {
  using ReturnType = ceres::Jet<T, N>;
};

}  // namespace Eigen

#endif  // CERES_PUBLIC_JET_H_
