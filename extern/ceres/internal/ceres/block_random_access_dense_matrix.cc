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

#include "ceres/block_random_access_dense_matrix.h"

#include <utility>
#include <vector>

#include "ceres/internal/eigen.h"
#include "ceres/parallel_vector_ops.h"
#include "glog/logging.h"

namespace ceres::internal {

BlockRandomAccessDenseMatrix::BlockRandomAccessDenseMatrix(
    std::vector<Block> blocks, ContextImpl* context, int num_threads)
    : blocks_(std::move(blocks)), context_(context), num_threads_(num_threads) {
  const int num_blocks = blocks_.size();
  num_rows_ = NumScalarEntries(blocks_);
  values_ = std::make_unique<double[]>(num_rows_ * num_rows_);
  cell_infos_ = std::make_unique<CellInfo[]>(num_blocks * num_blocks);
  for (int i = 0; i < num_blocks * num_blocks; ++i) {
    cell_infos_[i].values = values_.get();
  }

  SetZero();
}

CellInfo* BlockRandomAccessDenseMatrix::GetCell(const int row_block_id,
                                                const int col_block_id,
                                                int* row,
                                                int* col,
                                                int* row_stride,
                                                int* col_stride) {
  *row = blocks_[row_block_id].position;
  *col = blocks_[col_block_id].position;
  *row_stride = num_rows_;
  *col_stride = num_rows_;
  return &cell_infos_[row_block_id * blocks_.size() + col_block_id];
}

// Assume that the user does not hold any locks on any cell blocks
// when they are calling SetZero.
void BlockRandomAccessDenseMatrix::SetZero() {
  ParallelSetZero(context_, num_threads_, values_.get(), num_rows_ * num_rows_);
}

}  // namespace ceres::internal
