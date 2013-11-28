// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2010, 2011, 2012 Google Inc. All rights reserved.
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
// Author: sameeragarwal@google.com (Sameer Agarwal)

#include "ceres/partitioned_matrix_view.h"

#include <algorithm>
#include <cstring>
#include <vector>
#include "ceres/block_sparse_matrix.h"
#include "ceres/block_structure.h"
#include "ceres/internal/eigen.h"
#include "ceres/small_blas.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
PartitionedMatrixView(
    const BlockSparseMatrix& matrix,
    int num_col_blocks_e)
    : matrix_(matrix),
      num_col_blocks_e_(num_col_blocks_e) {
  const CompressedRowBlockStructure* bs = matrix_.block_structure();
  CHECK_NOTNULL(bs);

  num_col_blocks_f_ = bs->cols.size() - num_col_blocks_e_;

  // Compute the number of row blocks in E. The number of row blocks
  // in E maybe less than the number of row blocks in the input matrix
  // as some of the row blocks at the bottom may not have any
  // e_blocks. For a definition of what an e_block is, please see
  // explicit_schur_complement_solver.h
  num_row_blocks_e_ = 0;
  for (int r = 0; r < bs->rows.size(); ++r) {
    const vector<Cell>& cells = bs->rows[r].cells;
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
}

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
~PartitionedMatrixView() {
}

// The next four methods don't seem to be particularly cache
// friendly. This is an artifact of how the BlockStructure of the
// input matrix is constructed. These methods will benefit from
// multithreading as well as improved data layout.

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void
PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
RightMultiplyE(const double* x, double* y) const {
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
    MatrixVectorMultiply<kRowBlockSize, kEBlockSize, 1>(
        values + cell.position, row_block_size, col_block_size,
        x + col_block_pos,
        y + row_block_pos);
  }
}

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void
PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
RightMultiplyF(const double* x, double* y) const {
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
    const vector<Cell>& cells = bs->rows[r].cells;
    for (int c = 1; c < cells.size(); ++c) {
      const int col_block_id = cells[c].block_id;
      const int col_block_pos = bs->cols[col_block_id].position;
      const int col_block_size = bs->cols[col_block_id].size;
      MatrixVectorMultiply<kRowBlockSize, kFBlockSize, 1>(
          values + cells[c].position, row_block_size, col_block_size,
          x + col_block_pos - num_cols_e_,
          y + row_block_pos);
    }
  }

  for (int r = num_row_blocks_e_; r < bs->rows.size(); ++r) {
    const int row_block_pos = bs->rows[r].block.position;
    const int row_block_size = bs->rows[r].block.size;
    const vector<Cell>& cells = bs->rows[r].cells;
    for (int c = 0; c < cells.size(); ++c) {
      const int col_block_id = cells[c].block_id;
      const int col_block_pos = bs->cols[col_block_id].position;
      const int col_block_size = bs->cols[col_block_id].size;
      MatrixVectorMultiply<Eigen::Dynamic, Eigen::Dynamic, 1>(
          values + cells[c].position, row_block_size, col_block_size,
          x + col_block_pos - num_cols_e_,
          y + row_block_pos);
    }
  }
}

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void
PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
LeftMultiplyE(const double* x, double* y) const {
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
    MatrixTransposeVectorMultiply<kRowBlockSize, kEBlockSize, 1>(
        values + cell.position, row_block_size, col_block_size,
        x + row_block_pos,
        y + col_block_pos);
  }
}

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void
PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
LeftMultiplyF(const double* x, double* y) const {
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
    const vector<Cell>& cells = bs->rows[r].cells;
    for (int c = 1; c < cells.size(); ++c) {
      const int col_block_id = cells[c].block_id;
      const int col_block_pos = bs->cols[col_block_id].position;
      const int col_block_size = bs->cols[col_block_id].size;
      MatrixTransposeVectorMultiply<kRowBlockSize, kFBlockSize, 1>(
        values + cells[c].position, row_block_size, col_block_size,
        x + row_block_pos,
        y + col_block_pos - num_cols_e_);
    }
  }

  for (int r = num_row_blocks_e_; r < bs->rows.size(); ++r) {
    const int row_block_pos = bs->rows[r].block.position;
    const int row_block_size = bs->rows[r].block.size;
    const vector<Cell>& cells = bs->rows[r].cells;
    for (int c = 0; c < cells.size(); ++c) {
      const int col_block_id = cells[c].block_id;
      const int col_block_pos = bs->cols[col_block_id].position;
      const int col_block_size = bs->cols[col_block_id].size;
      MatrixTransposeVectorMultiply<Eigen::Dynamic, Eigen::Dynamic, 1>(
        values + cells[c].position, row_block_size, col_block_size,
        x + row_block_pos,
        y + col_block_pos - num_cols_e_);
    }
  }
}

// Given a range of columns blocks of a matrix m, compute the block
// structure of the block diagonal of the matrix m(:,
// start_col_block:end_col_block)'m(:, start_col_block:end_col_block)
// and return a BlockSparseMatrix with the this block structure. The
// caller owns the result.
template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
BlockSparseMatrix*
PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
CreateBlockDiagonalMatrixLayout(int start_col_block, int end_col_block) const {
  const CompressedRowBlockStructure* bs = matrix_.block_structure();
  CompressedRowBlockStructure* block_diagonal_structure =
      new CompressedRowBlockStructure;

  int block_position = 0;
  int diagonal_cell_position = 0;

  // Iterate over the column blocks, creating a new diagonal block for
  // each column block.
  for (int c = start_col_block; c < end_col_block; ++c) {
    const Block& block = bs->cols[c];
    block_diagonal_structure->cols.push_back(Block());
    Block& diagonal_block = block_diagonal_structure->cols.back();
    diagonal_block.size = block.size;
    diagonal_block.position = block_position;

    block_diagonal_structure->rows.push_back(CompressedRow());
    CompressedRow& row = block_diagonal_structure->rows.back();
    row.block = diagonal_block;

    row.cells.push_back(Cell());
    Cell& cell = row.cells.back();
    cell.block_id = c - start_col_block;
    cell.position = diagonal_cell_position;

    block_position += block.size;
    diagonal_cell_position += block.size * block.size;
  }

  // Build a BlockSparseMatrix with the just computed block
  // structure.
  return new BlockSparseMatrix(block_diagonal_structure);
}

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
BlockSparseMatrix*
PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
CreateBlockDiagonalEtE() const {
  BlockSparseMatrix* block_diagonal =
      CreateBlockDiagonalMatrixLayout(0, num_col_blocks_e_);
  UpdateBlockDiagonalEtE(block_diagonal);
  return block_diagonal;
}

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
BlockSparseMatrix*
PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
CreateBlockDiagonalFtF() const {
  BlockSparseMatrix* block_diagonal =
      CreateBlockDiagonalMatrixLayout(
          num_col_blocks_e_, num_col_blocks_e_ + num_col_blocks_f_);
  UpdateBlockDiagonalFtF(block_diagonal);
  return block_diagonal;
}

// Similar to the code in RightMultiplyE, except instead of the matrix
// vector multiply its an outer product.
//
//    block_diagonal = block_diagonal(E'E)
//
template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void
PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
UpdateBlockDiagonalEtE(
    BlockSparseMatrix* block_diagonal) const {
  const CompressedRowBlockStructure* bs = matrix_.block_structure();
  const CompressedRowBlockStructure* block_diagonal_structure =
      block_diagonal->block_structure();

  block_diagonal->SetZero();
  const double* values = matrix_.values();
  for (int r = 0; r < num_row_blocks_e_ ; ++r) {
    const Cell& cell = bs->rows[r].cells[0];
    const int row_block_size = bs->rows[r].block.size;
    const int block_id = cell.block_id;
    const int col_block_size = bs->cols[block_id].size;
    const int cell_position =
        block_diagonal_structure->rows[block_id].cells[0].position;

    MatrixTransposeMatrixMultiply
        <kRowBlockSize, kEBlockSize, kRowBlockSize, kEBlockSize, 1>(
            values + cell.position, row_block_size, col_block_size,
            values + cell.position, row_block_size, col_block_size,
            block_diagonal->mutable_values() + cell_position,
            0, 0, col_block_size, col_block_size);
  }
}

// Similar to the code in RightMultiplyF, except instead of the matrix
// vector multiply its an outer product.
//
//   block_diagonal = block_diagonal(F'F)
//
template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void
PartitionedMatrixView<kRowBlockSize, kEBlockSize, kFBlockSize>::
UpdateBlockDiagonalFtF(BlockSparseMatrix* block_diagonal) const {
  const CompressedRowBlockStructure* bs = matrix_.block_structure();
  const CompressedRowBlockStructure* block_diagonal_structure =
      block_diagonal->block_structure();

  block_diagonal->SetZero();
  const double* values = matrix_.values();
  for (int r = 0; r < num_row_blocks_e_; ++r) {
    const int row_block_size = bs->rows[r].block.size;
    const vector<Cell>& cells = bs->rows[r].cells;
    for (int c = 1; c < cells.size(); ++c) {
      const int col_block_id = cells[c].block_id;
      const int col_block_size = bs->cols[col_block_id].size;
      const int diagonal_block_id = col_block_id - num_col_blocks_e_;
      const int cell_position =
          block_diagonal_structure->rows[diagonal_block_id].cells[0].position;

      MatrixTransposeMatrixMultiply
          <kRowBlockSize, kFBlockSize, kRowBlockSize, kFBlockSize, 1>(
              values + cells[c].position, row_block_size, col_block_size,
              values + cells[c].position, row_block_size, col_block_size,
              block_diagonal->mutable_values() + cell_position,
              0, 0, col_block_size, col_block_size);
    }
  }

  for (int r = num_row_blocks_e_; r < bs->rows.size(); ++r) {
    const int row_block_size = bs->rows[r].block.size;
    const vector<Cell>& cells = bs->rows[r].cells;
    for (int c = 0; c < cells.size(); ++c) {
      const int col_block_id = cells[c].block_id;
      const int col_block_size = bs->cols[col_block_id].size;
      const int diagonal_block_id = col_block_id - num_col_blocks_e_;
      const int cell_position =
          block_diagonal_structure->rows[diagonal_block_id].cells[0].position;

      MatrixTransposeMatrixMultiply
          <Eigen::Dynamic, Eigen::Dynamic, Eigen::Dynamic, Eigen::Dynamic, 1>(
              values + cells[c].position, row_block_size, col_block_size,
              values + cells[c].position, row_block_size, col_block_size,
              block_diagonal->mutable_values() + cell_position,
              0, 0, col_block_size, col_block_size);
    }
  }
}

}  // namespace internal
}  // namespace ceres
