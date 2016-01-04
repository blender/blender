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

#include "ceres/gradient_checking_cost_function.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <string>
#include <vector>

#include "ceres/cost_function.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/scoped_ptr.h"
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

// True if x and y have an absolute relative difference less than
// relative_precision and false otherwise. Stores the relative and absolute
// difference in relative/absolute_error if non-NULL.
bool IsClose(double x, double y, double relative_precision,
             double *relative_error,
             double *absolute_error) {
  double local_absolute_error;
  double local_relative_error;
  if (!absolute_error) {
    absolute_error = &local_absolute_error;
  }
  if (!relative_error) {
    relative_error = &local_relative_error;
  }
  *absolute_error = abs(x - y);
  *relative_error = *absolute_error / max(abs(x), abs(y));
  if (x == 0 || y == 0) {
    // If x or y is exactly zero, then relative difference doesn't have any
    // meaning. Take the absolute difference instead.
    *relative_error = *absolute_error;
  }
  return abs(*relative_error) < abs(relative_precision);
}

class GradientCheckingCostFunction : public CostFunction {
 public:
  GradientCheckingCostFunction(const CostFunction* function,
                               const NumericDiffOptions& options,
                               double relative_precision,
                               const string& extra_info)
      : function_(function),
        relative_precision_(relative_precision),
        extra_info_(extra_info) {
    DynamicNumericDiffCostFunction<CostFunction, CENTRAL>*
        finite_diff_cost_function =
        new DynamicNumericDiffCostFunction<CostFunction, CENTRAL>(
            function,
            DO_NOT_TAKE_OWNERSHIP,
            options);

    const vector<int32>& parameter_block_sizes =
        function->parameter_block_sizes();
    for (int i = 0; i < parameter_block_sizes.size(); ++i) {
      finite_diff_cost_function->AddParameterBlock(parameter_block_sizes[i]);
    }
    *mutable_parameter_block_sizes() = parameter_block_sizes;
    set_num_residuals(function->num_residuals());
    finite_diff_cost_function->SetNumResiduals(num_residuals());
    finite_diff_cost_function_.reset(finite_diff_cost_function);
  }

  virtual ~GradientCheckingCostFunction() { }

  virtual bool Evaluate(double const* const* parameters,
                        double* residuals,
                        double** jacobians) const {
    if (!jacobians) {
      // Nothing to check in this case; just forward.
      return function_->Evaluate(parameters, residuals, NULL);
    }

    int num_residuals = function_->num_residuals();

    // Make space for the jacobians of the two methods.
    const vector<int32>& block_sizes = function_->parameter_block_sizes();
    vector<Matrix> term_jacobians(block_sizes.size());
    vector<Matrix> finite_difference_jacobians(block_sizes.size());
    vector<double*> term_jacobian_pointers(block_sizes.size());
    vector<double*> finite_difference_jacobian_pointers(block_sizes.size());
    for (int i = 0; i < block_sizes.size(); i++) {
      term_jacobians[i].resize(num_residuals, block_sizes[i]);
      term_jacobian_pointers[i] = term_jacobians[i].data();
      finite_difference_jacobians[i].resize(num_residuals, block_sizes[i]);
      finite_difference_jacobian_pointers[i] =
          finite_difference_jacobians[i].data();
    }

    // Evaluate the derivative using the user supplied code.
    if (!function_->Evaluate(parameters,
                             residuals,
                             &term_jacobian_pointers[0])) {
      LOG(WARNING) << "Function evaluation failed.";
      return false;
    }

    // Evaluate the derivative using numeric derivatives.
    finite_diff_cost_function_->Evaluate(
        parameters,
        residuals,
        &finite_difference_jacobian_pointers[0]);

    // See if any elements have relative error larger than the threshold.
    int num_bad_jacobian_components = 0;
    double worst_relative_error = 0;

    // Accumulate the error message for all the jacobians, since it won't get
    // output if there are no bad jacobian components.
    string m;
    for (int k = 0; k < block_sizes.size(); k++) {
      // Copy the original jacobian blocks into the jacobians array.
      if (jacobians[k] != NULL) {
        MatrixRef(jacobians[k],
                  term_jacobians[k].rows(),
                  term_jacobians[k].cols()) = term_jacobians[k];
      }

      StringAppendF(&m,
                    "========== "
                    "Jacobian for " "block %d: (%ld by %ld)) "
                    "==========\n",
                    k,
                    static_cast<long>(term_jacobians[k].rows()),
                    static_cast<long>(term_jacobians[k].cols()));
      // The funny spacing creates appropriately aligned column headers.
      m += " block  row  col        user dx/dy    num diff dx/dy         "
           "abs error    relative error         parameter          residual\n";

      for (int i = 0; i < term_jacobians[k].rows(); i++) {
        for (int j = 0; j < term_jacobians[k].cols(); j++) {
          double term_jacobian = term_jacobians[k](i, j);
          double finite_jacobian = finite_difference_jacobians[k](i, j);
          double relative_error, absolute_error;
          bool bad_jacobian_entry =
              !IsClose(term_jacobian,
                       finite_jacobian,
                       relative_precision_,
                       &relative_error,
                       &absolute_error);
          worst_relative_error = max(worst_relative_error, relative_error);

          StringAppendF(&m, "%6d %4d %4d %17g %17g %17g %17g %17g %17g",
                        k, i, j,
                        term_jacobian, finite_jacobian,
                        absolute_error, relative_error,
                        parameters[k][j],
                        residuals[i]);

          if (bad_jacobian_entry) {
            num_bad_jacobian_components++;
            StringAppendF(
                &m, " ------ (%d,%d,%d) Relative error worse than %g",
                k, i, j, relative_precision_);
          }
          m += "\n";
        }
      }
    }

    // Since there were some bad errors, dump comprehensive debug info.
    if (num_bad_jacobian_components) {
      string header = StringPrintf("Detected %d bad jacobian component(s). "
                                   "Worst relative error was %g.\n",
                                   num_bad_jacobian_components,
                                   worst_relative_error);
      if (!extra_info_.empty()) {
        header += "Extra info for this residual: " + extra_info_ + "\n";
      }
      LOG(WARNING) << "\n" << header << m;
    }
    return true;
  }

 private:
  const CostFunction* function_;
  internal::scoped_ptr<CostFunction> finite_diff_cost_function_;
  double relative_precision_;
  string extra_info_;
};

}  // namespace

CostFunction *CreateGradientCheckingCostFunction(
    const CostFunction *cost_function,
    double relative_step_size,
    double relative_precision,
    const string& extra_info) {
  NumericDiffOptions numeric_diff_options;
  numeric_diff_options.relative_step_size = relative_step_size;

  return new GradientCheckingCostFunction(cost_function,
                                          numeric_diff_options,
                                          relative_precision,
                                          extra_info);
}

ProblemImpl* CreateGradientCheckingProblemImpl(ProblemImpl* problem_impl,
                                               double relative_step_size,
                                               double relative_precision) {
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
    for (int j = 0; j < residual_block->NumParameterBlocks(); ++j) {
      ParameterBlock* parameter_block = residual_block->parameter_blocks()[j];
      parameter_blocks.push_back(parameter_block->mutable_user_state());
      StringAppendF(&extra_info, "%p", parameter_block->mutable_user_state());
      extra_info += (j < residual_block->NumParameterBlocks() - 1) ? ", " : "]";
    }

    // Wrap the original CostFunction in a GradientCheckingCostFunction.
    CostFunction* gradient_checking_cost_function =
        CreateGradientCheckingCostFunction(residual_block->cost_function(),
                                           relative_step_size,
                                           relative_precision,
                                           extra_info);

    // The const_cast is necessary because
    // ProblemImpl::AddResidualBlock can potentially take ownership of
    // the LossFunction, but in this case we are guaranteed that this
    // will not be the case, so this const_cast is harmless.
    gradient_checking_problem_impl->AddResidualBlock(
        gradient_checking_cost_function,
        const_cast<LossFunction*>(residual_block->loss_function()),
        parameter_blocks);
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
