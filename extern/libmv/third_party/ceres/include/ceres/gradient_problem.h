// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2014 Google Inc. All rights reserved.
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
// Author: sameeragarwal@google.com (Sameer Agarwal)

#ifndef CERES_PUBLIC_GRADIENT_PROBLEM_H_
#define CERES_PUBLIC_GRADIENT_PROBLEM_H_

#include "ceres/internal/macros.h"
#include "ceres/internal/port.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/local_parameterization.h"

namespace ceres {

class FirstOrderFunction;

// Instances of GradientProblem represent general non-linear
// optimization problems that must be solved using just the value of
// the objective function and its gradient. Unlike the Problem class,
// which can only be used to model non-linear least squares problems,
// instances of GradientProblem not restricted in the form of the
// objective function.
//
// Structurally GradientProblem is a composition of a
// FirstOrderFunction and optionally a LocalParameterization.
//
// The FirstOrderFunction is responsible for evaluating the cost and
// gradient of the objective function.
//
// The LocalParameterization is responsible for going back and forth
// between the ambient space and the local tangent space. (See
// local_parameterization.h for more details). When a
// LocalParameterization is not provided, then the tangent space is
// assumed to coincide with the ambient Euclidean space that the
// gradient vector lives in.
//
// Example usage:
//
// The following demonstrate the problem construction for Rosenbrock's function
//
//   f(x,y) = (1-x)^2 + 100(y - x^2)^2;
//
// class Rosenbrock : public ceres::FirstOrderFunction {
//  public:
//   virtual ~Rosenbrock() {}
//
//   virtual bool Evaluate(const double* parameters,
//                         double* cost,
//                         double* gradient) const {
//     const double x = parameters[0];
//     const double y = parameters[1];
//
//     cost[0] = (1.0 - x) * (1.0 - x) + 100.0 * (y - x * x) * (y - x * x);
//     if (gradient != NULL) {
//       gradient[0] = -2.0 * (1.0 - x) - 200.0 * (y - x * x) * 2.0 * x;
//       gradient[1] = 200.0 * (y - x * x);
//     }
//     return true;
//   };
//
//   virtual int NumParameters() const { return 2; };
// };
//
// ceres::GradientProblem problem(new Rosenbrock());
class CERES_EXPORT GradientProblem {
 public:
  // Takes ownership of the function.
  explicit GradientProblem(FirstOrderFunction* function);

  // Takes ownership of the function and the parameterization.
  GradientProblem(FirstOrderFunction* function,
                  LocalParameterization* parameterization);

  int NumParameters() const;
  int NumLocalParameters() const;

  // This call is not thread safe.
  bool Evaluate(const double* parameters, double* cost, double* gradient) const;
  bool Plus(const double* x, const double* delta, double* x_plus_delta) const;

 private:
  internal::scoped_ptr<FirstOrderFunction> function_;
  internal::scoped_ptr<LocalParameterization> parameterization_;
  internal::scoped_array<double> scratch_;
};

// A FirstOrderFunction object implements the evaluation of a function
// and its gradient.
class CERES_EXPORT FirstOrderFunction {
 public:
  virtual ~FirstOrderFunction() {}
  // cost is never NULL. gradient may be null.
  virtual bool Evaluate(const double* const parameters,
                        double* cost,
                        double* gradient) const = 0;
  virtual int NumParameters() const = 0;
};

}  // namespace ceres

#endif  // CERES_PUBLIC_GRADIENT_PROBLEM_H_
