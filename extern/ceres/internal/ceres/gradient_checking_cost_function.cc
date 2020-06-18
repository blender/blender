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
// Authors: keir@google.com (Keir Mierle),
//          dgossow@google.com (David Gossow)

#include "ceres/gradient_checking_cost_function.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <string>
#include <vector>

#include "ceres/gradient_checker.h"
#include "ceres/internal/eigen.h"
#include "ceres/parameter_block.h"
#include "ceres/problem.h"
#include "ceres/problem_impl.h"
#include "ceres/program.h"
#include "ceres/residual_block.h"
#include "ceres/dynamic_numeric_diff_cost_function.h"
#include "ceres/stringprintf.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

using std::abs;
using std::max;
using std::string;
using std::vector;

namespace {

class GradientCheckingCostFunction : public CostFunction {
 public:
  GradientCheckingCostFunction(
      const CostFunction* function,
      const std::vector<const LocalParameterization*>* local_parameterizations,
      const NumericDiffOptions& options,
      double relative_precision,
      const string& extra_info,
      GradientCheckingIterationCallback* callback)
      : function_(function),
        gradient_checker_(function, local_parameterizations, options),
        relative_precision_(relative_precision),
        extra_info_(extra_info),
        callback_(callback) {
    CHECK(callback_ != nullptr);
    const vector<int32_t>& parameter_block_sizes =
        function->parameter_block_sizes();
    *mutable_parameter_block_sizes() = parameter_block_sizes;
    set_num_residuals(function->num_residuals());
  }

  virtual ~GradientCheckingCostFunction() { }

  bool Evaluate(double const* const* parameters,
                double* residuals,
                double** jacobians) const final {
    if (!jacobians) {
      // Nothing to check in this case; just forward.
      return function_->Evaluate(parameters, residuals, NULL);
    }

    GradientChecker::ProbeResults results;
    bool okay = gradient_checker_.Probe(parameters,
                                        relative_precision_,
                                        &results);

    // If the cost function returned false, there's nothing we can say about
    // the gradients.
    if (results.return_value == false) {
      return false;
    }

    // Copy the residuals.
    const int num_residuals = function_->num_residuals();
    MatrixRef(residuals, num_residuals, 1) = results.residuals;

    // Copy the original jacobian blocks into the jacobians array.
    const vector<int32_t>& block_sizes = function_->parameter_block_sizes();
    for (int k = 0; k < block_sizes.size(); k++) {
      if (jacobians[k] != NULL) {
        MatrixRef(jacobians[k],
                  results.jacobians[k].rows(),
                  results.jacobians[k].cols()) = results.jacobians[k];
      }
    }

    if (!okay) {
      std::string error_log = "Gradient Error detected!\nExtra info for "
          "this residual: " + extra_info_ + "\n" + results.error_log;
      callback_->SetGradientErrorDetected(error_log);
    }
    return true;
  }

 private:
  const CostFunction* function_;
  GradientChecker gradient_checker_;
  double relative_precision_;
  string extra_info_;
  GradientCheckingIterationCallback* callback_;
};

}  // namespace

GradientCheckingIterationCallback::GradientCheckingIterationCallback()
    : gradient_error_detected_(false) {
}

CallbackReturnType GradientCheckingIterationCallback::operator()(
    const IterationSummary& summary) {
  if (gradient_error_detected_) {
    LOG(ERROR)<< "Gradient error detected. Terminating solver.";
    return SOLVER_ABORT;
  }
  return SOLVER_CONTINUE;
}
void GradientCheckingIterationCallback::SetGradientErrorDetected(
    std::string& error_log) {
  std::lock_guard<std::mutex> l(mutex_);
  gradient_error_detected_ = true;
  error_log_ += "\n" + error_log;
}

CostFunction* CreateGradientCheckingCostFunction(
    const CostFunction* cost_function,
    const std::vector<const LocalParameterization*>* local_parameterizations,
    double relative_step_size,
    double relative_precision,
    const std::string& extra_info,
    GradientCheckingIterationCallback* callback) {
  NumericDiffOptions numeric_diff_options;
  numeric_diff_options.relative_step_size = relative_step_size;

  return new GradientCheckingCostFunction(cost_function,
                                          local_parameterizations,
                                          numeric_diff_options,
                                          relative_precision, extra_info,
                                          callback);
}

ProblemImpl* CreateGradientCheckingProblemImpl(
    ProblemImpl* problem_impl,
    double relative_step_size,
    double relative_precision,
    GradientCheckingIterationCallback* callback) {
  CHECK(callback != nullptr);
  // We create new CostFunctions by wrapping the original CostFunction
  // in a gradient checking CostFunction. So its okay for the
  // ProblemImpl to take ownership of it and destroy it. The
  // LossFunctions and LocalParameterizations are reused and since
  // they are owned by problem_impl, gradient_checking_problem_impl
  // should not take ownership of it.
  Problem::Options gradient_checking_problem_options;
  gradient_checking_problem_options.cost_function_ownership = TAKE_OWNERSHIP;
  gradient_checking_problem_options.loss_function_ownership =
      DO_NOT_TAKE_OWNERSHIP;
  gradient_checking_problem_options.local_parameterization_ownership =
      DO_NOT_TAKE_OWNERSHIP;
  gradient_checking_problem_options.context = problem_impl->context();

  NumericDiffOptions numeric_diff_options;
  numeric_diff_options.relative_step_size = relative_step_size;

  ProblemImpl* gradient_checking_problem_impl = new ProblemImpl(
      gradient_checking_problem_options);

  Program* program = problem_impl->mutable_program();

  // For every ParameterBlock in problem_impl, create a new parameter
  // block with the same local parameterization and constancy.
  const vector<ParameterBlock*>& parameter_blocks = program->parameter_blocks();
  for (int i = 0; i < parameter_blocks.size(); ++i) {
    ParameterBlock* parameter_block = parameter_blocks[i];
    gradient_checking_problem_impl->AddParameterBlock(
        parameter_block->mutable_user_state(),
        parameter_block->Size(),
        parameter_block->mutable_local_parameterization());

    if (parameter_block->IsConstant()) {
      gradient_checking_problem_impl->SetParameterBlockConstant(
          parameter_block->mutable_user_state());
    }

    for (int i = 0; i <  parameter_block->Size(); ++i) {
      gradient_checking_problem_impl->SetParameterUpperBound(
          parameter_block->mutable_user_state(),
          i,
          parameter_block->UpperBound(i));
      gradient_checking_problem_impl->SetParameterLowerBound(
          parameter_block->mutable_user_state(),
          i,
          parameter_block->LowerBound(i));
    }
  }

  // For every ResidualBlock in problem_impl, create a new
  // ResidualBlock by wrapping its CostFunction inside a
  // GradientCheckingCostFunction.
  const vector<ResidualBlock*>& residual_blocks = program->residual_blocks();
  for (int i = 0; i < residual_blocks.size(); ++i) {
    ResidualBlock* residual_block = residual_blocks[i];

    // Build a human readable string which identifies the
    // ResidualBlock. This is used by the GradientCheckingCostFunction
    // when logging debugging information.
    string extra_info = StringPrintf(
        "Residual block id %d; depends on parameters [", i);
    vector<double*> parameter_blocks;
    vector<const LocalParameterization*> local_parameterizations;
    parameter_blocks.reserve(residual_block->NumParameterBlocks());
    local_parameterizations.reserve(residual_block->NumParameterBlocks());
    for (int j = 0; j < residual_block->NumParameterBlocks(); ++j) {
      ParameterBlock* parameter_block = residual_block->parameter_blocks()[j];
      parameter_blocks.push_back(parameter_block->mutable_user_state());
      StringAppendF(&extra_info, "%p", parameter_block->mutable_user_state());
      extra_info += (j < residual_block->NumParameterBlocks() - 1) ? ", " : "]";
      local_parameterizations.push_back(problem_impl->GetParameterization(
          parameter_block->mutable_user_state()));
    }

    // Wrap the original CostFunction in a GradientCheckingCostFunction.
    CostFunction* gradient_checking_cost_function =
        new GradientCheckingCostFunction(residual_block->cost_function(),
                                         &local_parameterizations,
                                         numeric_diff_options,
                                         relative_precision,
                                         extra_info,
                                         callback);

    // The const_cast is necessary because
    // ProblemImpl::AddResidualBlock can potentially take ownership of
    // the LossFunction, but in this case we are guaranteed that this
    // will not be the case, so this const_cast is harmless.
    gradient_checking_problem_impl->AddResidualBlock(
        gradient_checking_cost_function,
        const_cast<LossFunction*>(residual_block->loss_function()),
        parameter_blocks.data(),
        static_cast<int>(parameter_blocks.size()));
  }

  // Normally, when a problem is given to the solver, we guarantee
  // that the state pointers for each parameter block point to the
  // user provided data. Since we are creating this new problem from a
  // problem given to us at an arbitrary stage of the solve, we cannot
  // depend on this being the case, so we explicitly call
  // SetParameterBlockStatePtrsToUserStatePtrs to ensure that this is
  // the case.
  gradient_checking_problem_impl
      ->mutable_program()
      ->SetParameterBlockStatePtrsToUserStatePtrs();

  return gradient_checking_problem_impl;
}


}  // namespace internal
}  // namespace ceres
