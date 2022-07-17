// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2019 Google Inc. All rights reserved.
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
// Author: sameeragarwal@google.com (Sameer Agarwal)

#ifndef CERES_PUBLIC_GRADIENT_PROBLEM_H_
#define CERES_PUBLIC_GRADIENT_PROBLEM_H_

#include <memory>

#include "ceres/first_order_function.h"
#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/export.h"
#include "ceres/local_parameterization.h"
#include "ceres/manifold.h"

namespace ceres {

class FirstOrderFunction;

// Instances of GradientProblem represent general non-linear
// optimization problems that must be solved using just the value of
// the objective function and its gradient.

// Unlike the Problem class, which can only be used to model non-linear least
// squares problems, instances of GradientProblem are not restricted in the form
// of the objective function.
//
// Structurally GradientProblem is a composition of a FirstOrderFunction and
// optionally a Manifold.
//
// The FirstOrderFunction is responsible for evaluating the cost and gradient of
// the objective function.
//
// The Manifold is responsible for going back and forth between the ambient
// space and the local tangent space. (See manifold.h for more details). When a
// Manifold is not provided, then the tangent space is assumed to coincide with
// the ambient Euclidean space that the gradient vector lives in.
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
//     if (gradient != nullptr) {
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
//
// NOTE: We are currently in the process of transitioning from
// LocalParameterization to Manifolds in the Ceres API. During this period,
// GradientProblem will support using both Manifold and LocalParameterization
// objects interchangably. For methods in the API affected by this change, see
// their documentation below.
class CERES_EXPORT GradientProblem {
 public:
  // Takes ownership of the function.
  explicit GradientProblem(FirstOrderFunction* function);

  // Takes ownership of the function and the parameterization.
  //
  // NOTE: This constructor is deprecated and will be removed in the next public
  // release of Ceres Solver. Please move to using the Manifold based
  // constructor.
  CERES_DEPRECATED_WITH_MSG(
      "LocalParameterizations are deprecated. Please use the constructor that "
      "uses Manifold instead.")
  GradientProblem(FirstOrderFunction* function,
                  LocalParameterization* parameterization);

  // Takes ownership of the function and the manifold.
  GradientProblem(FirstOrderFunction* function, Manifold* manifold);

  int NumParameters() const;

  // Dimension of the manifold (and its tangent space).
  //
  // During the transition from LocalParameterization to Manifold, this method
  // reports the LocalSize of the LocalParameterization or the TangentSize of
  // the Manifold object associated with this problem.
  int NumTangentParameters() const;

  // Dimension of the manifold (and its tangent space).
  //
  // NOTE: This method is deprecated and will be removed in the next public
  // release of Ceres Solver. Please move to using NumTangentParameters()
  // instead.
  int NumLocalParameters() const { return NumTangentParameters(); }

  // This call is not thread safe.
  bool Evaluate(const double* parameters, double* cost, double* gradient) const;
  bool Plus(const double* x, const double* delta, double* x_plus_delta) const;

  const FirstOrderFunction* function() const { return function_.get(); }
  FirstOrderFunction* mutable_function() { return function_.get(); }

  // NOTE: During the transition from LocalParameterization to Manifold we need
  // to support both The LocalParameterization and Manifold based constructors.
  //
  // When the user uses the LocalParameterization, internally the solver will
  // wrap it in a ManifoldAdapter object and return it when manifold or
  // mutable_manifold are called.
  //
  // As a result this method will return a non-nullptr result if a Manifold or a
  // LocalParameterization was used when constructing the GradientProblem.
  const Manifold* manifold() const { return manifold_.get(); }
  Manifold* mutable_manifold() { return manifold_.get(); }

  // If the problem is constructed without a LocalParameterization or with a
  // Manifold this method will return a nullptr.
  //
  // NOTE: This method is deprecated and will be removed in the next public
  // release of Ceres Solver.
  CERES_DEPRECATED_WITH_MSG("Use Manifolds instead.")
  const LocalParameterization* parameterization() const {
    return parameterization_.get();
  }

  // If the problem is constructed without a LocalParameterization or with a
  // Manifold this method will return a nullptr.
  //
  // NOTE: This method is deprecated and will be removed in the next public
  // release of Ceres Solver.
  CERES_DEPRECATED_WITH_MSG("Use Manifolds instead.")
  LocalParameterization* mutable_parameterization() {
    return parameterization_.get();
  }

 private:
  std::unique_ptr<FirstOrderFunction> function_;
  CERES_DEPRECATED_WITH_MSG("")
  std::unique_ptr<LocalParameterization> parameterization_;
  std::unique_ptr<Manifold> manifold_;
  std::unique_ptr<double[]> scratch_;
};

}  // namespace ceres

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_PUBLIC_GRADIENT_PROBLEM_H_
