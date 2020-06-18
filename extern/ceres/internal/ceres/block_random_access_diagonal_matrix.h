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
// Author: sameeragarwal@google.com (Sameer Agarwal)

#ifndef CERES_INTERNAL_BLOCK_RANDOM_ACCESS_DIAGONAL_MATRIX_H_
#define CERES_INTERNAL_BLOCK_RANDOM_ACCESS_DIAGONAL_MATRIX_H_

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "ceres/block_random_access_matrix.h"
#include "ceres/internal/port.h"
#include "ceres/triplet_sparse_matrix.h"
#include "ceres/types.h"

namespace ceres {
namespace internal {

// A thread safe block diagonal matrix implementation of
// BlockRandomAccessMatrix.
class BlockRandomAccessDiagonalMatrix : public BlockRandomAccessMatrix {
 public:
  // blocks is an array of block sizes.
  explicit BlockRandomAccessDiagonalMatrix(const std::vector<int>& blocks);
  BlockRandomAccessDiagonalMatrix(const BlockRandomAccessDiagonalMatrix&) = delete;
  void operator=(const BlockRandomAccessDiagonalMatrix&) = delete;

  // The destructor is not thread safe. It assumes that no one is
  // modifying any cells when the matrix is being destroyed.
  virtual ~BlockRandomAccessDiagonalMatrix();

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

  // Invert the matrix assuming that each block is positive definite.
  void Invert();

  // y += S * x
  void RightMultiply(const double* x, double* y) const;

  // Since the matrix is square, num_rows() == num_cols().
  int num_rows() const final { return tsm_->num_rows(); }
  int num_cols() const final { return tsm_->num_cols(); }

  const TripletSparseMatrix* matrix() const { return tsm_.get(); }
  TripletSparseMatrix* mutable_matrix() { return tsm_.get(); }

 private:
  // row/column block sizes.
  const std::vector<int> blocks_;
  std::vector<CellInfo*> layout_;

  // The underlying matrix object which actually stores the cells.
  std::unique_ptr<TripletSparseMatrix> tsm_;

  friend class BlockRandomAccessDiagonalMatrixTest;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_BLOCK_RANDOM_ACCESS_DIAGONAL_MATRIX_H_
