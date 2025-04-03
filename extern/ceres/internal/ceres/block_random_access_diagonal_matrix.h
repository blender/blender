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

#ifndef CERES_INTERNAL_BLOCK_RANDOM_ACCESS_DIAGONAL_MATRIX_H_
#define CERES_INTERNAL_BLOCK_RANDOM_ACCESS_DIAGONAL_MATRIX_H_

#include <memory>
#include <utility>
#include <vector>

#include "ceres/block_random_access_matrix.h"
#include "ceres/block_structure.h"
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/context_impl.h"
#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/export.h"
#include "ceres/types.h"

namespace ceres::internal {

// A BlockRandomAccessMatrix which only stores the block diagonal.
// BlockRandomAccessSparseMatrix can also be used to do this, but this class is
// more efficient in time and in space.
class CERES_NO_EXPORT BlockRandomAccessDiagonalMatrix
    : public BlockRandomAccessMatrix {
 public:
  // blocks is an array of block sizes.
  BlockRandomAccessDiagonalMatrix(const std::vector<Block>& blocks,
                                  ContextImpl* context,
                                  int num_threads);
  ~BlockRandomAccessDiagonalMatrix() override = default;

  // BlockRandomAccessMatrix Interface.
  CellInfo* GetCell(int row_block_id,
                    int col_block_id,
                    int* row,
                    int* col,
                    int* row_stride,
                    int* col_stride) final;

  // m = 0
  void SetZero() final;

  // m = m^{-1}
  void Invert();

  // y += m * x
  void RightMultiplyAndAccumulate(const double* x, double* y) const;

  // Since the matrix is square, num_rows() == num_cols().
  int num_rows() const final { return m_->num_rows(); }
  int num_cols() const final { return m_->num_cols(); }

  const CompressedRowSparseMatrix* matrix() const { return m_.get(); }
  CompressedRowSparseMatrix* mutable_matrix() { return m_.get(); }

 private:
  ContextImpl* context_ = nullptr;
  const int num_threads_ = 1;
  std::unique_ptr<CompressedRowSparseMatrix> m_;
  std::vector<std::unique_ptr<CellInfo>> layout_;
};

}  // namespace ceres::internal

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_INTERNAL_BLOCK_RANDOM_ACCESS_DIAGONAL_MATRIX_H_
