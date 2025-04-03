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

#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

#include "ceres/block_sparse_matrix.h"
#include "ceres/block_structure.h"
#include "ceres/internal/eigen.h"
#include "ceres/parallel_for.h"
#include "ceres/partition_range_for_parallel_for.h"
#include "ceres/partitioned_matrix_view.h"
#include "ceres/small_blas.h"
#include "glog/logging.h"

namespace ceres::internal {

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
    PartitionedMatrixView(const LinearSolver::Options& options,
                          const BlockSparseMatrix& matrix)

    : options_(options), matrix_(matrix) {
  const CompressedRowBlockStructure* bs = matrix_.block_structure();
  CHECK(bs != nullptr);

  num_col_blocks_e_ = options_.elimination_groups[0];
  num_col_blocks_f_ = bs->cols.size() - num_col_blocks_e_;

  // Compute the number of row blocks in E. The number of row blocks
  // in E maybe less than the number of row blocks in the input matrix
  // as some of the row blocks at the bottom may not have any
  // e_blocks. For a definition of what an e_block is, please see
  // schur_complement_solver.h
  num_row_blocks_e_ = 0;
  for (const auto& row : bs->rows) {
    const std::vector<Cell>& cells = row.cells;
    if (cells[0].block_id < num_col_blocks_e_) {
      ++num_row_blocks_e_;
    }
  }

  // Compute the number of columns in E and F.
  num_cols_e_ = 0;
  num_cols_f_ = 0;

  for (int c = 0; c < bs->cols.size(); ++c) {
    const Block& block = bs->cols[c];
    if (c < num_col_blocks_e_) {
      num_cols_e_ += block.size;
    } else {
      num_cols_f_ += block.size;
    }
  }

  CHECK_EQ(num_cols_e_ + num_cols_f_, matrix_.num_cols());

  auto transpose_bs = matrix_.transpose_block_structure();
  const int num_threads = options_.num_threads;
  if (transpose_bs != nullptr && num_threads > 1) {
    int kMaxPartitions = num_threads * 4;
    e_cols_partition_ = PartitionRangeForParallelFor(
        0,
        num_col_blocks_e_,
        kMaxPartitions,
        transpose_bs->rows.data(),
        [](const CompressedRow& row) { return row.cumulative_nnz; });

    f_cols_partition_ = PartitionRangeForParallelFor(
        num_col_blocks_e_,
        num_col_blocks_e_ + num_col_blocks_f_,
        kMaxPartitions,
        transpose_bs->rows.data(),
        [](const CompressedRow& row) { return row.cumulative_nnz; });
  }
}

// The next four methods don't seem to be particularly cache
// friendly. This is an artifact of how the BlockStructure of the
// input matrix is constructed. These methods will benefit from
// multithreading as well as improved data layout.

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
    RightMultiplyAndAccumulateE(const double* x, double* y) const {
  // Iterate over the first num_row_blocks_e_ row blocks, and multiply
  // by the first cell in each row block.
  auto bs = matrix_.block_structure();
  const double* values = matrix_.values();
  ParallelFor(options_.context,
              0,
              num_row_blocks_e_,
              options_.num_threads,
              [values, bs, x, y](int row_block_id) {
                const Cell& cell = bs->rows[row_block_id].cells[0];
                const int row_block_pos = bs->rows[row_block_id].block.position;
                const int row_block_size = bs->rows[row_block_id].block.size;
                const int col_block_id = cell.block_id;
                const int col_block_pos = bs->cols[col_block_id].position;
                const int col_block_size = bs->cols[col_block_id].size;
                // clang-format off
                MatrixVectorMultiply<kRowBlockSize, kEBlockSize, 1>(
                    values + cell.position, row_block_size, col_block_size,
                    x + col_block_pos,
                    y + row_block_pos);
                // clang-format on
              });
}

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
    RightMultiplyAndAccumulateF(const double* x, double* y) const {
  // Iterate over row blocks, and if the row block is in E, then
  // multiply by all the cells except the first one which is of type
  // E. If the row block is not in E (i.e its in the bottom
  // num_row_blocks - num_row_blocks_e row blocks), then all the cells
  // are of type F and multiply by them all.
  const CompressedRowBlockStructure* bs = matrix_.block_structure();
  const int num_row_blocks = bs->rows.size();
  const int num_cols_e = num_cols_e_;
  const double* values = matrix_.values();
  ParallelFor(options_.context,
              0,
              num_row_blocks_e_,
              options_.num_threads,
              [values, bs, num_cols_e, x, y](int row_block_id) {
                const int row_block_pos = bs->rows[row_block_id].block.position;
                const int row_block_size = bs->rows[row_block_id].block.size;
                const auto& cells = bs->rows[row_block_id].cells;
                for (int c = 1; c < cells.size(); ++c) {
                  const int col_block_id = cells[c].block_id;
                  const int col_block_pos = bs->cols[col_block_id].position;
                  const int col_block_size = bs->cols[col_block_id].size;
                  // clang-format off
                  MatrixVectorMultiply<kRowBlockSize, kFBlockSize, 1>(
                      values + cells[c].position, row_block_size, col_block_size,
                      x + col_block_pos - num_cols_e,
                      y + row_block_pos);
                  // clang-format on
                }
              });
  ParallelFor(options_.context,
              num_row_blocks_e_,
              num_row_blocks,
              options_.num_threads,
              [values, bs, num_cols_e, x, y](int row_block_id) {
                const int row_block_pos = bs->rows[row_block_id].block.position;
                const int row_block_size = bs->rows[row_block_id].block.size;
                const auto& cells = bs->rows[row_block_id].cells;
                for (const auto& cell : cells) {
                  const int col_block_id = cell.block_id;
                  const int col_block_pos = bs->cols[col_block_id].position;
                  const int col_block_size = bs->cols[col_block_id].size;
                  // clang-format off
                  MatrixVectorMultiply<Eigen::Dynamic, Eigen::Dynamic, 1>(
                      values + cell.position, row_block_size, col_block_size,
                      x + col_block_pos - num_cols_e,
                      y + row_block_pos);
                  // clang-format on
                }
              });
}

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
    LeftMultiplyAndAccumulateE(const double* x, double* y) const {
  if (!num_col_blocks_e_) return;
  if (!num_row_blocks_e_) return;
  if (options_.num_threads == 1) {
    LeftMultiplyAndAccumulateESingleThreaded(x, y);
  } else {
    CHECK(options_.context != nullptr);
    LeftMultiplyAndAccumulateEMultiThreaded(x, y);
  }
}

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
    LeftMultiplyAndAccumulateESingleThreaded(const double* x, double* y) const {
  const CompressedRowBlockStructure* bs = matrix_.block_structure();

  // Iterate over the first num_row_blocks_e_ row blocks, and multiply
  // by the first cell in each row block.
  const double* values = matrix_.values();
  for (int r = 0; r < num_row_blocks_e_; ++r) {
    const Cell& cell = bs->rows[r].cells[0];
    const int row_block_pos = bs->rows[r].block.position;
    const int row_block_size = bs->rows[r].block.size;
    const int col_block_id = cell.block_id;
    const int col_block_pos = bs->cols[col_block_id].position;
    const int col_block_size = bs->cols[col_block_id].size;
    // clang-format off
    MatrixTransposeVectorMultiply<kRowBlockSize, kEBlockSize, 1>(
        values + cell.position, row_block_size, col_block_size,
        x + row_block_pos,
        y + col_block_pos);
    // clang-format on
  }
}

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
    LeftMultiplyAndAccumulateEMultiThreaded(const double* x, double* y) const {
  auto transpose_bs = matrix_.transpose_block_structure();
  CHECK(transpose_bs != nullptr);

  // Local copies of class members in order to avoid capturing pointer to the
  // whole object in lambda function
  auto values = matrix_.values();
  const int num_row_blocks_e = num_row_blocks_e_;
  ParallelFor(
      options_.context,
      0,
      num_col_blocks_e_,
      options_.num_threads,
      [values, transpose_bs, num_row_blocks_e, x, y](int row_block_id) {
        int row_block_pos = transpose_bs->rows[row_block_id].block.position;
        int row_block_size = transpose_bs->rows[row_block_id].block.size;
        auto& cells = transpose_bs->rows[row_block_id].cells;

        for (auto& cell : cells) {
          const int col_block_id = cell.block_id;
          const int col_block_size = transpose_bs->cols[col_block_id].size;
          const int col_block_pos = transpose_bs->cols[col_block_id].position;
          if (col_block_id >= num_row_blocks_e) break;
          MatrixTransposeVectorMultiply<kRowBlockSize, kEBlockSize, 1>(
              values + cell.position,
              col_block_size,
              row_block_size,
              x + col_block_pos,
              y + row_block_pos);
        }
      },
      e_cols_partition());
}

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
    LeftMultiplyAndAccumulateF(const double* x, double* y) const {
  if (!num_col_blocks_f_) return;
  if (options_.num_threads == 1) {
    LeftMultiplyAndAccumulateFSingleThreaded(x, y);
  } else {
    CHECK(options_.context != nullptr);
    LeftMultiplyAndAccumulateFMultiThreaded(x, y);
  }
}

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
    LeftMultiplyAndAccumulateFSingleThreaded(const double* x, double* y) const {
  const CompressedRowBlockStructure* bs = matrix_.block_structure();

  // Iterate over row blocks, and if the row block is in E, then
  // multiply by all the cells except the first one which is of type
  // E. If the row block is not in E (i.e its in the bottom
  // num_row_blocks - num_row_blocks_e row blocks), then all the cells
  // are of type F and multiply by them all.
  const double* values = matrix_.values();
  for (int r = 0; r < num_row_blocks_e_; ++r) {
    const int row_block_pos = bs->rows[r].block.position;
    const int row_block_size = bs->rows[r].block.size;
    const std::vector<Cell>& cells = bs->rows[r].cells;
    for (int c = 1; c < cells.size(); ++c) {
      const int col_block_id = cells[c].block_id;
      const int col_block_pos = bs->cols[col_block_id].position;
      const int col_block_size = bs->cols[col_block_id].size;
      // clang-format off
      MatrixTransposeVectorMultiply<kRowBlockSize, kFBlockSize, 1>(
        values + cells[c].position, row_block_size, col_block_size,
        x + row_block_pos,
        y + col_block_pos - num_cols_e_);
      // clang-format on
    }
  }

  for (int r = num_row_blocks_e_; r < bs->rows.size(); ++r) {
    const int row_block_pos = bs->rows[r].block.position;
    const int row_block_size = bs->rows[r].block.size;
    const std::vector<Cell>& cells = bs->rows[r].cells;
    for (const auto& cell : cells) {
      const int col_block_id = cell.block_id;
      const int col_block_pos = bs->cols[col_block_id].position;
      const int col_block_size = bs->cols[col_block_id].size;
      // clang-format off
      MatrixTransposeVectorMultiply<Eigen::Dynamic, Eigen::Dynamic, 1>(
        values + cell.position, row_block_size, col_block_size,
        x + row_block_pos,
        y + col_block_pos - num_cols_e_);
      // clang-format on
    }
  }
}

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
    LeftMultiplyAndAccumulateFMultiThreaded(const double* x, double* y) const {
  auto transpose_bs = matrix_.transpose_block_structure();
  CHECK(transpose_bs != nullptr);
  // Local copies of class members  in order to avoid capturing pointer to the
  // whole object in lambda function
  auto values = matrix_.values();
  const int num_row_blocks_e = num_row_blocks_e_;
  const int num_cols_e = num_cols_e_;
  ParallelFor(
      options_.context,
      num_col_blocks_e_,
      num_col_blocks_e_ + num_col_blocks_f_,
      options_.num_threads,
      [values, transpose_bs, num_row_blocks_e, num_cols_e, x, y](
          int row_block_id) {
        int row_block_pos = transpose_bs->rows[row_block_id].block.position;
        int row_block_size = transpose_bs->rows[row_block_id].block.size;
        auto& cells = transpose_bs->rows[row_block_id].cells;

        const int num_cells = cells.size();
        int cell_idx = 0;
        for (; cell_idx < num_cells; ++cell_idx) {
          auto& cell = cells[cell_idx];
          const int col_block_id = cell.block_id;
          const int col_block_size = transpose_bs->cols[col_block_id].size;
          const int col_block_pos = transpose_bs->cols[col_block_id].position;
          if (col_block_id >= num_row_blocks_e) break;

          MatrixTransposeVectorMultiply<kRowBlockSize, kFBlockSize, 1>(
              values + cell.position,
              col_block_size,
              row_block_size,
              x + col_block_pos,
              y + row_block_pos - num_cols_e);
        }
        for (; cell_idx < num_cells; ++cell_idx) {
          auto& cell = cells[cell_idx];
          const int col_block_id = cell.block_id;
          const int col_block_size = transpose_bs->cols[col_block_id].size;
          const int col_block_pos = transpose_bs->cols[col_block_id].position;
          MatrixTransposeVectorMultiply<Eigen::Dynamic, Eigen::Dynamic, 1>(
              values + cell.position,
              col_block_size,
              row_block_size,
              x + col_block_pos,
              y + row_block_pos - num_cols_e);
        }
      },
      f_cols_partition());
}

// Given a range of columns blocks of a matrix m, compute the block
// structure of the block diagonal of the matrix m(:,
// start_col_block:end_col_block)'m(:, start_col_block:end_col_block)
// and return a BlockSparseMatrix with this block structure. The
// caller owns the result.
template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
std::unique_ptr<BlockSparseMatrix>
PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
    CreateBlockDiagonalMatrixLayout(int start_col_block,
                                    int end_col_block) const {
  const CompressedRowBlockStructure* bs = matrix_.block_structure();
  auto* block_diagonal_structure = new CompressedRowBlockStructure;

  int block_position = 0;
  int diagonal_cell_position = 0;

  // Iterate over the column blocks, creating a new diagonal block for
  // each column block.
  for (int c = start_col_block; c < end_col_block; ++c) {
    const Block& block = bs->cols[c];
    block_diagonal_structure->cols.emplace_back();
    Block& diagonal_block = block_diagonal_structure->cols.back();
    diagonal_block.size = block.size;
    diagonal_block.position = block_position;

    block_diagonal_structure->rows.emplace_back();
    CompressedRow& row = block_diagonal_structure->rows.back();
    row.block = diagonal_block;

    row.cells.emplace_back();
    Cell& cell = row.cells.back();
    cell.block_id = c - start_col_block;
    cell.position = diagonal_cell_position;

    block_position += block.size;
    diagonal_cell_position += block.size * block.size;
  }

  // Build a BlockSparseMatrix with the just computed block
  // structure.
  return std::make_unique<BlockSparseMatrix>(block_diagonal_structure);
}

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
std::unique_ptr<BlockSparseMatrix>
PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
    CreateBlockDiagonalEtE() const {
  std::unique_ptr<BlockSparseMatrix> block_diagonal =
      CreateBlockDiagonalMatrixLayout(0, num_col_blocks_e_);
  UpdateBlockDiagonalEtE(block_diagonal.get());
  return block_diagonal;
}

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
std::unique_ptr<BlockSparseMatrix>
PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
    CreateBlockDiagonalFtF() const {
  std::unique_ptr<BlockSparseMatrix> block_diagonal =
      CreateBlockDiagonalMatrixLayout(num_col_blocks_e_,
                                      num_col_blocks_e_ + num_col_blocks_f_);
  UpdateBlockDiagonalFtF(block_diagonal.get());
  return block_diagonal;
}

// Similar to the code in RightMultiplyAndAccumulateE, except instead of the
// matrix vector multiply its an outer product.
//
//    block_diagonal = block_diagonal(E'E)
//
template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
    UpdateBlockDiagonalEtESingleThreaded(
        BlockSparseMatrix* block_diagonal) const {
  auto bs = matrix_.block_structure();
  auto block_diagonal_structure = block_diagonal->block_structure();

  block_diagonal->SetZero();
  const double* values = matrix_.values();
  for (int r = 0; r < num_row_blocks_e_; ++r) {
    const Cell& cell = bs->rows[r].cells[0];
    const int row_block_size = bs->rows[r].block.size;
    const int block_id = cell.block_id;
    const int col_block_size = bs->cols[block_id].size;
    const int cell_position =
        block_diagonal_structure->rows[block_id].cells[0].position;

    // clang-format off
    MatrixTransposeMatrixMultiply
        <kRowBlockSize, kEBlockSize, kRowBlockSize, kEBlockSize, 1>(
            values + cell.position, row_block_size, col_block_size,
            values + cell.position, row_block_size, col_block_size,
            block_diagonal->mutable_values() + cell_position,
            0, 0, col_block_size, col_block_size);
    // clang-format on
  }
}

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
    UpdateBlockDiagonalEtEMultiThreaded(
        BlockSparseMatrix* block_diagonal) const {
  auto transpose_block_structure = matrix_.transpose_block_structure();
  CHECK(transpose_block_structure != nullptr);
  auto block_diagonal_structure = block_diagonal->block_structure();

  const double* values = matrix_.values();
  double* values_diagonal = block_diagonal->mutable_values();
  ParallelFor(
      options_.context,
      0,
      num_col_blocks_e_,
      options_.num_threads,
      [values,
       transpose_block_structure,
       values_diagonal,
       block_diagonal_structure](int col_block_id) {
        int cell_position =
            block_diagonal_structure->rows[col_block_id].cells[0].position;
        double* cell_values = values_diagonal + cell_position;
        int col_block_size =
            transpose_block_structure->rows[col_block_id].block.size;
        auto& cells = transpose_block_structure->rows[col_block_id].cells;
        MatrixRef(cell_values, col_block_size, col_block_size).setZero();

        for (auto& c : cells) {
          int row_block_size = transpose_block_structure->cols[c.block_id].size;
          // clang-format off
          MatrixTransposeMatrixMultiply<kRowBlockSize, kEBlockSize, kRowBlockSize, kEBlockSize, 1>(
            values + c.position, row_block_size, col_block_size,
            values + c.position, row_block_size, col_block_size,
            cell_values, 0, 0, col_block_size, col_block_size);
          // clang-format on
        }
      },
      e_cols_partition_);
}

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
    UpdateBlockDiagonalEtE(BlockSparseMatrix* block_diagonal) const {
  if (options_.num_threads == 1) {
    UpdateBlockDiagonalEtESingleThreaded(block_diagonal);
  } else {
    CHECK(options_.context != nullptr);
    UpdateBlockDiagonalEtEMultiThreaded(block_diagonal);
  }
}

// Similar to the code in RightMultiplyAndAccumulateF, except instead of the
// matrix vector multiply its an outer product.
//
//   block_diagonal = block_diagonal(F'F)
//
template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
    UpdateBlockDiagonalFtFSingleThreaded(
        BlockSparseMatrix* block_diagonal) const {
  auto bs = matrix_.block_structure();
  auto block_diagonal_structure = block_diagonal->block_structure();

  block_diagonal->SetZero();
  const double* values = matrix_.values();
  for (int r = 0; r < num_row_blocks_e_; ++r) {
    const int row_block_size = bs->rows[r].block.size;
    const std::vector<Cell>& cells = bs->rows[r].cells;
    for (int c = 1; c < cells.size(); ++c) {
      const int col_block_id = cells[c].block_id;
      const int col_block_size = bs->cols[col_block_id].size;
      const int diagonal_block_id = col_block_id - num_col_blocks_e_;
      const int cell_position =
          block_diagonal_structure->rows[diagonal_block_id].cells[0].position;

      // clang-format off
      MatrixTransposeMatrixMultiply
          <kRowBlockSize, kFBlockSize, kRowBlockSize, kFBlockSize, 1>(
              values + cells[c].position, row_block_size, col_block_size,
              values + cells[c].position, row_block_size, col_block_size,
              block_diagonal->mutable_values() + cell_position,
              0, 0, col_block_size, col_block_size);
      // clang-format on
    }
  }

  for (int r = num_row_blocks_e_; r < bs->rows.size(); ++r) {
    const int row_block_size = bs->rows[r].block.size;
    const std::vector<Cell>& cells = bs->rows[r].cells;
    for (const auto& cell : cells) {
      const int col_block_id = cell.block_id;
      const int col_block_size = bs->cols[col_block_id].size;
      const int diagonal_block_id = col_block_id - num_col_blocks_e_;
      const int cell_position =
          block_diagonal_structure->rows[diagonal_block_id].cells[0].position;

      // clang-format off
      MatrixTransposeMatrixMultiply
          <Eigen::Dynamic, Eigen::Dynamic, Eigen::Dynamic, Eigen::Dynamic, 1>(
              values + cell.position, row_block_size, col_block_size,
              values + cell.position, row_block_size, col_block_size,
              block_diagonal->mutable_values() + cell_position,
              0, 0, col_block_size, col_block_size);
      // clang-format on
    }
  }
}

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
    UpdateBlockDiagonalFtFMultiThreaded(
        BlockSparseMatrix* block_diagonal) const {
  auto transpose_block_structure = matrix_.transpose_block_structure();
  CHECK(transpose_block_structure != nullptr);
  auto block_diagonal_structure = block_diagonal->block_structure();

  const double* values = matrix_.values();
  double* values_diagonal = block_diagonal->mutable_values();

  const int num_col_blocks_e = num_col_blocks_e_;
  const int num_row_blocks_e = num_row_blocks_e_;
  ParallelFor(
      options_.context,
      num_col_blocks_e_,
      num_col_blocks_e + num_col_blocks_f_,
      options_.num_threads,
      [transpose_block_structure,
       block_diagonal_structure,
       num_col_blocks_e,
       num_row_blocks_e,
       values,
       values_diagonal](int col_block_id) {
        const int col_block_size =
            transpose_block_structure->rows[col_block_id].block.size;
        const int diagonal_block_id = col_block_id - num_col_blocks_e;
        const int cell_position =
            block_diagonal_structure->rows[diagonal_block_id].cells[0].position;
        double* cell_values = values_diagonal + cell_position;

        MatrixRef(cell_values, col_block_size, col_block_size).setZero();

        auto& cells = transpose_block_structure->rows[col_block_id].cells;
        const int num_cells = cells.size();
        int i = 0;
        for (; i < num_cells; ++i) {
          auto& cell = cells[i];
          const int row_block_id = cell.block_id;
          if (row_block_id >= num_row_blocks_e) break;
          const int row_block_size =
              transpose_block_structure->cols[row_block_id].size;
          // clang-format off
          MatrixTransposeMatrixMultiply
              <kRowBlockSize, kFBlockSize, kRowBlockSize, kFBlockSize, 1>(
                  values + cell.position, row_block_size, col_block_size,
                  values + cell.position, row_block_size, col_block_size,
                  cell_values, 0, 0, col_block_size, col_block_size);
          // clang-format on
        }
        for (; i < num_cells; ++i) {
          auto& cell = cells[i];
          const int row_block_id = cell.block_id;
          const int row_block_size =
              transpose_block_structure->cols[row_block_id].size;
          // clang-format off
          MatrixTransposeMatrixMultiply
              <Eigen::Dynamic, Eigen::Dynamic, Eigen::Dynamic, Eigen::Dynamic, 1>(
                  values + cell.position, row_block_size, col_block_size,
                  values + cell.position, row_block_size, col_block_size,
                  cell_values, 0, 0, col_block_size, col_block_size);
          // clang-format on
        }
      },
      f_cols_partition_);
}

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
    UpdateBlockDiagonalFtF(BlockSparseMatrix* block_diagonal) const {
  if (options_.num_threads == 1) {
    UpdateBlockDiagonalFtFSingleThreaded(block_diagonal);
  } else {
    CHECK(options_.context != nullptr);
    UpdateBlockDiagonalFtFMultiThreaded(block_diagonal);
  }
}

}  // namespace ceres::internal
