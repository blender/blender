// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2015 Google Inc. All rights reserved.
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
// To write an numerically-differentiable cost function for the above model, first
// define the object
//
//   class MyScalarCostFunctor {
//     MyScalarCostFunctor(double k): k_(k) {}
//
//     bool operator()(const double* const x,
//                     const double* const y,
//                     double* residuals) const {
//       residuals[0] = k_ - x[0] * y[0] + x[1] * y[1];
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
//
// The framework can currently accommodate cost functions of up to 10
// independent variables, and there is no limit on the dimensionality
// of each of them.
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

#include "Eigen/Dense"
#include "ceres/cost_function.h"
#include "ceres/internal/numeric_diff.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/numeric_diff_options.h"
#include "ceres/sized_cost_function.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres {

template <typename CostFunctor,
          NumericDiffMethodType method = CENTRAL,
          int kNumResiduals = 0,  // Number of residuals, or ceres::DYNAMIC
          int N0 = 0,   // Number of parameters in block 0.
          int N1 = 0,   // Number of parameters in block 1.
          int N2 = 0,   // Number of parameters in block 2.
          int N3 = 0,   // Number of parameters in block 3.
          int N4 = 0,   // Number of parameters in block 4.
          int N5 = 0,   // Number of parameters in block 5.
          int N6 = 0,   // Number of parameters in block 6.
          int N7 = 0,   // Number of parameters in block 7.
          int N8 = 0,   // Number of parameters in block 8.
          int N9 = 0>   // Number of parameters in block 9.
class NumericDiffCostFunction
    : public SizedCostFunction<kNumResiduals,
                               N0, N1, N2, N3, N4,
                               N5, N6, N7, N8, N9> {
 public:
  NumericDiffCostFunction(
      CostFunctor* functor,
      Ownership ownership = TAKE_OWNERSHIP,
      int num_residuals = kNumResiduals,
      const NumericDiffOptions& options = NumericDiffOptions())
      : functor_(functor),
        ownership_(ownership),
        options_(options) {
    if (kNumResiduals == DYNAMIC) {
      SizedCostFunction<kNumResiduals,
                        N0, N1, N2, N3, N4,
                        N5, N6, N7, N8, N9>
          ::set_num_residuals(num_residuals);
    }
  }

  // Deprecated. New users should avoid using this constructor. Instead, use the
  // constructor with NumericDiffOptions.
  NumericDiffCostFunction(CostFunctor* functor,
                          Ownership ownership,
                          int num_residuals,
                          const double relative_step_size)
      :functor_(functor),
       ownership_(ownership),
       options_() {
    LOG(WARNING) << "This constructor is deprecated and will be removed in "
                    "a future version. Please use the NumericDiffOptions "
                    "constructor instead.";

    if (kNumResiduals == DYNAMIC) {
      SizedCostFunction<kNumResiduals,
                        N0, N1, N2, N3, N4,
                        N5, N6, N7, N8, N9>
          ::set_num_residuals(num_residuals);
    }

    options_.relative_step_size = relative_step_size;
  }

  ~NumericDiffCostFunction() {
    if (ownership_ != TAKE_OWNERSHIP) {
      functor_.release();
    }
  }

  virtual bool Evaluate(double const* const* parameters,
                        double* residuals,
                        double** jacobians) const {
    using internal::FixedArray;
    using internal::NumericDiff;

    const int kNumParameters = N0 + N1 + N2 + N3 + N4 + N5 + N6 + N7 + N8 + N9;
    const int kNumParameterBlocks =
        (N0 > 0) + (N1 > 0) + (N2 > 0) + (N3 > 0) + (N4 > 0) +
        (N5 > 0) + (N6 > 0) + (N7 > 0) + (N8 > 0) + (N9 > 0);

    // Get the function value (residuals) at the the point to evaluate.
    if (!internal::EvaluateImpl<CostFunctor,
                                N0, N1, N2, N3, N4, N5, N6, N7, N8, N9>(
                                    functor_.get(),
                                    parameters,
                                    residuals,
                                    functor_.get())) {
      return false;
    }

    if (jacobians == NULL) {
      return true;
    }

    // Create a copy of the parameters which will get mutated.
    FixedArray<double> parameters_copy(kNumParameters);
    FixedArray<double*> parameters_reference_copy(kNumParameterBlocks);

    parameters_reference_copy[0] = parameters_copy.get();
    if (N1) parameters_reference_copy[1] = parameters_reference_copy[0] + N0;
    if (N2) parameters_reference_copy[2] = parameters_reference_copy[1] + N1;
    if (N3) parameters_reference_copy[3] = parameters_reference_copy[2] + N2;
    if (N4) parameters_reference_copy[4] = parameters_reference_copy[3] + N3;
    if (N5) parameters_reference_copy[5] = parameters_reference_copy[4] + N4;
    if (N6) parameters_reference_copy[6] = parameters_reference_copy[5] + N5;
    if (N7) parameters_reference_copy[7] = parameters_reference_copy[6] + N6;
    if (N8) parameters_reference_copy[8] = parameters_reference_copy[7] + N7;
    if (N9) parameters_reference_copy[9] = parameters_reference_copy[8] + N8;

#define CERES_COPY_PARAMETER_BLOCK(block)                               \
  if (N ## block) memcpy(parameters_reference_copy[block],              \
                         parameters[block],                             \
                         sizeof(double) * N ## block);  // NOLINT

    CERES_COPY_PARAMETER_BLOCK(0);
    CERES_COPY_PARAMETER_BLOCK(1);
    CERES_COPY_PARAMETER_BLOCK(2);
    CERES_COPY_PARAMETER_BLOCK(3);
    CERES_COPY_PARAMETER_BLOCK(4);
    CERES_COPY_PARAMETER_BLOCK(5);
    CERES_COPY_PARAMETER_BLOCK(6);
    CERES_COPY_PARAMETER_BLOCK(7);
    CERES_COPY_PARAMETER_BLOCK(8);
    CERES_COPY_PARAMETER_BLOCK(9);

#undef CERES_COPY_PARAMETER_BLOCK

#define CERES_EVALUATE_JACOBIAN_FOR_BLOCK(block)                        \
    if (N ## block && jacobians[block] != NULL) {                       \
      if (!NumericDiff<CostFunctor,                                     \
                       method,                                          \
                       kNumResiduals,                                   \
                       N0, N1, N2, N3, N4, N5, N6, N7, N8, N9,          \
                       block,                                           \
                       N ## block >::EvaluateJacobianForParameterBlock( \
                           functor_.get(),                              \
                           residuals,                                   \
                           options_,                                    \
                          SizedCostFunction<kNumResiduals,              \
                           N0, N1, N2, N3, N4,                          \
                           N5, N6, N7, N8, N9>::num_residuals(),        \
                           block,                                       \
                           N ## block,                                  \
                           parameters_reference_copy.get(),             \
                           jacobians[block])) {                         \
        return false;                                                   \
      }                                                                 \
    }

    CERES_EVALUATE_JACOBIAN_FOR_BLOCK(0);
    CERES_EVALUATE_JACOBIAN_FOR_BLOCK(1);
    CERES_EVALUATE_JACOBIAN_FOR_BLOCK(2);
    CERES_EVALUATE_JACOBIAN_FOR_BLOCK(3);
    CERES_EVALUATE_JACOBIAN_FOR_BLOCK(4);
    CERES_EVALUATE_JACOBIAN_FOR_BLOCK(5);
    CERES_EVALUATE_JACOBIAN_FOR_BLOCK(6);
    CERES_EVALUATE_JACOBIAN_FOR_BLOCK(7);
    CERES_EVALUATE_JACOBIAN_FOR_BLOCK(8);
    CERES_EVALUATE_JACOBIAN_FOR_BLOCK(9);

#undef CERES_EVALUATE_JACOBIAN_FOR_BLOCK

    return true;
  }

 private:
  internal::scoped_ptr<CostFunctor> functor_;
  Ownership ownership_;
  NumericDiffOptions options_;
};

}  // namespace ceres

#endif  // CERES_PUBLIC_NUMERIC_DIFF_COST_FUNCTION_H_
