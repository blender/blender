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
// Author: keir@google.com (Keir Mierle)
//         sameeragarwal@google.com (Sameer Agarwal)
//
// Create CostFunctions as needed by the least squares framework with jacobians
// computed via numeric (a.k.a. finite) differentiation. For more details see
// http://en.wikipedia.org/wiki/Numerical_differentiation.
//
// To get an numerically differentiated cost function, you must define
// a class with a operator() (a functor) that computes the residuals.
//
// The function must write the computed value in the last argument
// (the only non-const one) and return true to indicate success.
// Please see cost_function.h for details on how the return value
// maybe used to impose simple constraints on the parameter block.
//
// For example, consider a scalar error e = k - x'y, where both x and y are
// two-dimensional column vector parameters, the prime sign indicates
// transposition, and k is a constant. The form of this error, which is the
// difference between a constant and an expression, is a common pattern in least
// squares problems. For example, the value x'y might be the model expectation
// for a series of measurements, where there is an instance of the cost function
// for each measurement k.
//
// The actual cost added to the total problem is e^2, or (k - x'k)^2; however,
// the squaring is implicitly done by the optimization framework.
//
// To write an numerically-differentiable cost function for the above model,
// first define the object
//
//   class MyScalarCostFunctor {
//     explicit MyScalarCostFunctor(double k): k_(k) {}
//
//     bool operator()(const double* const x,
//                     const double* const y,
//                     double* residuals) const {
//       residuals[0] = k_ - x[0] * y[0] - x[1] * y[1];
//       return true;
//     }
//
//    private:
//     double k_;
//   };
//
// Note that in the declaration of operator() the input parameters x
// and y come first, and are passed as const pointers to arrays of
// doubles. If there were three input parameters, then the third input
// parameter would come after y. The output is always the last
// parameter, and is also a pointer to an array. In the example above,
// the residual is a scalar, so only residuals[0] is set.
//
// Then given this class definition, the numerically differentiated
// cost function with central differences used for computing the
// derivative can be constructed as follows.
//
//   CostFunction* cost_function
//       = new NumericDiffCostFunction<MyScalarCostFunctor, CENTRAL, 1, 2, 2>(
//           new MyScalarCostFunctor(1.0));                    ^     ^  ^  ^
//                                                             |     |  |  |
//                                 Finite Differencing Scheme -+     |  |  |
//                                 Dimension of residual ------------+  |  |
//                                 Dimension of x ----------------------+  |
//                                 Dimension of y -------------------------+
//
// In this example, there is usually an instance for each measurement of k.
//
// In the instantiation above, the template parameters following
// "MyScalarCostFunctor", "1, 2, 2", describe the functor as computing
// a 1-dimensional output from two arguments, both 2-dimensional.
//
// NumericDiffCostFunction also supports cost functions with a
// runtime-determined number of residuals. For example:
//
// clang-format off
//
//   CostFunction* cost_function
//       = new NumericDiffCostFunction<MyScalarCostFunctor, CENTRAL, DYNAMIC, 2, 2>(
//           new CostFunctorWithDynamicNumResiduals(1.0),               ^     ^  ^
//           TAKE_OWNERSHIP,                                            |     |  |
//           runtime_number_of_residuals); <----+                       |     |  |
//                                              |                       |     |  |
//                                              |                       |     |  |
//             Actual number of residuals ------+                       |     |  |
//             Indicate dynamic number of residuals --------------------+     |  |
//             Dimension of x ------------------------------------------------+  |
//             Dimension of y ---------------------------------------------------+
// clang-format on
//
//
// The central difference method is considerably more accurate at the cost of
// twice as many function evaluations than forward difference. Consider using
// central differences begin with, and only after that works, trying forward
// difference to improve performance.
//
// WARNING #1: A common beginner's error when first using
// NumericDiffCostFunction is to get the sizing wrong. In particular,
// there is a tendency to set the template parameters to (dimension of
// residual, number of parameters) instead of passing a dimension
// parameter for *every parameter*. In the example above, that would
// be <MyScalarCostFunctor, 1, 2>, which is missing the last '2'
// argument. Please be careful when setting the size parameters.
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
// ALTERNATE INTERFACE
//
// For a variety of reasons, including compatibility with legacy code,
// NumericDiffCostFunction can also take CostFunction objects as
// input. The following describes how.
//
// To get a numerically differentiated cost function, define a
// subclass of CostFunction such that the Evaluate() function ignores
// the jacobian parameter. The numeric differentiation wrapper will
// fill in the jacobian parameter if necessary by repeatedly calling
// the Evaluate() function with small changes to the appropriate
// parameters, and computing the slope. For performance, the numeric
// differentiation wrapper class is templated on the concrete cost
// function, even though it could be implemented only in terms of the
// virtual CostFunction interface.
//
// The numerically differentiated version of a cost function for a cost function
// can be constructed as follows:
//
//   CostFunction* cost_function
//       = new NumericDiffCostFunction<MyCostFunction, CENTRAL, 1, 4, 8>(
//           new MyCostFunction(...), TAKE_OWNERSHIP);
//
// where MyCostFunction has 1 residual and 2 parameter blocks with sizes 4 and 8
// respectively. Look at the tests for a more detailed example.
//
// TODO(keir): Characterize accuracy; mention pitfalls; provide alternatives.

#ifndef CERES_PUBLIC_NUMERIC_DIFF_COST_FUNCTION_H_
#define CERES_PUBLIC_NUMERIC_DIFF_COST_FUNCTION_H_

#include <array>
#include <memory>

#include "Eigen/Dense"
#include "ceres/cost_function.h"
#include "ceres/internal/numeric_diff.h"
#include "ceres/internal/parameter_dims.h"
#include "ceres/numeric_diff_options.h"
#include "ceres/sized_cost_function.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres {

template <typename CostFunctor,
          NumericDiffMethodType method = CENTRAL,
          int kNumResiduals = 0,  // Number of residuals, or ceres::DYNAMIC
          int... Ns>              // Parameters dimensions for each block.
class NumericDiffCostFunction : public SizedCostFunction<kNumResiduals, Ns...> {
 public:
  NumericDiffCostFunction(
      CostFunctor* functor,
      Ownership ownership = TAKE_OWNERSHIP,
      int num_residuals = kNumResiduals,
      const NumericDiffOptions& options = NumericDiffOptions())
      : functor_(functor), ownership_(ownership), options_(options) {
    if (kNumResiduals == DYNAMIC) {
      SizedCostFunction<kNumResiduals, Ns...>::set_num_residuals(num_residuals);
    }
  }

  ~NumericDiffCostFunction() {
    if (ownership_ != TAKE_OWNERSHIP) {
      functor_.release();
    }
  }

  bool Evaluate(double const* const* parameters,
                double* residuals,
                double** jacobians) const override {
    using internal::FixedArray;
    using internal::NumericDiff;

    using ParameterDims =
        typename SizedCostFunction<kNumResiduals, Ns...>::ParameterDims;

    constexpr int kNumParameters = ParameterDims::kNumParameters;
    constexpr int kNumParameterBlocks = ParameterDims::kNumParameterBlocks;

    // Get the function value (residuals) at the the point to evaluate.
    if (!internal::VariadicEvaluate<ParameterDims>(
            *functor_, parameters, residuals)) {
      return false;
    }

    if (jacobians == NULL) {
      return true;
    }

    // Create a copy of the parameters which will get mutated.
    FixedArray<double> parameters_copy(kNumParameters);
    std::array<double*, kNumParameterBlocks> parameters_reference_copy =
        ParameterDims::GetUnpackedParameters(parameters_copy.data());

    for (int block = 0; block < kNumParameterBlocks; ++block) {
      memcpy(parameters_reference_copy[block],
             parameters[block],
             sizeof(double) * ParameterDims::GetDim(block));
    }

    internal::EvaluateJacobianForParameterBlocks<ParameterDims>::
        template Apply<method, kNumResiduals>(
            functor_.get(),
            residuals,
            options_,
            SizedCostFunction<kNumResiduals, Ns...>::num_residuals(),
            parameters_reference_copy.data(),
            jacobians);

    return true;
  }

 private:
  std::unique_ptr<CostFunctor> functor_;
  Ownership ownership_;
  NumericDiffOptions options_;
};

}  // namespace ceres

#endif  // CERES_PUBLIC_NUMERIC_DIFF_COST_FUNCTION_H_
