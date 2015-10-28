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

#include "ceres/block_evaluate_preparer.h"

#include <vector>
#include "ceres/block_sparse_matrix.h"
#include "ceres/casts.h"
#include "ceres/parameter_block.h"
#include "ceres/residual_block.h"
#include "ceres/sparse_matrix.h"

namespace ceres {
namespace internal {

void BlockEvaluatePreparer::Init(int const* const* jacobian_layout,
                                 int max_derivatives_per_residual_block) {
  jacobian_layout_ = jacobian_layout;
  scratch_evaluate_preparer_.Init(max_derivatives_per_residual_block);
}

// Point the jacobian blocks directly into the block sparse matrix.
void BlockEvaluatePreparer::Prepare(const ResidualBlock* residual_block,
                                    int residual_block_index,
                                    SparseMatrix* jacobian,
                                    double** jacobians) {
  // If the overall jacobian is not available, use the scratch space.
  if (jacobian == NULL) {
    scratch_evaluate_preparer_.Prepare(residual_block,
                                       residual_block_index,
                                       jacobian,
                                       jacobians);
    return;
  }

  double* jacobian_values =
      down_cast<BlockSparseMatrix*>(jacobian)->mutable_values();

  const int* jacobian_block_offset = jacobian_layout_[residual_block_index];
  const int num_parameter_blocks = residual_block->NumParameterBlocks();
  for (int j = 0; j < num_parameter_blocks; ++j) {
    if (!residual_block->parameter_blocks()[j]->IsConstant()) {
      jacobians[j] = jacobian_values + *jacobian_block_offset;

      // The jacobian_block_offset can't be indexed with 'j' since the code
      // that creates the layout strips out any blocks for inactive
      // parameters. Instead, bump the pointer for active parameters only.
      jacobian_block_offset++;
    } else {
      jacobians[j] = NULL;
    }
  }
}

}  // namespace internal
}  // namespace ceres
