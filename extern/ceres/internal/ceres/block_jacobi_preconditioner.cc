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
// Author: keir@google.com (Keir Mierle)

#include "ceres/block_jacobi_preconditioner.h"

#include "ceres/block_sparse_matrix.h"
#include "ceres/block_structure.h"
#include "ceres/block_random_access_diagonal_matrix.h"
#include "ceres/casts.h"
#include "ceres/internal/eigen.h"

namespace ceres {
namespace internal {

BlockJacobiPreconditioner::BlockJacobiPreconditioner(
    const BlockSparseMatrix& A) {
  const CompressedRowBlockStructure* bs = A.block_structure();
  std::vector<int> blocks(bs->cols.size());
  for (int i = 0; i < blocks.size(); ++i) {
    blocks[i] = bs->cols[i].size;
  }

  m_.reset(new BlockRandomAccessDiagonalMatrix(blocks));
}

BlockJacobiPreconditioner::~BlockJacobiPreconditioner() {}

bool BlockJacobiPreconditioner::UpdateImpl(const BlockSparseMatrix& A,
                                           const double* D) {
  const CompressedRowBlockStructure* bs = A.block_structure();
  const double* values = A.values();
  m_->SetZero();
  for (int i = 0; i < bs->rows.size(); ++i) {
    const int row_block_size = bs->rows[i].block.size;
    const std::vector<Cell>& cells = bs->rows[i].cells;
    for (int j = 0; j < cells.size(); ++j) {
      const int block_id = cells[j].block_id;
      const int col_block_size = bs->cols[block_id].size;

      int r, c, row_stride, col_stride;
      CellInfo* cell_info = m_->GetCell(block_id, block_id,
                                        &r, &c,
                                        &row_stride, &col_stride);
      MatrixRef m(cell_info->values, row_stride, col_stride);
      ConstMatrixRef b(values + cells[j].position,
                       row_block_size,
                       col_block_size);
      m.block(r, c, col_block_size, col_block_size) += b.transpose() * b;
    }
  }

  if (D != NULL) {
    // Add the diagonal.
    int position = 0;
    for (int i = 0; i < bs->cols.size(); ++i) {
      const int block_size = bs->cols[i].size;
      int r, c, row_stride, col_stride;
      CellInfo* cell_info = m_->GetCell(i, i,
                                        &r, &c,
                                        &row_stride, &col_stride);
      MatrixRef m(cell_info->values, row_stride, col_stride);
      m.block(r, c, block_size, block_size).diagonal() +=
          ConstVectorRef(D + position, block_size).array().square().matrix();
      position += block_size;
    }
  }

  m_->Invert();
  return true;
}

void BlockJacobiPreconditioner::RightMultiply(const double* x,
                                              double* y) const {
  m_->RightMultiply(x, y);
}

}  // namespace internal
}  // namespace ceres
