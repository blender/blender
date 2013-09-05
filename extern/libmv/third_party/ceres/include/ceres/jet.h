// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2010, 2011, 2012 Google Inc. All rights reserved.
// http://code.google.com/p/ceres-solver/
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
// While a complete treatment of the mechanics of automatic differentation is
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
// Next, augument 10 with an infinitesimal to get:
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
//   Jet<double, 2> x(0);  // Pick the 0th dual number for x.
//   Jet<double, 2> y(1);  // Pick the 1st dual number for y.
//   Jet<double, 2> z = f(x, y);
//
//   LG << "df/dx = " << z.a[0]
//      << "df/dy = " << z.a[1];
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
// A shorthand is to write an element as x = a + u, where u is the pertubation.
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
#include <iosfwd>
#include <iostream>  // NOLINT
#include <string>

#include "Eigen/Core"
#include "ceres/fpclassify.h"

namespace ceres {

template <typename T, int N>
struct Jet {
  enum { DIMENSION = N };

  // Default-construct "a" because otherwise this can lead to false errors about
  // uninitialized uses when other classes relying on default constructed T
  // (where T is a Jet<T, N>). This usually only happens in opt mode. Note that
  // the C++ standard mandates that e.g. default constructed doubles are
  // initialized to 0.0; see sections 8.5 of the C++03 standard.
  Jet() : a() {
    v.setZero();
  }

  // Constructor from scalar: a + 0.
  explicit Jet(const T& value) {
    a = value;
    v.setZero();
  }

  // Constructor from scalar plus variable: a + t_i.
  Jet(const T& value, int k) {
    a = value;
    v.setZero();
    v[k] = T(1.0);
  }

  // Compound operators
  Jet<T, N>& operator+=(const Jet<T, N> &y) {
    *this = *this + y;
    return *this;
  }

  Jet<T, N>& operator-=(const Jet<T, N> &y) {
    *this = *this - y;
    return *this;
  }

  Jet<T, N>& operator*=(const Jet<T, N> &y) {
    *this = *this * y;
    return *this;
  }

  Jet<T, N>& operator/=(const Jet<T, N> &y) {
    *this = *this / y;
    return *this;
  }

  // The scalar part.
  T a;

  // The infinitesimal part.
  //
  // Note the Eigen::DontAlign bit is needed here because this object
  // gets allocated on the stack and as part of other arrays and
  // structs. Forcing the right alignment there is the source of much
  // pain and suffering. Even if that works, passing Jets around to
  // functions by value has problems because the C++ ABI does not
  // guarantee alignment for function arguments.
  //
  // Setting the DontAlign bit prevents Eigen from using SSE for the
  // various operations on Jets. This is a small performance penalty
  // since the AutoDiff code will still expose much of the code as
  // statically sized loops to the compiler. But given the subtle
  // issues that arise due to alignment, especially when dealing with
  // multiple platforms, it seems to be a trade off worth making.
  Eigen::Matrix<T, N, 1, Eigen::DontAlign> v;
};

// Unary +
template<typename T, int N> inline
Jet<T, N> const& operator+(const Jet<T, N>& f) {
  return f;
}

// TODO(keir): Try adding __attribute__((always_inline)) to these functions to
// see if it causes a performance increase.

// Unary -
template<typename T, int N> inline
Jet<T, N> operator-(const Jet<T, N>&f) {
  Jet<T, N> g;
  g.a = -f.a;
  g.v = -f.v;
  return g;
}

// Binary +
template<typename T, int N> inline
Jet<T, N> operator+(const Jet<T, N>& f,
                    const Jet<T, N>& g) {
  Jet<T, N> h;
  h.a = f.a + g.a;
  h.v = f.v + g.v;
  return h;
}

// Binary + with a scalar: x + s
template<typename T, int N> inline
Jet<T, N> operator+(const Jet<T, N>& f, T s) {
  Jet<T, N> h;
  h.a = f.a + s;
  h.v = f.v;
  return h;
}

// Binary + with a scalar: s + x
template<typename T, int N> inline
Jet<T, N> operator+(T s, const Jet<T, N>& f) {
  Jet<T, N> h;
  h.a = f.a + s;
  h.v = f.v;
  return h;
}

// Binary -
template<typename T, int N> inline
Jet<T, N> operator-(const Jet<T, N>& f,
                    const Jet<T, N>& g) {
  Jet<T, N> h;
  h.a = f.a - g.a;
  h.v = f.v - g.v;
  return h;
}

// Binary - with a scalar: x - s
template<typename T, int N> inline
Jet<T, N> operator-(const Jet<T, N>& f, T s) {
  Jet<T, N> h;
  h.a = f.a - s;
  h.v = f.v;
  return h;
}

// Binary - with a scalar: s - x
template<typename T, int N> inline
Jet<T, N> operator-(T s, const Jet<T, N>& f) {
  Jet<T, N> h;
  h.a = s - f.a;
  h.v = -f.v;
  return h;
}

// Binary *
template<typename T, int N> inline
Jet<T, N> operator*(const Jet<T, N>& f,
                    const Jet<T, N>& g) {
  Jet<T, N> h;
  h.a = f.a * g.a;
  h.v = f.a * g.v + f.v * g.a;
  return h;
}

// Binary * with a scalar: x * s
template<typename T, int N> inline
Jet<T, N> operator*(const Jet<T, N>& f, T s) {
  Jet<T, N> h;
  h.a = f.a * s;
  h.v = f.v * s;
  return h;
}

// Binary * with a scalar: s * x
template<typename T, int N> inline
Jet<T, N> operator*(T s, const Jet<T, N>& f) {
  Jet<T, N> h;
  h.a = f.a * s;
  h.v = f.v * s;
  return h;
}

// Binary /
template<typename T, int N> inline
Jet<T, N> operator/(const Jet<T, N>& f,
                    const Jet<T, N>& g) {
  Jet<T, N> h;
  // This uses:
  //
  //   a + u   (a + u)(b - v)   (a + u)(b - v)
  //   ----- = -------------- = --------------
  //   b + v   (b + v)(b - v)        b^2
  //
  // which holds because v*v = 0.
  const T g_a_inverse = T(1.0) / g.a;
  h.a = f.a * g_a_inverse;
  const T f_a_by_g_a = f.a * g_a_inverse;
  for (int i = 0; i < N; ++i) {
    h.v[i] = (f.v[i] - f_a_by_g_a * g.v[i]) * g_a_inverse;
  }
  return h;
}

// Binary / with a scalar: s / x
template<typename T, int N> inline
Jet<T, N> operator/(T s, const Jet<T, N>& g) {
  Jet<T, N> h;
  h.a = s / g.a;
  const T minus_s_g_a_inverse2 = -s / (g.a * g.a);
  h.v = g.v * minus_s_g_a_inverse2;
  return h;
}

// Binary / with a scalar: x / s
template<typename T, int N> inline
Jet<T, N> operator/(const Jet<T, N>& f, T s) {
  Jet<T, N> h;
  const T s_inverse = 1.0 / s;
  h.a = f.a * s_inverse;
  h.v = f.v * s_inverse;
  return h;
}

// Binary comparison operators for both scalars and jets.
#define CERES_DEFINE_JET_COMPARISON_OPERATOR(op) \
template<typename T, int N> inline \
bool operator op(const Jet<T, N>& f, const Jet<T, N>& g) { \
  return f.a op g.a; \
} \
template<typename T, int N> inline \
bool operator op(const T& s, const Jet<T, N>& g) { \
  return s op g.a; \
} \
template<typename T, int N> inline \
bool operator op(const Jet<T, N>& f, const T& s) { \
  return f.a op s; \
}
CERES_DEFINE_JET_COMPARISON_OPERATOR( <  )  // NOLINT
CERES_DEFINE_JET_COMPARISON_OPERATOR( <= )  // NOLINT
CERES_DEFINE_JET_COMPARISON_OPERATOR( >  )  // NOLINT
CERES_DEFINE_JET_COMPARISON_OPERATOR( >= )  // NOLINT
CERES_DEFINE_JET_COMPARISON_OPERATOR( == )  // NOLINT
CERES_DEFINE_JET_COMPARISON_OPERATOR( != )  // NOLINT
#undef CERES_DEFINE_JET_COMPARISON_OPERATOR

// Pull some functions from namespace std.
//
// This is necessary because we want to use the same name (e.g. 'sqrt') for
// double-valued and Jet-valued functions, but we are not allowed to put
// Jet-valued functions inside namespace std.
//
// TODO(keir): Switch to "using".
inline double abs     (double x) { return std::abs(x);      }
inline double log     (double x) { return std::log(x);      }
inline double exp     (double x) { return std::exp(x);      }
inline double sqrt    (double x) { return std::sqrt(x);     }
inline double cos     (double x) { return std::cos(x);      }
inline double acos    (double x) { return std::acos(x);     }
inline double sin     (double x) { return std::sin(x);      }
inline double asin    (double x) { return std::asin(x);     }
inline double tan     (double x) { return std::tan(x);      }
inline double atan    (double x) { return std::atan(x);     }
inline double sinh    (double x) { return std::sinh(x);     }
inline double cosh    (double x) { return std::cosh(x);     }
inline double tanh    (double x) { return std::tanh(x);     }
inline double pow  (double x, double y) { return std::pow(x, y);   }
inline double atan2(double y, double x) { return std::atan2(y, x); }

// In general, f(a + h) ~= f(a) + f'(a) h, via the chain rule.

// abs(x + h) ~= x + h or -(x + h)
template <typename T, int N> inline
Jet<T, N> abs(const Jet<T, N>& f) {
  return f.a < T(0.0) ? -f : f;
}

// log(a + h) ~= log(a) + h / a
template <typename T, int N> inline
Jet<T, N> log(const Jet<T, N>& f) {
  Jet<T, N> g;
  g.a = log(f.a);
  const T a_inverse = T(1.0) / f.a;
  g.v = f.v * a_inverse;
  return g;
}

// exp(a + h) ~= exp(a) + exp(a) h
template <typename T, int N> inline
Jet<T, N> exp(const Jet<T, N>& f) {
  Jet<T, N> g;
  g.a = exp(f.a);
  g.v = g.a * f.v;
  return g;
}

// sqrt(a + h) ~= sqrt(a) + h / (2 sqrt(a))
template <typename T, int N> inline
Jet<T, N> sqrt(const Jet<T, N>& f) {
  Jet<T, N> g;
  g.a = sqrt(f.a);
  const T two_a_inverse = T(1.0) / (T(2.0) * g.a);
  g.v = f.v * two_a_inverse;
  return g;
}

// cos(a + h) ~= cos(a) - sin(a) h
template <typename T, int N> inline
Jet<T, N> cos(const Jet<T, N>& f) {
  Jet<T, N> g;
  g.a = cos(f.a);
  const T sin_a = sin(f.a);
  g.v = - sin_a * f.v;
  return g;
}

// acos(a + h) ~= acos(a) - 1 / sqrt(1 - a^2) h
template <typename T, int N> inline
Jet<T, N> acos(const Jet<T, N>& f) {
  Jet<T, N> g;
  g.a = acos(f.a);
  const T tmp = - T(1.0) / sqrt(T(1.0) - f.a * f.a);
  g.v = tmp * f.v;
  return g;
}

// sin(a + h) ~= sin(a) + cos(a) h
template <typename T, int N> inline
Jet<T, N> sin(const Jet<T, N>& f) {
  Jet<T, N> g;
  g.a = sin(f.a);
  const T cos_a = cos(f.a);
  g.v = cos_a * f.v;
  return g;
}

// asin(a + h) ~= asin(a) + 1 / sqrt(1 - a^2) h
template <typename T, int N> inline
Jet<T, N> asin(const Jet<T, N>& f) {
  Jet<T, N> g;
  g.a = asin(f.a);
  const T tmp = T(1.0) / sqrt(T(1.0) - f.a * f.a);
  g.v = tmp * f.v;
  return g;
}

// tan(a + h) ~= tan(a) + (1 + tan(a)^2) h
template <typename T, int N> inline
Jet<T, N> tan(const Jet<T, N>& f) {
  Jet<T, N> g;
  g.a = tan(f.a);
  double tan_a = tan(f.a);
  const T tmp = T(1.0) + tan_a * tan_a;
  g.v = tmp * f.v;
  return g;
}

// atan(a + h) ~= atan(a) + 1 / (1 + a^2) h
template <typename T, int N> inline
Jet<T, N> atan(const Jet<T, N>& f) {
  Jet<T, N> g;
  g.a = atan(f.a);
  const T tmp = T(1.0) / (T(1.0) + f.a * f.a);
  g.v = tmp * f.v;
  return g;
}

// sinh(a + h) ~= sinh(a) + cosh(a) h
template <typename T, int N> inline
Jet<T, N> sinh(const Jet<T, N>& f) {
  Jet<T, N> g;
  g.a = sinh(f.a);
  const T cosh_a = cosh(f.a);
  g.v = cosh_a * f.v;
  return g;
}

// cosh(a + h) ~= cosh(a) + sinh(a) h
template <typename T, int N> inline
Jet<T, N> cosh(const Jet<T, N>& f) {
  Jet<T, N> g;
  g.a = cosh(f.a);
  const T sinh_a = sinh(f.a);
  g.v = sinh_a * f.v;
  return g;
}

// tanh(a + h) ~= tanh(a) + (1 - tanh(a)^2) h
template <typename T, int N> inline
Jet<T, N> tanh(const Jet<T, N>& f) {
  Jet<T, N> g;
  g.a = tanh(f.a);
  double tanh_a = tanh(f.a);
  const T tmp = T(1.0) - tanh_a * tanh_a;
  g.v = tmp * f.v;
  return g;
}

// Jet Classification. It is not clear what the appropriate semantics are for
// these classifications. This picks that IsFinite and isnormal are "all"
// operations, i.e. all elements of the jet must be finite for the jet itself
// to be finite (or normal). For IsNaN and IsInfinite, the answer is less
// clear. This takes a "any" approach for IsNaN and IsInfinite such that if any
// part of a jet is nan or inf, then the entire jet is nan or inf. This leads
// to strange situations like a jet can be both IsInfinite and IsNaN, but in
// practice the "any" semantics are the most useful for e.g. checking that
// derivatives are sane.

// The jet is finite if all parts of the jet are finite.
template <typename T, int N> inline
bool IsFinite(const Jet<T, N>& f) {
  if (!IsFinite(f.a)) {
    return false;
  }
  for (int i = 0; i < N; ++i) {
    if (!IsFinite(f.v[i])) {
      return false;
    }
  }
  return true;
}

// The jet is infinite if any part of the jet is infinite.
template <typename T, int N> inline
bool IsInfinite(const Jet<T, N>& f) {
  if (IsInfinite(f.a)) {
    return true;
  }
  for (int i = 0; i < N; i++) {
    if (IsInfinite(f.v[i])) {
      return true;
    }
  }
  return false;
}

// The jet is NaN if any part of the jet is NaN.
template <typename T, int N> inline
bool IsNaN(const Jet<T, N>& f) {
  if (IsNaN(f.a)) {
    return true;
  }
  for (int i = 0; i < N; ++i) {
    if (IsNaN(f.v[i])) {
      return true;
    }
  }
  return false;
}

// The jet is normal if all parts of the jet are normal.
template <typename T, int N> inline
bool IsNormal(const Jet<T, N>& f) {
  if (!IsNormal(f.a)) {
    return false;
  }
  for (int i = 0; i < N; ++i) {
    if (!IsNormal(f.v[i])) {
      return false;
    }
  }
  return true;
}

// atan2(b + db, a + da) ~= atan2(b, a) + (- b da + a db) / (a^2 + b^2)
//
// In words: the rate of change of theta is 1/r times the rate of
// change of (x, y) in the positive angular direction.
template <typename T, int N> inline
Jet<T, N> atan2(const Jet<T, N>& g, const Jet<T, N>& f) {
  // Note order of arguments:
  //
  //   f = a + da
  //   g = b + db

  Jet<T, N> out;

  out.a = atan2(g.a, f.a);

  T const temp = T(1.0) / (f.a * f.a + g.a * g.a);
  out.v = temp * (- g.a * f.v + f.a * g.v);
  return out;
}


// pow -- base is a differentiatble function, exponent is a constant.
// (a+da)^p ~= a^p + p*a^(p-1) da
template <typename T, int N> inline
Jet<T, N> pow(const Jet<T, N>& f, double g) {
  Jet<T, N> out;
  out.a = pow(f.a, g);
  T const temp = g * pow(f.a, g - T(1.0));
  out.v = temp * f.v;
  return out;
}

// pow -- base is a constant, exponent is a differentiable function.
// (a)^(p+dp) ~= a^p + a^p log(a) dp
template <typename T, int N> inline
Jet<T, N> pow(double f, const Jet<T, N>& g) {
  Jet<T, N> out;
  out.a = pow(f, g.a);
  T const temp = log(f) * out.a;
  out.v = temp * g.v;
  return out;
}


// pow -- both base and exponent are differentiable functions.
// (a+da)^(b+db) ~= a^b + b * a^(b-1) da + a^b log(a) * db
template <typename T, int N> inline
Jet<T, N> pow(const Jet<T, N>& f, const Jet<T, N>& g) {
  Jet<T, N> out;

  T const temp1 = pow(f.a, g.a);
  T const temp2 = g.a * pow(f.a, g.a - T(1.0));
  T const temp3 = temp1 * log(f.a);

  out.a = temp1;
  out.v = temp2 * f.v + temp3 * g.v;
  return out;
}

// Define the helper functions Eigen needs to embed Jet types.
//
// NOTE(keir): machine_epsilon() and precision() are missing, because they don't
// work with nested template types (e.g. where the scalar is itself templated).
// Among other things, this means that decompositions of Jet's does not work,
// for example
//
//   Matrix<Jet<T, N> ... > A, x, b;
//   ...
//   A.solve(b, &x)
//
// does not work and will fail with a strange compiler error.
//
// TODO(keir): This is an Eigen 2.0 limitation that is lifted in 3.0. When we
// switch to 3.0, also add the rest of the specialization functionality.
template<typename T, int N> inline const Jet<T, N>& ei_conj(const Jet<T, N>& x) { return x;              }  // NOLINT
template<typename T, int N> inline const Jet<T, N>& ei_real(const Jet<T, N>& x) { return x;              }  // NOLINT
template<typename T, int N> inline       Jet<T, N>  ei_imag(const Jet<T, N>&  ) { return Jet<T, N>(0.0); }  // NOLINT
template<typename T, int N> inline       Jet<T, N>  ei_abs (const Jet<T, N>& x) { return fabs(x);        }  // NOLINT
template<typename T, int N> inline       Jet<T, N>  ei_abs2(const Jet<T, N>& x) { return x * x;          }  // NOLINT
template<typename T, int N> inline       Jet<T, N>  ei_sqrt(const Jet<T, N>& x) { return sqrt(x);        }  // NOLINT
template<typename T, int N> inline       Jet<T, N>  ei_exp (const Jet<T, N>& x) { return exp(x);         }  // NOLINT
template<typename T, int N> inline       Jet<T, N>  ei_log (const Jet<T, N>& x) { return log(x);         }  // NOLINT
template<typename T, int N> inline       Jet<T, N>  ei_sin (const Jet<T, N>& x) { return sin(x);         }  // NOLINT
template<typename T, int N> inline       Jet<T, N>  ei_cos (const Jet<T, N>& x) { return cos(x);         }  // NOLINT
template<typename T, int N> inline       Jet<T, N>  ei_tan (const Jet<T, N>& x) { return tan(x);         }  // NOLINT
template<typename T, int N> inline       Jet<T, N>  ei_atan(const Jet<T, N>& x) { return atan(x);        }  // NOLINT
template<typename T, int N> inline       Jet<T, N>  ei_sinh(const Jet<T, N>& x) { return sinh(x);        }  // NOLINT
template<typename T, int N> inline       Jet<T, N>  ei_cosh(const Jet<T, N>& x) { return cosh(x);        }  // NOLINT
template<typename T, int N> inline       Jet<T, N>  ei_tanh(const Jet<T, N>& x) { return tanh(x);        }  // NOLINT
template<typename T, int N> inline       Jet<T, N>  ei_pow (const Jet<T, N>& x, Jet<T, N> y) { return pow(x, y); }  // NOLINT

// Note: This has to be in the ceres namespace for argument dependent lookup to
// function correctly. Otherwise statements like CHECK_LE(x, 2.0) fail with
// strange compile errors.
template <typename T, int N>
inline std::ostream &operator<<(std::ostream &s, const Jet<T, N>& z) {
  return s << "[" << z.a << " ; " << z.v.transpose() << "]";
}

}  // namespace ceres

namespace Eigen {

// Creating a specialization of NumTraits enables placing Jet objects inside
// Eigen arrays, getting all the goodness of Eigen combined with autodiff.
template<typename T, int N>
struct NumTraits<ceres::Jet<T, N> > {
  typedef ceres::Jet<T, N> Real;
  typedef ceres::Jet<T, N> NonInteger;
  typedef ceres::Jet<T, N> Nested;

  static typename ceres::Jet<T, N> dummy_precision() {
    return ceres::Jet<T, N>(1e-12);
  }

  enum {
    IsComplex = 0,
    IsInteger = 0,
    IsSigned,
    ReadCost = 1,
    AddCost = 1,
    // For Jet types, multiplication is more expensive than addition.
    MulCost = 3,
    HasFloatingPoint = 1
  };
};

}  // namespace Eigen

#endif  // CERES_PUBLIC_JET_H_
