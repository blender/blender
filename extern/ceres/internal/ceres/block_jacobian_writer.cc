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
// Author: keir@google.com (Keir Mierle)

#include "ceres/block_jacobian_writer.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "ceres/block_evaluate_preparer.h"
#include "ceres/block_sparse_matrix.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/export.h"
#include "ceres/parameter_block.h"
#include "ceres/program.h"
#include "ceres/residual_block.h"

namespace ceres::internal {

namespace {

// Given the residual block ordering, build a lookup table to determine which
// per-parameter jacobian goes where in the overall program jacobian.
//
// Since we expect to use a Schur type linear solver to solve the LM step, take
// extra care to place the E blocks and the F blocks contiguously. E blocks are
// the first num_eliminate_blocks parameter blocks as indicated by the parameter
// block ordering. The remaining parameter blocks are the F blocks.
//
// In order to simplify handling block-sparse to CRS conversion, cells within
// the row-block of non-partitioned matrix are stored in memory sequentially in
// the order of increasing column-block id. In case of partitioned matrices,
// cells corresponding to F sub-matrix are stored sequentially in the order of
// increasing column-block id (with cells corresponding to E sub-matrix stored
// separately).
//
// TODO(keir): Consider if we should use a boolean for each parameter block
// instead of num_eliminate_blocks.
bool BuildJacobianLayout(const Program& program,
                         int num_eliminate_blocks,
                         std::vector<int*>* jacobian_layout,
                         std::vector<int>* jacobian_layout_storage) {
  const std::vector<ResidualBlock*>& residual_blocks =
      program.residual_blocks();

  // Iterate over all the active residual blocks and determine how many E blocks
  // are there. This will determine where the F blocks start in the jacobian
  // matrix. Also compute the number of jacobian blocks.
  unsigned int f_block_pos = 0;
  unsigned int num_jacobian_blocks = 0;
  for (auto* residual_block : residual_blocks) {
    const int num_residuals = residual_block->NumResiduals();
    const int num_parameter_blocks = residual_block->NumParameterBlocks();

    // Advance f_block_pos over each E block for this residual.
    for (int j = 0; j < num_parameter_blocks; ++j) {
      ParameterBlock* parameter_block = residual_block->parameter_blocks()[j];
      if (!parameter_block->IsConstant()) {
        // Only count blocks for active parameters.
        num_jacobian_blocks++;
        if (parameter_block->index() < num_eliminate_blocks) {
          f_block_pos += num_residuals * parameter_block->TangentSize();
        }
      }
    }
    if (num_jacobian_blocks > std::numeric_limits<int>::max()) {
      LOG(ERROR) << "Overlow error. Too many blocks in the jacobian matrix : "
                 << num_jacobian_blocks;
      return false;
    }
  }

  // We now know that the E blocks are laid out starting at zero, and the F
  // blocks are laid out starting at f_block_pos. Iterate over the residual
  // blocks again, and this time fill the jacobian_layout array with the
  // position information.

  jacobian_layout->resize(program.NumResidualBlocks());
  jacobian_layout_storage->resize(num_jacobian_blocks);

  int e_block_pos = 0;
  int* jacobian_pos = jacobian_layout_storage->data();
  std::vector<std::pair<int, int>> active_parameter_blocks;
  for (int i = 0; i < residual_blocks.size(); ++i) {
    const ResidualBlock* residual_block = residual_blocks[i];
    const int num_residuals = residual_block->NumResiduals();
    const int num_parameter_blocks = residual_block->NumParameterBlocks();

    (*jacobian_layout)[i] = jacobian_pos;
    // Cells from F sub-matrix are to be stored sequentially with increasing
    // column block id. For each non-constant parameter block, a pair of indices
    // (index in the list of active parameter blocks and index in the list of
    // all parameter blocks) is computed, and index pairs are sorted by the
    // index of corresponding column block id.
    active_parameter_blocks.clear();
    active_parameter_blocks.reserve(num_parameter_blocks);
    for (int j = 0; j < num_parameter_blocks; ++j) {
      ParameterBlock* parameter_block = residual_block->parameter_blocks()[j];
      if (parameter_block->IsConstant()) {
        continue;
      }
      const int k = active_parameter_blocks.size();
      active_parameter_blocks.emplace_back(k, j);
    }
    std::sort(active_parameter_blocks.begin(),
              active_parameter_blocks.end(),
              [&residual_block](const std::pair<int, int>& a,
                                const std::pair<int, int>& b) {
                return residual_block->parameter_blocks()[a.second]->index() <
                       residual_block->parameter_blocks()[b.second]->index();
              });
    // Cell positions for each active parameter block are filled in the order of
    // active parameter block indices sorted by columnd block index. This
    // guarantees that cells are laid out sequentially with increasing column
    // block indices.
    for (const auto& indices : active_parameter_blocks) {
      const auto [k, j] = indices;
      ParameterBlock* parameter_block = residual_block->parameter_blocks()[j];
      const int parameter_block_index = parameter_block->index();
      const int jacobian_block_size =
          num_residuals * parameter_block->TangentSize();
      if (parameter_block_index < num_eliminate_blocks) {
        jacobian_pos[k] = e_block_pos;
        e_block_pos += jacobian_block_size;
      } else {
        jacobian_pos[k] = static_cast<int>(f_block_pos);
        f_block_pos += jacobian_block_size;
        if (f_block_pos > std::numeric_limits<int>::max()) {
          LOG(ERROR)
              << "Overlow error. Too many entries in the Jacobian matrix.";
          return false;
        }
      }
    }
    jacobian_pos += active_parameter_blocks.size();
  }
  return true;
}

}  // namespace

BlockJacobianWriter::BlockJacobianWriter(const Evaluator::Options& options,
                                         Program* program)
    : options_(options), program_(program) {
  CHECK_GE(options.num_eliminate_blocks, 0)
      << "num_eliminate_blocks must be greater than 0.";

  jacobian_layout_is_valid_ = BuildJacobianLayout(*program,
                                                  options.num_eliminate_blocks,
                                                  &jacobian_layout_,
                                                  &jacobian_layout_storage_);
}

// Create evaluate prepareres that point directly into the final jacobian. This
// makes the final Write() a nop.
std::unique_ptr<BlockEvaluatePreparer[]>
BlockJacobianWriter::CreateEvaluatePreparers(unsigned num_threads) {
  const int max_derivatives_per_residual_block =
      program_->MaxDerivativesPerResidualBlock();

  auto preparers = std::make_unique<BlockEvaluatePreparer[]>(num_threads);
  for (unsigned i = 0; i < num_threads; i++) {
    preparers[i].Init(jacobian_layout_.data(),
                      max_derivatives_per_residual_block);
  }
  return preparers;
}

std::unique_ptr<SparseMatrix> BlockJacobianWriter::CreateJacobian() const {
  if (!jacobian_layout_is_valid_) {
    LOG(ERROR) << "Unable to create Jacobian matrix. Too many entries in the "
                  "Jacobian matrix.";
    return nullptr;
  }

  auto* bs = new CompressedRowBlockStructure;

  const std::vector<ParameterBlock*>& parameter_blocks =
      program_->parameter_blocks();

  // Construct the column blocks.
  bs->cols.resize(parameter_blocks.size());
  for (int i = 0, cursor = 0; i < parameter_blocks.size(); ++i) {
    CHECK_NE(parameter_blocks[i]->index(), -1);
    CHECK(!parameter_blocks[i]->IsConstant());
    bs->cols[i].size = parameter_blocks[i]->TangentSize();
    bs->cols[i].position = cursor;
    cursor += bs->cols[i].size;
  }

  // Construct the cells in each row.
  const std::vector<ResidualBlock*>& residual_blocks =
      program_->residual_blocks();
  int row_block_position = 0;
  bs->rows.resize(residual_blocks.size());
  for (int i = 0; i < residual_blocks.size(); ++i) {
    const ResidualBlock* residual_block = residual_blocks[i];
    CompressedRow* row = &bs->rows[i];

    row->block.size = residual_block->NumResiduals();
    row->block.position = row_block_position;
    row_block_position += row->block.size;

    // Size the row by the number of active parameters in this residual.
    const int num_parameter_blocks = residual_block->NumParameterBlocks();
    int num_active_parameter_blocks = 0;
    for (int j = 0; j < num_parameter_blocks; ++j) {
      if (residual_block->parameter_blocks()[j]->index() != -1) {
        num_active_parameter_blocks++;
      }
    }
    row->cells.resize(num_active_parameter_blocks);

    // Add layout information for the active parameters in this row.
    for (int j = 0, k = 0; j < num_parameter_blocks; ++j) {
      const ParameterBlock* parameter_block =
          residual_block->parameter_blocks()[j];
      if (!parameter_block->IsConstant()) {
        Cell& cell = row->cells[k];
        cell.block_id = parameter_block->index();
        cell.position = jacobian_layout_[i][k];

        // Only increment k for active parameters, since there is only layout
        // information for active parameters.
        k++;
      }
    }

    std::sort(row->cells.begin(), row->cells.end(), CellLessThan);
  }

  return std::make_unique<BlockSparseMatrix>(
      bs, options_.sparse_linear_algebra_library_type == CUDA_SPARSE);
}

}  // namespace ceres::internal
