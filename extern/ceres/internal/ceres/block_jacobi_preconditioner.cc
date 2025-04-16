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

#include "ceres/block_jacobi_preconditioner.h"

#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "Eigen/Dense"
#include "ceres/block_random_access_diagonal_matrix.h"
#include "ceres/block_sparse_matrix.h"
#include "ceres/block_structure.h"
#include "ceres/casts.h"
#include "ceres/internal/eigen.h"
#include "ceres/parallel_for.h"
#include "ceres/small_blas.h"

namespace ceres::internal {

BlockSparseJacobiPreconditioner::BlockSparseJacobiPreconditioner(
    Preconditioner::Options options, const BlockSparseMatrix& A)
    : options_(std::move(options)) {
  m_ = std::make_unique<BlockRandomAccessDiagonalMatrix>(
      A.block_structure()->cols, options_.context, options_.num_threads);
}

BlockSparseJacobiPreconditioner::~BlockSparseJacobiPreconditioner() = default;

bool BlockSparseJacobiPreconditioner::UpdateImpl(const BlockSparseMatrix& A,
                                                 const double* D) {
  const CompressedRowBlockStructure* bs = A.block_structure();
  const double* values = A.values();
  m_->SetZero();

  ParallelFor(options_.context,
              0,
              bs->rows.size(),
              options_.num_threads,
              [this, bs, values](int i) {
                const int row_block_size = bs->rows[i].block.size;
                const std::vector<Cell>& cells = bs->rows[i].cells;
                for (const auto& cell : cells) {
                  const int block_id = cell.block_id;
                  const int col_block_size = bs->cols[block_id].size;
                  int r, c, row_stride, col_stride;
                  CellInfo* cell_info = m_->GetCell(
                      block_id, block_id, &r, &c, &row_stride, &col_stride);
                  MatrixRef m(cell_info->values, row_stride, col_stride);
                  ConstMatrixRef b(
                      values + cell.position, row_block_size, col_block_size);
                  auto lock =
                      MakeConditionalLock(options_.num_threads, cell_info->m);
                  // clang-format off
                  MatrixTransposeMatrixMultiply<Eigen::Dynamic, Eigen::Dynamic,
                      Eigen::Dynamic,Eigen::Dynamic, 1>(
                          values + cell.position, row_block_size,col_block_size,
                          values + cell.position, row_block_size,col_block_size,
                          cell_info->values,r, c,row_stride,col_stride);
                  // clang-format on
                }
              });

  if (D != nullptr) {
    // Add the diagonal.
    ParallelFor(options_.context,
                0,
                bs->cols.size(),
                options_.num_threads,
                [this, bs, D](int i) {
                  const int block_size = bs->cols[i].size;
                  int r, c, row_stride, col_stride;
                  CellInfo* cell_info =
                      m_->GetCell(i, i, &r, &c, &row_stride, &col_stride);
                  MatrixRef m(cell_info->values, row_stride, col_stride);
                  m.block(r, c, block_size, block_size).diagonal() +=
                      ConstVectorRef(D + bs->cols[i].position, block_size)
                          .array()
                          .square()
                          .matrix();
                });
  }

  m_->Invert();
  return true;
}

BlockCRSJacobiPreconditioner::BlockCRSJacobiPreconditioner(
    Preconditioner::Options options, const CompressedRowSparseMatrix& A)
    : options_(std::move(options)), locks_(A.col_blocks().size()) {
  auto& col_blocks = A.col_blocks();

  // Compute the number of non-zeros in the preconditioner. This is needed so
  // that we can construct the CompressedRowSparseMatrix.
  const int m_nnz = SumSquaredSizes(col_blocks);
  m_ = std::make_unique<CompressedRowSparseMatrix>(
      A.num_cols(), A.num_cols(), m_nnz);

  const int num_col_blocks = col_blocks.size();

  // Populate the sparsity structure of the preconditioner matrix.
  int* m_cols = m_->mutable_cols();
  int* m_rows = m_->mutable_rows();
  m_rows[0] = 0;
  for (int i = 0, idx = 0; i < num_col_blocks; ++i) {
    // For each column block populate a diagonal block in the preconditioner.
    // Not that the because of the way the CompressedRowSparseMatrix format
    // works, the entire diagonal block is laid out contiguously in memory as a
    // row-major matrix. We will use this when updating the block.
    auto& block = col_blocks[i];
    for (int j = 0; j < block.size; ++j) {
      for (int k = 0; k < block.size; ++k, ++idx) {
        m_cols[idx] = block.position + k;
      }
      m_rows[block.position + j + 1] = idx;
    }
  }

  // In reality we only need num_col_blocks locks, however that would require
  // that in UpdateImpl we are able to look up the column block from the it
  // first column. To save ourselves this map we will instead spend a few extra
  // lock objects.
  std::vector<std::mutex> locks(A.num_cols());
  locks_.swap(locks);
  CHECK_EQ(m_rows[A.num_cols()], m_nnz);
}

BlockCRSJacobiPreconditioner::~BlockCRSJacobiPreconditioner() = default;

bool BlockCRSJacobiPreconditioner::UpdateImpl(
    const CompressedRowSparseMatrix& A, const double* D) {
  const auto& col_blocks = A.col_blocks();
  const auto& row_blocks = A.row_blocks();
  const int num_col_blocks = col_blocks.size();
  const int num_row_blocks = row_blocks.size();

  const int* a_rows = A.rows();
  const int* a_cols = A.cols();
  const double* a_values = A.values();
  double* m_values = m_->mutable_values();
  const int* m_rows = m_->rows();

  m_->SetZero();

  ParallelFor(
      options_.context,
      0,
      num_row_blocks,
      options_.num_threads,
      [this, row_blocks, a_rows, a_cols, a_values, m_values, m_rows](int i) {
        const int row = row_blocks[i].position;
        const int row_block_size = row_blocks[i].size;
        const int row_nnz = a_rows[row + 1] - a_rows[row];
        ConstMatrixRef row_block(
            a_values + a_rows[row], row_block_size, row_nnz);
        int c = 0;
        while (c < row_nnz) {
          const int idx = a_rows[row] + c;
          const int col = a_cols[idx];
          const int col_block_size = m_rows[col + 1] - m_rows[col];

          // We make use of the fact that the entire diagonal block is
          // stored contiguously in memory as a row-major matrix.
          MatrixRef m(m_values + m_rows[col], col_block_size, col_block_size);
          // We do not have a row_stride version of
          // MatrixTransposeMatrixMultiply, otherwise we could use it
          // here to further speed up the following expression.
          auto b = row_block.middleCols(c, col_block_size);
          auto lock = MakeConditionalLock(options_.num_threads, locks_[col]);
          m.noalias() += b.transpose() * b;
          c += col_block_size;
        }
      });

  ParallelFor(
      options_.context,
      0,
      num_col_blocks,
      options_.num_threads,
      [col_blocks, m_rows, m_values, D](int i) {
        const int col = col_blocks[i].position;
        const int col_block_size = col_blocks[i].size;
        MatrixRef m(m_values + m_rows[col], col_block_size, col_block_size);

        if (D != nullptr) {
          m.diagonal() +=
              ConstVectorRef(D + col, col_block_size).array().square().matrix();
        }

        // TODO(sameeragarwal): Deal with Cholesky inversion failure here and
        // elsewhere.
        m = m.llt().solve(Matrix::Identity(col_block_size, col_block_size));
      });

  return true;
}

}  // namespace ceres::internal
