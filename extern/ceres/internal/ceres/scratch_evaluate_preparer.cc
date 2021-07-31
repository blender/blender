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

#include "ceres/scratch_evaluate_preparer.h"

#include "ceres/parameter_block.h"
#include "ceres/program.h"
#include "ceres/residual_block.h"

namespace ceres {
namespace internal {

ScratchEvaluatePreparer* ScratchEvaluatePreparer::Create(
    const Program &program,
    int num_threads) {
  ScratchEvaluatePreparer* preparers = new ScratchEvaluatePreparer[num_threads];
  int max_derivatives_per_residual_block =
      program.MaxDerivativesPerResidualBlock();
  for (int i = 0; i < num_threads; i++) {
    preparers[i].Init(max_derivatives_per_residual_block);
  }
  return preparers;
}

void ScratchEvaluatePreparer::Init(int max_derivatives_per_residual_block) {
  jacobian_scratch_.reset(
      new double[max_derivatives_per_residual_block]);
}

// Point the jacobian blocks into the scratch area of this evaluate preparer.
void ScratchEvaluatePreparer::Prepare(const ResidualBlock* residual_block,
                                      int /* residual_block_index */,
                                      SparseMatrix* /* jacobian */,
                                      double** jacobians) {
  double* jacobian_block_cursor = jacobian_scratch_.get();
  int num_residuals = residual_block->NumResiduals();
  int num_parameter_blocks = residual_block->NumParameterBlocks();
  for (int j = 0; j < num_parameter_blocks; ++j) {
    const ParameterBlock* parameter_block =
        residual_block->parameter_blocks()[j];
    if (parameter_block->IsConstant()) {
      jacobians[j] = NULL;
    } else {
      jacobians[j] = jacobian_block_cursor;
      jacobian_block_cursor += num_residuals * parameter_block->LocalSize();
    }
  }
}

}  // namespace internal
}  // namespace ceres
