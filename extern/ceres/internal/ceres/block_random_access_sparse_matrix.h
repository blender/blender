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

#ifndef CERES_INTERNAL_BLOCK_RANDOM_ACCESS_SPARSE_MATRIX_H_
#define CERES_INTERNAL_BLOCK_RANDOM_ACCESS_SPARSE_MATRIX_H_

#include <cstdint>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ceres/block_random_access_matrix.h"
#include "ceres/block_sparse_matrix.h"
#include "ceres/block_structure.h"
#include "ceres/context_impl.h"
#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/export.h"
#include "ceres/small_blas.h"
#include "ceres/types.h"

namespace ceres::internal {

// A thread safe square block sparse implementation of
// BlockRandomAccessMatrix. Internally a BlockSparseMatrix is used
// for doing the actual storage. This class augments this matrix with
// an unordered_map that allows random read/write access.
class CERES_NO_EXPORT BlockRandomAccessSparseMatrix
    : public BlockRandomAccessMatrix {
 public:
  // blocks is an array of block sizes. block_pairs is a set of
  // <row_block_id, col_block_id> pairs to identify the non-zero cells
  // of this matrix.
  BlockRandomAccessSparseMatrix(
      const std::vector<Block>& blocks,
      const std::set<std::pair<int, int>>& block_pairs,
      ContextImpl* context,
      int num_threads);

  // The destructor is not thread safe. It assumes that no one is
  // modifying any cells when the matrix is being destroyed.
  ~BlockRandomAccessSparseMatrix() override = default;

  // BlockRandomAccessMatrix Interface.
  CellInfo* GetCell(int row_block_id,
                    int col_block_id,
                    int* row,
                    int* col,
                    int* row_stride,
                    int* col_stride) final;

  // This is not a thread safe method, it assumes that no cell is
  // locked.
  void SetZero() final;

  // Assume that the matrix is symmetric and only one half of the matrix is
  // stored.
  //
  // y += S * x
  void SymmetricRightMultiplyAndAccumulate(const double* x, double* y) const;

  // Since the matrix is square, num_rows() == num_cols().
  int num_rows() const final { return bsm_->num_rows(); }
  int num_cols() const final { return bsm_->num_cols(); }

  // Access to the underlying matrix object.
  const BlockSparseMatrix* matrix() const { return bsm_.get(); }
  BlockSparseMatrix* mutable_matrix() { return bsm_.get(); }

 private:
  int64_t IntPairToInt64(int row, int col) const {
    return row * kRowShift + col;
  }

  void Int64ToIntPair(int64_t index, int* row, int* col) const {
    *row = index / kRowShift;
    *col = index % kRowShift;
  }

  constexpr static int64_t kRowShift{1ll << 32};

  // row/column block sizes.
  const std::vector<Block> blocks_;
  ContextImpl* context_ = nullptr;
  const int num_threads_ = 1;

  // A mapping from <row_block_id, col_block_id> to the position in
  // the values array of tsm_ where the block is stored.
  using LayoutType = std::unordered_map<int64_t, std::unique_ptr<CellInfo>>;
  LayoutType layout_;

  // The underlying matrix object which actually stores the cells.
  std::unique_ptr<BlockSparseMatrix> bsm_;

  friend class BlockRandomAccessSparseMatrixTest;
};

}  // namespace ceres::internal

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_INTERNAL_BLOCK_RANDOM_ACCESS_SPARSE_MATRIX_H_
