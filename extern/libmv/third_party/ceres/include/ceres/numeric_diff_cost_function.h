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
// Create CostFunctions as needed by the least squares framework with jacobians
// computed via numeric (a.k.a. finite) differentiation. For more details see
// http://en.wikipedia.org/wiki/Numerical_differentiation.
//
// To get a numerically differentiated cost function, define a subclass of
// CostFunction such that the Evaluate() function ignores the jacobian
// parameter. The numeric differentiation wrapper will fill in the jacobian
// parameter if nececssary by repeatedly calling the Evaluate() function with
// small changes to the appropriate parameters, and computing the slope. For
// performance, the numeric differentiation wrapper class is templated on the
// concrete cost function, even though it could be implemented only in terms of
// the virtual CostFunction interface.
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
// The central difference method is considerably more accurate at the cost of
// twice as many function evaluations than forward difference. Consider using
// central differences begin with, and only after that works, trying forward
// difference to improve performance.
//
// TODO(keir): Characterize accuracy; mention pitfalls; provide alternatives.

#ifndef CERES_PUBLIC_NUMERIC_DIFF_COST_FUNCTION_H_
#define CERES_PUBLIC_NUMERIC_DIFF_COST_FUNCTION_H_

#include <cstring>
#include <glog/logging.h>
#include "Eigen/Dense"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/sized_cost_function.h"
#include "ceres/types.h"

namespace ceres {

enum NumericDiffMethod {
  CENTRAL,
  FORWARD
};

// This is split from the main class because C++ doesn't allow partial template
// specializations for member functions. The alternative is to repeat the main
// class for differing numbers of parameters, which is also unfortunate.
template <typename CostFunctionNoJacobian,
          int num_residuals,
          int parameter_block_size,
          int parameter_block,
          NumericDiffMethod method>
struct Differencer {
  // Mutates parameters but must restore them before return.
  static bool EvaluateJacobianForParameterBlock(
      const CostFunctionNoJacobian *function,
      double const* residuals_at_eval_point,
      double **parameters,
      double **jacobians) {
    using Eigen::Map;
    using Eigen::Matrix;
    using Eigen::RowMajor;

    typedef Matrix<double, num_residuals, 1> ResidualVector;
    typedef Matrix<double, parameter_block_size, 1> ParameterVector;
    typedef Matrix<double, num_residuals, parameter_block_size, RowMajor>
        JacobianMatrix;

    Map<JacobianMatrix> parameter_jacobian(jacobians[parameter_block],
                                           num_residuals,
                                           parameter_block_size);

    // Mutate 1 element at a time and then restore.
    Map<ParameterVector> x_plus_delta(parameters[parameter_block],
                                      parameter_block_size);
    ParameterVector x(x_plus_delta);

    // TODO(keir): Pick a smarter number! In theory a good choice is sqrt(eps) *
    // x, which for doubles means about 1e-8 * x. However, I have found this
    // number too optimistic. This number should be exposed for users to change.
    const double kRelativeStepSize = 1e-6;

    ParameterVector step_size = x.array().abs() * kRelativeStepSize;

    // To handle cases where a parameter is exactly zero, instead use the mean
    // step_size for the other dimensions.
    double fallback_step_size = step_size.sum() / step_size.rows();
    if (fallback_step_size == 0.0) {
      // If all the parameters are zero, there's no good answer. Take
      // kRelativeStepSize as a guess and hope for the best.
      fallback_step_size = kRelativeStepSize;
    }

    // For each parameter in the parameter block, use finite differences to
    // compute the derivative for that parameter.
    for (int j = 0; j < parameter_block_size; ++j) {
      if (step_size(j) == 0.0) {
        // The parameter is exactly zero, so compromise and use the mean
        // step_size from the other parameters. This can break in many cases,
        // but it's hard to pick a good number without problem specific
        // knowledge.
        step_size(j) = fallback_step_size;
      }
      x_plus_delta(j) = x(j) + step_size(j);

      double residuals[num_residuals];  // NOLINT
      if (!function->Evaluate(parameters, residuals, NULL)) {
        // Something went wrong; bail.
        return false;
      }

      // Compute this column of the jacobian in 3 steps:
      // 1. Store residuals for the forward part.
      // 2. Subtract residuals for the backward (or 0) part.
      // 3. Divide out the run.
      parameter_jacobian.col(j) =
          Map<const ResidualVector>(residuals, num_residuals);

      double one_over_h = 1 / step_size(j);
      if (method == CENTRAL) {
        // Compute the function on the other side of x(j).
        x_plus_delta(j) = x(j) - step_size(j);

        if (!function->Evaluate(parameters, residuals, NULL)) {
          // Something went wrong; bail.
          return false;
        }
        parameter_jacobian.col(j) -=
            Map<ResidualVector>(residuals, num_residuals, 1);
        one_over_h /= 2;
      } else {
        // Forward difference only; reuse existing residuals evaluation.
        parameter_jacobian.col(j) -=
            Map<const ResidualVector>(residuals_at_eval_point, num_residuals);
      }
      x_plus_delta(j) = x(j);  // Restore x_plus_delta.

      // Divide out the run to get slope.
      parameter_jacobian.col(j) *= one_over_h;
    }
    return true;
  }
};

// Prevent invalid instantiations.
template <typename CostFunctionNoJacobian,
          int num_residuals,
          int parameter_block,
          NumericDiffMethod method>
struct Differencer<CostFunctionNoJacobian,
                  num_residuals,
                  0 /* parameter_block_size */,
                  parameter_block,
                  method> {
  static bool EvaluateJacobianForParameterBlock(
      const CostFunctionNoJacobian *function,
      double const* residuals_at_eval_point,
      double **parameters,
      double **jacobians) {
    LOG(FATAL) << "Shouldn't get here.";
    return true;
  }
};

template <typename CostFunctionNoJacobian,
         NumericDiffMethod method = CENTRAL, int M = 0,
         int N0 = 0, int N1 = 0, int N2 = 0, int N3 = 0, int N4 = 0, int N5 = 0>
class NumericDiffCostFunction
    : public SizedCostFunction<M, N0, N1, N2, N3, N4, N5> {
 public:
  NumericDiffCostFunction(CostFunctionNoJacobian* function,
                          Ownership ownership)
      : function_(function), ownership_(ownership) {}

  virtual ~NumericDiffCostFunction() {
    if (ownership_ != TAKE_OWNERSHIP) {
      function_.release();
    }
  }

  virtual bool Evaluate(double const* const* parameters,
                        double* residuals,
                        double** jacobians) const {
    // Get the function value (residuals) at the the point to evaluate.
    bool success = function_->Evaluate(parameters, residuals, NULL);
    if (!success) {
      // Something went wrong; ignore the jacobian.
      return false;
    }
    if (!jacobians) {
      // Nothing to do; just forward.
      return true;
    }

    // Create a copy of the parameters which will get mutated.
    const int kParametersSize = N0 + N1 + N2 + N3 + N4 + N5;
    double parameters_copy[kParametersSize];
    double *parameters_references_copy[6];
    parameters_references_copy[0] = &parameters_copy[0];
    parameters_references_copy[1] = &parameters_copy[0] + N0;
    parameters_references_copy[2] = &parameters_copy[0] + N0 + N1;
    parameters_references_copy[3] = &parameters_copy[0] + N0 + N1 + N2;
    parameters_references_copy[4] = &parameters_copy[0] + N0 + N1 + N2 + N3;
    parameters_references_copy[5] =
        &parameters_copy[0] + N0 + N1 + N2 + N3 + N4;

#define COPY_PARAMETER_BLOCK(block) \
    if (N ## block) memcpy(parameters_references_copy[block], \
                           parameters[block], \
                           sizeof(double) * N ## block);  // NOLINT
    COPY_PARAMETER_BLOCK(0);
    COPY_PARAMETER_BLOCK(1);
    COPY_PARAMETER_BLOCK(2);
    COPY_PARAMETER_BLOCK(3);
    COPY_PARAMETER_BLOCK(4);
    COPY_PARAMETER_BLOCK(5);
#undef COPY_PARAMETER_BLOCK

#define EVALUATE_JACOBIAN_FOR_BLOCK(block) \
    if (N ## block && jacobians[block]) { \
      if (!Differencer<CostFunctionNoJacobian, /* NOLINT */ \
                       M, \
                       N ## block, \
                       block, \
                       method>::EvaluateJacobianForParameterBlock( \
          function_.get(), \
          residuals, \
          parameters_references_copy, \
          jacobians)) { \
        return false; \
      } \
    }
    EVALUATE_JACOBIAN_FOR_BLOCK(0);
    EVALUATE_JACOBIAN_FOR_BLOCK(1);
    EVALUATE_JACOBIAN_FOR_BLOCK(2);
    EVALUATE_JACOBIAN_FOR_BLOCK(3);
    EVALUATE_JACOBIAN_FOR_BLOCK(4);
    EVALUATE_JACOBIAN_FOR_BLOCK(5);
#undef EVALUATE_JACOBIAN_FOR_BLOCK
    return true;
  }

 private:
  internal::scoped_ptr<CostFunctionNoJacobian> function_;
  Ownership ownership_;
};

}  // namespace ceres

#endif  // CERES_PUBLIC_NUMERIC_DIFF_COST_FUNCTION_H_
