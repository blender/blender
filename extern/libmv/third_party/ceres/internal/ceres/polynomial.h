// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2012 Google Inc. All rights reserved.
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
// Author: moll.markus@arcor.de (Markus Moll)
//         sameeragarwal@google.com (Sameer Agarwal)

#ifndef CERES_INTERNAL_POLYNOMIAL_SOLVER_H_
#define CERES_INTERNAL_POLYNOMIAL_SOLVER_H_

#include <vector>
#include "ceres/internal/eigen.h"
#include "ceres/internal/port.h"

namespace ceres {
namespace internal {

// All polynomials are assumed to be the form
//
//   sum_{i=0}^N polynomial(i) x^{N-i}.
//
// and are given by a vector of coefficients of size N + 1.

// Evaluate the polynomial at x using the Horner scheme.
inline double EvaluatePolynomial(const Vector& polynomial, double x) {
  double v = 0.0;
  for (int i = 0; i < polynomial.size(); ++i) {
    v = v * x + polynomial(i);
  }
  return v;
}

// Use the companion matrix eigenvalues to determine the roots of the
// polynomial.
//
// This function returns true on success, false otherwise.
// Failure indicates that the polynomial is invalid (of size 0) or
// that the eigenvalues of the companion matrix could not be computed.
// On failure, a more detailed message will be written to LOG(ERROR).
// If real is not NULL, the real parts of the roots will be returned in it.
// Likewise, if imaginary is not NULL, imaginary parts will be returned in it.
bool FindPolynomialRoots(const Vector& polynomial,
                         Vector* real,
                         Vector* imaginary);

// Return the derivative of the given polynomial. It is assumed that
// the input polynomial is at least of degree zero.
Vector DifferentiatePolynomial(const Vector& polynomial);

// Find the minimum value of the polynomial in the interval [x_min,
// x_max]. The minimum is obtained by computing all the roots of the
// derivative of the input polynomial. All real roots within the
// interval [x_min, x_max] are considered as well as the end points
// x_min and x_max. Since polynomials are differentiable functions,
// this ensures that the true minimum is found.
void MinimizePolynomial(const Vector& polynomial,
                        double x_min,
                        double x_max,
                        double* optimal_x,
                        double* optimal_value);

// Structure for storing sample values of a function.
//
// Clients can use this struct to communicate the value of the
// function and or its gradient at a given point x.
struct FunctionSample {
  FunctionSample()
      : x(0.0),
        value(0.0),
        value_is_valid(false),
        gradient(0.0),
        gradient_is_valid(false) {
  }

  double x;
  double value;      // value = f(x)
  bool value_is_valid;
  double gradient;   // gradient = f'(x)
  bool gradient_is_valid;
};

// Given a set of function value and/or gradient samples, find a
// polynomial whose value and gradients are exactly equal to the ones
// in samples.
//
// Generally speaking,
//
// degree = # values + # gradients - 1
//
// Of course its possible to sample a polynomial any number of times,
// in which case, generally speaking the spurious higher order
// coefficients will be zero.
Vector FindInterpolatingPolynomial(const vector<FunctionSample>& samples);

// Interpolate the function described by samples with a polynomial,
// and minimize it on the interval [x_min, x_max]. Depending on the
// input samples, it is possible that the interpolation or the root
// finding algorithms may fail due to numerical difficulties. But the
// function is guaranteed to return its best guess of an answer, by
// considering the samples and the end points as possible solutions.
void MinimizeInterpolatingPolynomial(const vector<FunctionSample>& samples,
                                     double x_min,
                                     double x_max,
                                     double* optimal_x,
                                     double* optimal_value);

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_POLYNOMIAL_SOLVER_H_
