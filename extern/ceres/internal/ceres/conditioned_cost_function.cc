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
// Author: wjr@google.com (William Rucklidge)
//
// This file contains the implementation of the conditioned cost function.

#include "ceres/conditioned_cost_function.h"

#include <cstddef>

#include "ceres/internal/eigen.h"
#include "ceres/stl_util.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres {

// This cost function has the same dimensions (parameters, residuals) as
// the one it's wrapping.
ConditionedCostFunction::ConditionedCostFunction(
    CostFunction* wrapped_cost_function,
    const std::vector<CostFunction*>& conditioners,
    Ownership ownership)
    : wrapped_cost_function_(wrapped_cost_function),
      conditioners_(conditioners),
      ownership_(ownership) {
  // Set up our dimensions.
  set_num_residuals(wrapped_cost_function_->num_residuals());
  *mutable_parameter_block_sizes() =
      wrapped_cost_function_->parameter_block_sizes();

  // Sanity-check the conditioners' dimensions.
  CHECK_EQ(wrapped_cost_function_->num_residuals(), conditioners_.size());
  for (int i = 0; i < wrapped_cost_function_->num_residuals(); i++) {
    if (conditioners[i]) {
      CHECK_EQ(1, conditioners[i]->num_residuals());
      CHECK_EQ(1, conditioners[i]->parameter_block_sizes().size());
      CHECK_EQ(1, conditioners[i]->parameter_block_sizes()[0]);
    }
  }
}

ConditionedCostFunction::~ConditionedCostFunction() {
  if (ownership_ == TAKE_OWNERSHIP) {
    STLDeleteUniqueContainerPointers(conditioners_.begin(),
                                     conditioners_.end());
  } else {
    wrapped_cost_function_.release();
  }
}

bool ConditionedCostFunction::Evaluate(double const* const* parameters,
                                       double* residuals,
                                       double** jacobians) const {
  bool success =
      wrapped_cost_function_->Evaluate(parameters, residuals, jacobians);
  if (!success) {
    return false;
  }

  for (int r = 0; r < wrapped_cost_function_->num_residuals(); r++) {
    // On output, we want to have
    // residuals[r] = conditioners[r](wrapped_residuals[r])
    // For parameter block i, column c,
    // jacobians[i][r*parameter_block_size_[i] + c] =
    //   = d residual[r] / d parameters[i][c]
    //   = conditioners[r]'(wrapped_residuals[r]) *
    //       d wrapped_residuals[r] / d parameters[i][c]
    if (conditioners_[r]) {
      double conditioner_derivative;
      double* conditioner_derivative_pointer = &conditioner_derivative;
      double** conditioner_derivative_pointer2 =
          &conditioner_derivative_pointer;
      if (!jacobians) {
        conditioner_derivative_pointer2 = nullptr;
      }

      double unconditioned_residual = residuals[r];
      double* parameter_pointer = &unconditioned_residual;
      success = conditioners_[r]->Evaluate(
          &parameter_pointer, &residuals[r], conditioner_derivative_pointer2);
      if (!success) {
        return false;
      }

      if (jacobians) {
        for (int i = 0;
             i < wrapped_cost_function_->parameter_block_sizes().size();
             i++) {
          if (jacobians[i]) {
            int parameter_block_size =
                wrapped_cost_function_->parameter_block_sizes()[i];
            VectorRef jacobian_row(jacobians[i] + r * parameter_block_size,
                                   parameter_block_size,
                                   1);
            jacobian_row *= conditioner_derivative;
          }
        }
      }
    }
  }
  return true;
}

}  // namespace ceres
