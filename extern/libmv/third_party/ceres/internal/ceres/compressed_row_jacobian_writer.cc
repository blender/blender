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

#include "ceres/compressed_row_jacobian_writer.h"

#include "ceres/casts.h"
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/parameter_block.h"
#include "ceres/program.h"
#include "ceres/residual_block.h"
#include "ceres/scratch_evaluate_preparer.h"

namespace ceres {
namespace internal {

SparseMatrix* CompressedRowJacobianWriter::CreateJacobian() const {
  const vector<ResidualBlock*>& residual_blocks =
      program_->residual_blocks();

  int total_num_residuals = program_->NumResiduals();
  int total_num_effective_parameters = program_->NumEffectiveParameters();

  // Count the number of jacobian nonzeros.
  int num_jacobian_nonzeros = 0;
  for (int i = 0; i < residual_blocks.size(); ++i) {
    ResidualBlock* residual_block = residual_blocks[i];
    const int num_residuals = residual_block->NumResiduals();
    const int num_parameter_blocks = residual_block->NumParameterBlocks();
    for (int j = 0; j < num_parameter_blocks; ++j) {
      ParameterBlock* parameter_block = residual_block->parameter_blocks()[j];
      if (!parameter_block->IsConstant()) {
        num_jacobian_nonzeros += num_residuals * parameter_block->LocalSize();
      }
    }
  }

  // Allocate storage for the jacobian with some extra space at the end.
  // Allocate more space than needed to store the jacobian so that when the LM
  // algorithm adds the diagonal, no reallocation is necessary. This reduces
  // peak memory usage significantly.
  CompressedRowSparseMatrix* jacobian =
      new CompressedRowSparseMatrix(
          total_num_residuals,
          total_num_effective_parameters,
          num_jacobian_nonzeros + total_num_effective_parameters);

  // At this stage, the CompressedSparseMatrix is an invalid state. But this
  // seems to be the only way to construct it without doing a memory copy.
  int* rows = jacobian->mutable_rows();
  int* cols = jacobian->mutable_cols();
  int row_pos = 0;
  rows[0] = 0;
  for (int i = 0; i < residual_blocks.size(); ++i) {
    const ResidualBlock* residual_block = residual_blocks[i];
    const int num_parameter_blocks = residual_block->NumParameterBlocks();

    // Count the number of derivatives for a row of this residual block and
    // build a list of active parameter block indices.
    int num_derivatives = 0;
    vector<int> parameter_indices;
    for (int j = 0; j < num_parameter_blocks; ++j) {
      ParameterBlock* parameter_block = residual_block->parameter_blocks()[j];
      if (!parameter_block->IsConstant()) {
        parameter_indices.push_back(parameter_block->index());
        num_derivatives += parameter_block->LocalSize();
      }
    }

    // Sort the parameters by their position in the state vector.
    sort(parameter_indices.begin(), parameter_indices.end());
    CHECK(unique(parameter_indices.begin(), parameter_indices.end()) ==
          parameter_indices.end())
          << "Ceres internal error:  "
          << "Duplicate parameter blocks detected in a cost function. "
          << "This should never happen. Please report this to "
          << "the Ceres developers.";

    // Update the row indices.
    const int num_residuals = residual_block->NumResiduals();
    for (int j = 0; j < num_residuals; ++j) {
      rows[row_pos + j + 1] = rows[row_pos + j] + num_derivatives;
    }

    // Iterate over parameter blocks in the order which they occur in the
    // parameter vector. This code mirrors that in Write(), where jacobian
    // values are updated.
    int col_pos = 0;
    for (int j = 0; j < parameter_indices.size(); ++j) {
      ParameterBlock* parameter_block =
          program_->parameter_blocks()[parameter_indices[j]];
      const int parameter_block_size = parameter_block->LocalSize();

      for (int r = 0; r < num_residuals; ++r) {
        // This is the position in the values array of the jacobian where this
        // row of the jacobian block should go.
        const int column_block_begin = rows[row_pos + r] + col_pos;

        for (int c = 0; c < parameter_block_size; ++c) {
          cols[column_block_begin + c] = parameter_block->delta_offset() + c;
        }
      }
      col_pos += parameter_block_size;
    }
    row_pos += num_residuals;
  }

  CHECK_EQ(num_jacobian_nonzeros, rows[total_num_residuals]);
  return jacobian;
}

void CompressedRowJacobianWriter::Write(int residual_id,
                                        int residual_offset,
                                        double **jacobians,
                                        SparseMatrix* base_jacobian) {
  CompressedRowSparseMatrix* jacobian =
      down_cast<CompressedRowSparseMatrix*>(base_jacobian);

  double* jacobian_values = jacobian->mutable_values();
  const int* jacobian_rows = jacobian->rows();

  const ResidualBlock* residual_block =
      program_->residual_blocks()[residual_id];
  const int num_parameter_blocks = residual_block->NumParameterBlocks();
  const int num_residuals = residual_block->NumResiduals();

  // It is necessary to determine the order of the jacobian blocks before
  // copying them into the CompressedRowSparseMatrix. Just because a cost
  // function uses parameter blocks 1 after 2 in its arguments does not mean
  // that the block 1 occurs before block 2 in the column layout of the
  // jacobian. Thus, determine the order by sorting the jacobian blocks by their
  // position in the state vector.
  vector<pair<int, int> > evaluated_jacobian_blocks;
  for (int j = 0; j < num_parameter_blocks; ++j) {
    const ParameterBlock* parameter_block =
        residual_block->parameter_blocks()[j];
    if (!parameter_block->IsConstant()) {
      evaluated_jacobian_blocks.push_back(
          make_pair(parameter_block->index(), j));
    }
  }
  sort(evaluated_jacobian_blocks.begin(), evaluated_jacobian_blocks.end());

  // Where in the current row does the jacobian for a parameter block begin.
  int col_pos = 0;

  // Iterate over the jacobian blocks in increasing order of their
  // positions in the reduced parameter vector.
  for (int i = 0; i < evaluated_jacobian_blocks.size(); ++i) {
    const ParameterBlock* parameter_block =
        program_->parameter_blocks()[evaluated_jacobian_blocks[i].first];
    const int argument = evaluated_jacobian_blocks[i].second;
    const int parameter_block_size = parameter_block->LocalSize();

    // Copy one row of the jacobian block at a time.
    for (int r = 0; r < num_residuals; ++r) {
      // Position of the r^th row of the current jacobian block.
      const double* block_row_begin =
          jacobians[argument] + r * parameter_block_size;

      // Position in the values array of the jacobian where this
      // row of the jacobian block should go.
      double* column_block_begin =
          jacobian_values + jacobian_rows[residual_offset + r] + col_pos;

      copy(block_row_begin,
           block_row_begin + parameter_block_size,
           column_block_begin);
    }
    col_pos += parameter_block_size;
  }
}

}  // namespace internal
}  // namespace ceres
