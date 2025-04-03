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

#include "ceres/block_random_access_diagonal_matrix.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "Eigen/Dense"
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/internal/export.h"
#include "ceres/parallel_for.h"
#include "ceres/parallel_vector_ops.h"
#include "ceres/stl_util.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres::internal {

BlockRandomAccessDiagonalMatrix::BlockRandomAccessDiagonalMatrix(
    const std::vector<Block>& blocks, ContextImpl* context, int num_threads)
    : context_(context), num_threads_(num_threads) {
  m_ = CompressedRowSparseMatrix::CreateBlockDiagonalMatrix(nullptr, blocks);
  double* values = m_->mutable_values();
  layout_.reserve(blocks.size());
  for (auto& block : blocks) {
    layout_.emplace_back(std::make_unique<CellInfo>(values));
    values += block.size * block.size;
  }
}

CellInfo* BlockRandomAccessDiagonalMatrix::GetCell(int row_block_id,
                                                   int col_block_id,
                                                   int* row,
                                                   int* col,
                                                   int* row_stride,
                                                   int* col_stride) {
  if (row_block_id != col_block_id) {
    return nullptr;
  }

  auto& blocks = m_->row_blocks();
  const int stride = blocks[row_block_id].size;

  // Each cell is stored contiguously as its own little dense matrix.
  *row = 0;
  *col = 0;
  *row_stride = stride;
  *col_stride = stride;
  return layout_[row_block_id].get();
}

// Assume that the user does not hold any locks on any cell blocks
// when they are calling SetZero.
void BlockRandomAccessDiagonalMatrix::SetZero() {
  ParallelSetZero(
      context_, num_threads_, m_->mutable_values(), m_->num_nonzeros());
}

void BlockRandomAccessDiagonalMatrix::Invert() {
  auto& blocks = m_->row_blocks();
  const int num_blocks = blocks.size();
  ParallelFor(context_, 0, num_blocks, num_threads_, [this, blocks](int i) {
    auto* cell_info = layout_[i].get();
    auto& block = blocks[i];
    MatrixRef b(cell_info->values, block.size, block.size);
    b = b.selfadjointView<Eigen::Upper>().llt().solve(
        Matrix::Identity(block.size, block.size));
  });
}

void BlockRandomAccessDiagonalMatrix::RightMultiplyAndAccumulate(
    const double* x, double* y) const {
  CHECK(x != nullptr);
  CHECK(y != nullptr);
  auto& blocks = m_->row_blocks();
  const int num_blocks = blocks.size();
  ParallelFor(
      context_, 0, num_blocks, num_threads_, [this, blocks, x, y](int i) {
        auto* cell_info = layout_[i].get();
        auto& block = blocks[i];
        ConstMatrixRef b(cell_info->values, block.size, block.size);
        VectorRef(y + block.position, block.size).noalias() +=
            b * ConstVectorRef(x + block.position, block.size);
      });
}

}  // namespace ceres::internal
