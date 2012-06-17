// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2012 Google Inc. All rights reserved.
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

#include "ceres/block_jacobi_preconditioner.h"

#include "Eigen/Cholesky"
#include "ceres/block_sparse_matrix.h"
#include "ceres/block_structure.h"
#include "ceres/casts.h"
#include "ceres/integral_types.h"
#include "ceres/internal/eigen.h"

namespace ceres {
namespace internal {

BlockJacobiPreconditioner::BlockJacobiPreconditioner(
    const LinearOperator& A)
    : block_structure_(
        *(down_cast<const BlockSparseMatrix*>(&A)->block_structure())),
      num_rows_(A.num_rows()) {
  // Calculate the amount of storage needed.
  int storage_needed = 0;
  for (int c = 0; c < block_structure_.cols.size(); ++c) {
    int size = block_structure_.cols[c].size;
    storage_needed += size * size;
  }

  // Size the offsets and storage.
  blocks_.resize(block_structure_.cols.size());
  block_storage_.resize(storage_needed);

  // Put pointers to the storage in the offsets.
  double* block_cursor = &block_storage_[0];
  for (int c = 0; c < block_structure_.cols.size(); ++c) {
    int size = block_structure_.cols[c].size;
    blocks_[c] = block_cursor;
    block_cursor += size * size;
  }
}

BlockJacobiPreconditioner::~BlockJacobiPreconditioner() {
}

void BlockJacobiPreconditioner::Update(const LinearOperator& matrix, const double* D) {
  const BlockSparseMatrix& A = *(down_cast<const BlockSparseMatrix*>(&matrix));
  const CompressedRowBlockStructure* bs = A.block_structure();

  // Compute the diagonal blocks by block inner products.
  std::fill(block_storage_.begin(), block_storage_.end(), 0.0);
  for (int r = 0; r < bs->rows.size(); ++r) {
    const int row_block_size = bs->rows[r].block.size;
    const vector<Cell>& cells = bs->rows[r].cells;
    const double* row_values = A.RowBlockValues(r);
    for (int c = 0; c < cells.size(); ++c) {
      const int col_block_size = bs->cols[cells[c].block_id].size;
      ConstMatrixRef m(row_values + cells[c].position,
                       row_block_size,
                       col_block_size);

      MatrixRef(blocks_[cells[c].block_id],
                col_block_size,
                col_block_size).noalias() += m.transpose() * m;

      // TODO(keir): Figure out when the below expression is actually faster
      // than doing the full rank update. The issue is that for smaller sizes,
      // the rankUpdate() function is slower than the full product done above.
      //
      // On the typical bundling problems, the above product is ~5% faster.
      //
      //   MatrixRef(blocks_[cells[c].block_id],
      //             col_block_size,
      //             col_block_size).selfadjointView<Eigen::Upper>().rankUpdate(m);
      //
    }
  }

  // Add the diagonal and invert each block.
  for (int c = 0; c < bs->cols.size(); ++c) {
    const int size = block_structure_.cols[c].size;
    const int position = block_structure_.cols[c].position;
    MatrixRef block(blocks_[c], size, size);

    if (D != NULL) {
      block.diagonal() += ConstVectorRef(D + position, size).array().square().matrix();
    }

    block = block.selfadjointView<Eigen::Upper>()
                 .ldlt()
                 .solve(Matrix::Identity(size, size));
  }
}

void BlockJacobiPreconditioner::RightMultiply(const double* x, double* y) const {
  for (int c = 0; c < block_structure_.cols.size(); ++c) {
    const int size = block_structure_.cols[c].size;
    const int position = block_structure_.cols[c].position;
    ConstMatrixRef D(blocks_[c], size, size);
    ConstVectorRef x_block(x + position, size);
    VectorRef y_block(y + position, size);
    y_block += D * x_block;
  }
}

void BlockJacobiPreconditioner::LeftMultiply(const double* x, double* y) const {
  RightMultiply(x, y);
}

}  // namespace internal
}  // namespace ceres
