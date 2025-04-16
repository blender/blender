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
// Author: richie.stebbing@gmail.com (Richard Stebbing)

#include "ceres/dynamic_compressed_row_jacobian_writer.h"

#include <memory>
#include <utility>
#include <vector>

#include "ceres/casts.h"
#include "ceres/compressed_row_jacobian_writer.h"
#include "ceres/dynamic_compressed_row_sparse_matrix.h"
#include "ceres/parameter_block.h"
#include "ceres/program.h"
#include "ceres/residual_block.h"

namespace ceres::internal {

std::unique_ptr<ScratchEvaluatePreparer[]>
DynamicCompressedRowJacobianWriter::CreateEvaluatePreparers(int num_threads) {
  return ScratchEvaluatePreparer::Create(*program_, num_threads);
}

std::unique_ptr<SparseMatrix>
DynamicCompressedRowJacobianWriter::CreateJacobian() const {
  return std::make_unique<DynamicCompressedRowSparseMatrix>(
      program_->NumResiduals(),
      program_->NumEffectiveParameters(),
      0 /* max_num_nonzeros */);
}

void DynamicCompressedRowJacobianWriter::Write(int residual_id,
                                               int residual_offset,
                                               double** jacobians,
                                               SparseMatrix* base_jacobian) {
  auto* jacobian = down_cast<DynamicCompressedRowSparseMatrix*>(base_jacobian);

  // Get the `residual_block` of interest.
  const ResidualBlock* residual_block =
      program_->residual_blocks()[residual_id];
  const int num_residuals = residual_block->NumResiduals();

  std::vector<std::pair<int, int>> evaluated_jacobian_blocks;
  CompressedRowJacobianWriter::GetOrderedParameterBlocks(
      program_, residual_id, &evaluated_jacobian_blocks);

  // `residual_offset` is the residual row in the global jacobian.
  // Empty the jacobian rows.
  jacobian->ClearRows(residual_offset, num_residuals);

  // Iterate over each parameter block.
  for (const auto& evaluated_jacobian_block : evaluated_jacobian_blocks) {
    const ParameterBlock* parameter_block =
        program_->parameter_blocks()[evaluated_jacobian_block.first];
    const int parameter_block_jacobian_index = evaluated_jacobian_block.second;
    const int parameter_block_size = parameter_block->TangentSize();
    const double* parameter_jacobian =
        jacobians[parameter_block_jacobian_index];

    // For each parameter block only insert its non-zero entries.
    for (int r = 0; r < num_residuals; ++r) {
      for (int c = 0; c < parameter_block_size; ++c, ++parameter_jacobian) {
        const double v = *parameter_jacobian;
        // Only insert non-zero entries.
        if (v != 0.0) {
          jacobian->InsertEntry(
              residual_offset + r, parameter_block->delta_offset() + c, v);
        }
      }
    }
  }
}

}  // namespace ceres::internal
