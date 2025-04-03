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
// Author: sameeragarwal@google.com (Sameer Agarwal)

#include "ceres/block_random_access_sparse_matrix.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "ceres/internal/export.h"
#include "ceres/parallel_vector_ops.h"
#include "ceres/triplet_sparse_matrix.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres::internal {

BlockRandomAccessSparseMatrix::BlockRandomAccessSparseMatrix(
    const std::vector<Block>& blocks,
    const std::set<std::pair<int, int>>& block_pairs,
    ContextImpl* context,
    int num_threads)
    : blocks_(blocks), context_(context), num_threads_(num_threads) {
  CHECK_LE(blocks.size(), std::numeric_limits<std::int32_t>::max());

  const int num_cols = NumScalarEntries(blocks);
  const int num_blocks = blocks.size();

  std::vector<int> num_cells_at_row(num_blocks);
  for (auto& p : block_pairs) {
    ++num_cells_at_row[p.first];
  }
  auto block_structure_ = new CompressedRowBlockStructure;
  block_structure_->cols = blocks;
  block_structure_->rows.resize(num_blocks);
  auto p = block_pairs.begin();
  int num_nonzeros = 0;
  // Pairs of block indices are sorted lexicographically, thus pairs
  // corresponding to a single row-block are stored in segments of index pairs
  // with constant row-block index and increasing column-block index.
  // CompressedRowBlockStructure is created by traversing block_pairs set.
  for (int row_block_id = 0; row_block_id < num_blocks; ++row_block_id) {
    auto& row = block_structure_->rows[row_block_id];
    row.block = blocks[row_block_id];
    row.cells.reserve(num_cells_at_row[row_block_id]);
    const int row_block_size = blocks[row_block_id].size;
    // Process all index pairs corresponding to the current row block. Because
    // index pairs are sorted lexicographically, cells are being appended to the
    // current row-block till the first change in row-block index
    for (; p != block_pairs.end() && row_block_id == p->first; ++p) {
      const int col_block_id = p->second;
      row.cells.emplace_back(col_block_id, num_nonzeros);
      num_nonzeros += row_block_size * blocks[col_block_id].size;
    }
  }
  bsm_ = std::make_unique<BlockSparseMatrix>(block_structure_);
  VLOG(1) << "Matrix Size [" << num_cols << "," << num_cols << "] "
          << num_nonzeros;
  double* values = bsm_->mutable_values();
  for (int row_block_id = 0; row_block_id < num_blocks; ++row_block_id) {
    const auto& cells = block_structure_->rows[row_block_id].cells;
    for (auto& c : cells) {
      const int col_block_id = c.block_id;
      double* const data = values + c.position;
      layout_[IntPairToInt64(row_block_id, col_block_id)] =
          std::make_unique<CellInfo>(data);
    }
  }
}

CellInfo* BlockRandomAccessSparseMatrix::GetCell(int row_block_id,
                                                 int col_block_id,
                                                 int* row,
                                                 int* col,
                                                 int* row_stride,
                                                 int* col_stride) {
  const auto it = layout_.find(IntPairToInt64(row_block_id, col_block_id));
  if (it == layout_.end()) {
    return nullptr;
  }

  // Each cell is stored contiguously as its own little dense matrix.
  *row = 0;
  *col = 0;
  *row_stride = blocks_[row_block_id].size;
  *col_stride = blocks_[col_block_id].size;
  return it->second.get();
}

// Assume that the user does not hold any locks on any cell blocks
// when they are calling SetZero.
void BlockRandomAccessSparseMatrix::SetZero() {
  bsm_->SetZero(context_, num_threads_);
}

void BlockRandomAccessSparseMatrix::SymmetricRightMultiplyAndAccumulate(
    const double* x, double* y) const {
  const auto bs = bsm_->block_structure();
  const auto values = bsm_->values();
  const int num_blocks = blocks_.size();

  for (int row_block_id = 0; row_block_id < num_blocks; ++row_block_id) {
    const auto& row_block = bs->rows[row_block_id];
    const int row_block_size = row_block.block.size;
    const int row_block_pos = row_block.block.position;

    for (auto& c : row_block.cells) {
      const int col_block_id = c.block_id;
      const int col_block_size = blocks_[col_block_id].size;
      const int col_block_pos = blocks_[col_block_id].position;

      MatrixVectorMultiply<Eigen::Dynamic, Eigen::Dynamic, 1>(
          values + c.position,
          row_block_size,
          col_block_size,
          x + col_block_pos,
          y + row_block_pos);
      if (col_block_id == row_block_id) {
        continue;
      }

      // Since the matrix is symmetric, but only the upper triangular
      // part is stored, if the block being accessed is not a diagonal
      // block, then use the same block to do the corresponding lower
      // triangular multiply also
      MatrixTransposeVectorMultiply<Eigen::Dynamic, Eigen::Dynamic, 1>(
          values + c.position,
          row_block_size,
          col_block_size,
          x + row_block_pos,
          y + col_block_pos);
    }
  }
}

}  // namespace ceres::internal
