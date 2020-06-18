// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2016 Google Inc. All rights reserved.
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
// Authors: wjr@google.com (William Rucklidge),
//          keir@google.com (Keir Mierle),
//          dgossow@google.com (David Gossow)

#include "ceres/gradient_checker.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <string>
#include <vector>

#include "ceres/is_close.h"
#include "ceres/stringprintf.h"
#include "ceres/types.h"

namespace ceres {

using internal::IsClose;
using internal::StringAppendF;
using internal::StringPrintf;
using std::string;
using std::vector;

namespace {
// Evaluate the cost function and transform the returned Jacobians to
// the local space of the respective local parameterizations.
bool EvaluateCostFunction(
    const ceres::CostFunction* function,
    double const* const * parameters,
    const std::vector<const ceres::LocalParameterization*>&
        local_parameterizations,
    Vector* residuals,
    std::vector<Matrix>* jacobians,
    std::vector<Matrix>* local_jacobians) {
  CHECK(residuals != nullptr);
  CHECK(jacobians != nullptr);
  CHECK(local_jacobians != nullptr);

  const vector<int32_t>& block_sizes = function->parameter_block_sizes();
  const int num_parameter_blocks = block_sizes.size();

  // Allocate Jacobian matrices in local space.
  local_jacobians->resize(num_parameter_blocks);
  vector<double*> local_jacobian_data(num_parameter_blocks);
  for (int i = 0; i < num_parameter_blocks; ++i) {
    int block_size = block_sizes.at(i);
    if (local_parameterizations.at(i) != NULL) {
      block_size = local_parameterizations.at(i)->LocalSize();
    }
    local_jacobians->at(i).resize(function->num_residuals(), block_size);
    local_jacobians->at(i).setZero();
    local_jacobian_data.at(i) = local_jacobians->at(i).data();
  }

  // Allocate Jacobian matrices in global space.
  jacobians->resize(num_parameter_blocks);
  vector<double*> jacobian_data(num_parameter_blocks);
  for (int i = 0; i < num_parameter_blocks; ++i) {
    jacobians->at(i).resize(function->num_residuals(), block_sizes.at(i));
    jacobians->at(i).setZero();
    jacobian_data.at(i) = jacobians->at(i).data();
  }

  // Compute residuals & jacobians.
  CHECK_NE(0, function->num_residuals());
  residuals->resize(function->num_residuals());
  residuals->setZero();
  if (!function->Evaluate(parameters, residuals->data(),
                          jacobian_data.data())) {
    return false;
  }

  // Convert Jacobians from global to local space.
  for (size_t i = 0; i < local_jacobians->size(); ++i) {
    if (local_parameterizations.at(i) == NULL) {
      local_jacobians->at(i) = jacobians->at(i);
    } else {
      int global_size = local_parameterizations.at(i)->GlobalSize();
      int local_size = local_parameterizations.at(i)->LocalSize();
      CHECK_EQ(jacobians->at(i).cols(), global_size);
      Matrix global_J_local(global_size, local_size);
      local_parameterizations.at(i)->ComputeJacobian(
          parameters[i], global_J_local.data());
      local_jacobians->at(i).noalias() = jacobians->at(i) * global_J_local;
    }
  }
  return true;
}
} // namespace

GradientChecker::GradientChecker(
      const CostFunction* function,
      const vector<const LocalParameterization*>* local_parameterizations,
      const NumericDiffOptions& options) :
        function_(function) {
  CHECK(function != nullptr);
  if (local_parameterizations != NULL) {
    local_parameterizations_ = *local_parameterizations;
  } else {
    local_parameterizations_.resize(function->parameter_block_sizes().size(),
                                    NULL);
  }
  DynamicNumericDiffCostFunction<CostFunction, RIDDERS>*
      finite_diff_cost_function =
      new DynamicNumericDiffCostFunction<CostFunction, RIDDERS>(
          function, DO_NOT_TAKE_OWNERSHIP, options);
  finite_diff_cost_function_.reset(finite_diff_cost_function);

  const vector<int32_t>& parameter_block_sizes =
      function->parameter_block_sizes();
  const int num_parameter_blocks = parameter_block_sizes.size();
  for (int i = 0; i < num_parameter_blocks; ++i) {
    finite_diff_cost_function->AddParameterBlock(parameter_block_sizes[i]);
  }
  finite_diff_cost_function->SetNumResiduals(function->num_residuals());
}

bool GradientChecker::Probe(double const* const * parameters,
                            double relative_precision,
                            ProbeResults* results_param) const {
  int num_residuals = function_->num_residuals();

  // Make sure that we have a place to store results, no matter if the user has
  // provided an output argument.
  ProbeResults* results;
  ProbeResults results_local;
  if (results_param != NULL) {
    results = results_param;
    results->residuals.resize(0);
    results->jacobians.clear();
    results->numeric_jacobians.clear();
    results->local_jacobians.clear();
    results->local_numeric_jacobians.clear();
    results->error_log.clear();
  } else {
    results = &results_local;
  }
  results->maximum_relative_error = 0.0;
  results->return_value = true;

  // Evaluate the derivative using the user supplied code.
  vector<Matrix>& jacobians = results->jacobians;
  vector<Matrix>& local_jacobians = results->local_jacobians;
  if (!EvaluateCostFunction(function_, parameters, local_parameterizations_,
                       &results->residuals, &jacobians, &local_jacobians)) {
    results->error_log = "Function evaluation with Jacobians failed.";
    results->return_value = false;
  }

  // Evaluate the derivative using numeric derivatives.
  vector<Matrix>& numeric_jacobians = results->numeric_jacobians;
  vector<Matrix>& local_numeric_jacobians = results->local_numeric_jacobians;
  Vector finite_diff_residuals;
  if (!EvaluateCostFunction(finite_diff_cost_function_.get(), parameters,
                            local_parameterizations_, &finite_diff_residuals,
                            &numeric_jacobians, &local_numeric_jacobians)) {
    results->error_log += "\nFunction evaluation with numerical "
        "differentiation failed.";
    results->return_value = false;
  }

  if (!results->return_value) {
    return false;
  }

  for (int i = 0; i < num_residuals; ++i) {
    if (!IsClose(
        results->residuals[i],
        finite_diff_residuals[i],
        relative_precision,
        NULL,
        NULL)) {
      results->error_log = "Function evaluation with and without Jacobians "
          "resulted in different residuals.";
      LOG(INFO) << results->residuals.transpose();
      LOG(INFO) << finite_diff_residuals.transpose();
      return false;
    }
  }

  // See if any elements have relative error larger than the threshold.
  int num_bad_jacobian_components = 0;
  double& worst_relative_error = results->maximum_relative_error;
  worst_relative_error = 0;

  // Accumulate the error message for all the jacobians, since it won't get
  // output if there are no bad jacobian components.
  string error_log;
  for (int k = 0; k < function_->parameter_block_sizes().size(); k++) {
    StringAppendF(&error_log,
                  "========== "
                  "Jacobian for " "block %d: (%ld by %ld)) "
                  "==========\n",
                  k,
                  static_cast<long>(local_jacobians[k].rows()),
                  static_cast<long>(local_jacobians[k].cols()));
    // The funny spacing creates appropriately aligned column headers.
    error_log +=
        " block  row  col        user dx/dy    num diff dx/dy         "
        "abs error    relative error         parameter          residual\n";

    for (int i = 0; i < local_jacobians[k].rows(); i++) {
      for (int j = 0; j < local_jacobians[k].cols(); j++) {
        double term_jacobian = local_jacobians[k](i, j);
        double finite_jacobian = local_numeric_jacobians[k](i, j);
        double relative_error, absolute_error;
        bool bad_jacobian_entry =
            !IsClose(term_jacobian,
                     finite_jacobian,
                     relative_precision,
                     &relative_error,
                     &absolute_error);
        worst_relative_error = std::max(worst_relative_error, relative_error);

        StringAppendF(&error_log,
                      "%6d %4d %4d %17g %17g %17g %17g %17g %17g",
                      k, i, j,
                      term_jacobian, finite_jacobian,
                      absolute_error, relative_error,
                      parameters[k][j],
                      results->residuals[i]);

        if (bad_jacobian_entry) {
          num_bad_jacobian_components++;
          StringAppendF(
              &error_log,
              " ------ (%d,%d,%d) Relative error worse than %g",
              k, i, j, relative_precision);
        }
        error_log += "\n";
      }
    }
  }

  // Since there were some bad errors, dump comprehensive debug info.
  if (num_bad_jacobian_components) {
    string header = StringPrintf("\nDetected %d bad Jacobian component(s). "
        "Worst relative error was %g.\n",
        num_bad_jacobian_components,
        worst_relative_error);
     results->error_log = header + "\n" + error_log;
    return false;
  }
  return true;
}

}  // namespace ceres
