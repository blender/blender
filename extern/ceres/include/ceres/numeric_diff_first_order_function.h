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
// Author: sameeragarwal@google.com (Sameer Agarwal)

#ifndef CERES_PUBLIC_NUMERIC_DIFF_FIRST_ORDER_FUNCTION_H_
#define CERES_PUBLIC_NUMERIC_DIFF_FIRST_ORDER_FUNCTION_H_

#include <algorithm>
#include <memory>

#include "ceres/first_order_function.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/fixed_array.h"
#include "ceres/internal/numeric_diff.h"
#include "ceres/internal/parameter_dims.h"
#include "ceres/internal/variadic_evaluate.h"
#include "ceres/numeric_diff_options.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres {

// Creates FirstOrderFunctions as needed by the GradientProblem
// framework, with gradients computed via numeric differentiation. For
// more information on numeric differentiation, see the wikipedia
// article at https://en.wikipedia.org/wiki/Numerical_differentiation
//
// To get an numerically differentiated cost function, you must define
// a class with an operator() (a functor) that computes the cost.
//
// The function must write the computed value in the last argument
// (the only non-const one) and return true to indicate success.
//
// For example, consider a scalar error e = x'y - a, where both x and y are
// two-dimensional column vector parameters, the prime sign indicates
// transposition, and a is a constant.
//
// To write an numerically-differentiable cost function for the above model,
// first define the object
//
//  class QuadraticCostFunctor {
//   public:
//    explicit QuadraticCostFunctor(double a) : a_(a) {}
//    bool operator()(const double* const xy, double* cost) const {
//      constexpr int kInputVectorLength = 2;
//      const double* const x = xy;
//      const double* const y = xy + kInputVectorLength;
//      *cost = x[0] * y[0] + x[1] * y[1] - a_;
//      return true;
//    }
//
//   private:
//    double a_;
//  };
//
//
// Note that in the declaration of operator() the input parameters xy
// come first, and are passed as const pointers to array of
// doubles. The output cost is the last parameter.
//
// Then given this class definition, the numerically differentiated
// first order function with central differences used for computing the
// derivative can be constructed as follows.
//
//   FirstOrderFunction* function
//       = new NumericDiffFirstOrderFunction<MyScalarCostFunctor, CENTRAL, 4>(
//           new QuadraticCostFunctor(1.0));                   ^     ^     ^
//                                                             |     |     |
//                                 Finite Differencing Scheme -+     |     |
//                                 Dimension of xy ------------------------+
//
//
// In the instantiation above, the template parameters following
// "QuadraticCostFunctor", "CENTRAL, 4", describe the finite
// differencing scheme as "central differencing" and the functor as
// computing its cost from a 4 dimensional input.
//
// If the size of the parameter vector is not known at compile time, then an
// alternate construction syntax can be used:
//
//   FirstOrderFunction* function
//       = new NumericDiffFirstOrderFunction<MyScalarCostFunctor, CENTRAL>(
//           new QuadraticCostFunctor(1.0), 4);
//
// Note that instead of passing 4 as a template argument, it is now passed as
// the second argument to the constructor.
template <typename FirstOrderFunctor,
          NumericDiffMethodType kMethod,
          int kNumParameters = DYNAMIC>
class NumericDiffFirstOrderFunction final : public FirstOrderFunction {
 public:
  // Constructor for the case where the parameter size is known at compile time.
  explicit NumericDiffFirstOrderFunction(
      FirstOrderFunctor* functor,
      Ownership ownership = TAKE_OWNERSHIP,
      const NumericDiffOptions& options = NumericDiffOptions())
      : functor_(functor),
        num_parameters_(kNumParameters),
        ownership_(ownership),
        options_(options) {
    static_assert(kNumParameters != DYNAMIC,
                  "Number of parameters must be static when defined via the "
                  "template parameter. Use the other constructor for "
                  "dynamically sized functions.");
    static_assert(kNumParameters > 0, "kNumParameters must be positive");
  }

  // Constructor for the case where the parameter size is specified at run time.
  explicit NumericDiffFirstOrderFunction(
      FirstOrderFunctor* functor,
      int num_parameters,
      Ownership ownership = TAKE_OWNERSHIP,
      const NumericDiffOptions& options = NumericDiffOptions())
      : functor_(functor),
        num_parameters_(num_parameters),
        ownership_(ownership),
        options_(options) {
    static_assert(
        kNumParameters == DYNAMIC,
        "Template parameter must be DYNAMIC when using this constructor. If "
        "you want to provide the number of parameters statically use the other "
        "constructor.");
    CHECK_GT(num_parameters, 0);
  }

  ~NumericDiffFirstOrderFunction() override {
    if (ownership_ != TAKE_OWNERSHIP) {
      functor_.release();
    }
  }

  bool Evaluate(const double* const parameters,
                double* cost,
                double* gradient) const override {
    // Get the function value (cost) at the the point to evaluate.
    if (!(*functor_)(parameters, cost)) {
      return false;
    }

    if (gradient == nullptr) {
      return true;
    }

    // Create a copy of the parameters which will get mutated.
    internal::FixedArray<double, 32> parameters_copy(num_parameters_);
    std::copy_n(parameters, num_parameters_, parameters_copy.data());
    double* parameters_ptr = parameters_copy.data();
    constexpr int kNumResiduals = 1;
    if constexpr (kNumParameters == DYNAMIC) {
      internal::FirstOrderFunctorAdapter<FirstOrderFunctor> fofa(*functor_);
      return internal::NumericDiff<
          internal::FirstOrderFunctorAdapter<FirstOrderFunctor>,
          kMethod,
          kNumResiduals,
          internal::DynamicParameterDims,
          0,
          DYNAMIC>::EvaluateJacobianForParameterBlock(&fofa,
                                                      cost,
                                                      options_,
                                                      kNumResiduals,
                                                      0,
                                                      num_parameters_,
                                                      &parameters_ptr,
                                                      gradient);
    } else {
      return internal::EvaluateJacobianForParameterBlocks<
          internal::StaticParameterDims<kNumParameters>>::
          template Apply<kMethod, 1>(functor_.get(),
                                     cost,
                                     options_,
                                     kNumResiduals,
                                     &parameters_ptr,
                                     &gradient);
    }
  }

  int NumParameters() const override { return num_parameters_; }

  const FirstOrderFunctor& functor() const { return *functor_; }

 private:
  std::unique_ptr<FirstOrderFunctor> functor_;
  const int num_parameters_;
  const Ownership ownership_;
  const NumericDiffOptions options_;
};

}  // namespace ceres

#endif  // CERES_PUBLIC_NUMERIC_DIFF_FIRST_ORDER_FUNCTION_H_
