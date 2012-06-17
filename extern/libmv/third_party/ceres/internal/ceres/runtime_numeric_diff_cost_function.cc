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
// Based on the templated version in public/numeric_diff_cost_function.h.

#include "ceres/runtime_numeric_diff_cost_function.h"

#include <algorithm>
#include <numeric>
#include <vector>

#include <glog/logging.h>
#include "Eigen/Dense"
#include "ceres/cost_function.h"
#include "ceres/internal/scoped_ptr.h"

namespace ceres {
namespace internal {
namespace {

bool EvaluateJacobianForParameterBlock(const CostFunction* function,
                                       int parameter_block_size,
                                       int parameter_block,
                                       RuntimeNumericDiffMethod method,
                                       double relative_step_size,
                                       double const* residuals_at_eval_point,
                                       double** parameters,
                                       double** jacobians) {
  using Eigen::Map;
  using Eigen::Matrix;
  using Eigen::Dynamic;
  using Eigen::RowMajor;

  typedef Matrix<double, Dynamic, 1> ResidualVector;
  typedef Matrix<double, Dynamic, 1> ParameterVector;
  typedef Matrix<double, Dynamic, Dynamic, RowMajor> JacobianMatrix;

  int num_residuals = function->num_residuals();

  Map<JacobianMatrix> parameter_jacobian(jacobians[parameter_block],
                                         num_residuals,
                                         parameter_block_size);

  // Mutate one element at a time and then restore.
  Map<ParameterVector> x_plus_delta(parameters[parameter_block],
                                    parameter_block_size);
  ParameterVector x(x_plus_delta);
  ParameterVector step_size = x.array().abs() * relative_step_size;

  // To handle cases where a paremeter is exactly zero, instead use the mean
  // step_size for the other dimensions.
  double fallback_step_size = step_size.sum() / step_size.rows();
  if (fallback_step_size == 0.0) {
    // If all the parameters are zero, there's no good answer. Use the given
    // relative step_size as absolute step_size and hope for the best.
    fallback_step_size = relative_step_size;
  }

  // For each parameter in the parameter block, use finite differences to
  // compute the derivative for that parameter.
  for (int j = 0; j < parameter_block_size; ++j) {
    if (step_size(j) == 0.0) {
      // The parameter is exactly zero, so compromise and use the mean step_size
      // from the other parameters. This can break in many cases, but it's hard
      // to pick a good number without problem specific knowledge.
      step_size(j) = fallback_step_size;
    }
    x_plus_delta(j) = x(j) + step_size(j);

    ResidualVector residuals(num_residuals);
    if (!function->Evaluate(parameters, &residuals[0], NULL)) {
      // Something went wrong; bail.
      return false;
    }

    // Compute this column of the jacobian in 3 steps:
    // 1. Store residuals for the forward part.
    // 2. Subtract residuals for the backward (or 0) part.
    // 3. Divide out the run.
    parameter_jacobian.col(j) = residuals;

    double one_over_h = 1 / step_size(j);
    if (method == CENTRAL) {
      // Compute the function on the other side of x(j).
      x_plus_delta(j) = x(j) - step_size(j);

      if (!function->Evaluate(parameters, &residuals[0], NULL)) {
        // Something went wrong; bail.
        return false;
      }
      parameter_jacobian.col(j) -= residuals;
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

class RuntimeNumericDiffCostFunction : public CostFunction {
 public:
  RuntimeNumericDiffCostFunction(const CostFunction* function,
                                 RuntimeNumericDiffMethod method,
                                 double relative_step_size)
      : function_(function),
        method_(method),
        relative_step_size_(relative_step_size) {
    *mutable_parameter_block_sizes() = function->parameter_block_sizes();
    set_num_residuals(function->num_residuals());
  }

  virtual ~RuntimeNumericDiffCostFunction() { }

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

    const vector<int16>& block_sizes = function_->parameter_block_sizes();
    CHECK(!block_sizes.empty());

    // Create local space for a copy of the parameters which will get mutated.
    int parameters_size = accumulate(block_sizes.begin(), block_sizes.end(), 0);
    vector<double> parameters_copy(parameters_size);
    vector<double*> parameters_references_copy(block_sizes.size());
    parameters_references_copy[0] = &parameters_copy[0];
    for (int block = 1; block < block_sizes.size(); ++block) {
      parameters_references_copy[block] = parameters_references_copy[block - 1]
          + block_sizes[block - 1];
    }

    // Copy the parameters into the local temp space.
    for (int block = 0; block < block_sizes.size(); ++block) {
      memcpy(parameters_references_copy[block],
             parameters[block],
             block_sizes[block] * sizeof(*parameters[block]));
    }

    for (int block = 0; block < block_sizes.size(); ++block) {
      if (!jacobians[block]) {
        // No jacobian requested for this parameter / residual pair.
        continue;
      }
      if (!EvaluateJacobianForParameterBlock(function_,
                                             block_sizes[block],
                                             block,
                                             method_,
                                             relative_step_size_,
                                             residuals,
                                             &parameters_references_copy[0],
                                             jacobians)) {
        return false;
      }
    }
    return true;
  }

 private:
  const CostFunction* function_;
  RuntimeNumericDiffMethod method_;
  double relative_step_size_;
};

}  // namespace

CostFunction* CreateRuntimeNumericDiffCostFunction(
    const CostFunction* cost_function,
    RuntimeNumericDiffMethod method,
    double relative_step_size) {
  return new RuntimeNumericDiffCostFunction(cost_function,
                                            method,
                                            relative_step_size);
}

}  // namespace internal
}  // namespace ceres
