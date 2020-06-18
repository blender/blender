// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2017 Google Inc. All rights reserved.
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

#include "ceres/inner_product_computer.h"

#include <algorithm>
#include "ceres/small_blas.h"

namespace ceres {
namespace internal {


// Create the CompressedRowSparseMatrix matrix that will contain the
// inner product.
//
// storage_type controls whether the result matrix contains the upper
// or the lower triangular part of the product.
//
// num_nonzeros is the number of non-zeros in the result matrix.
CompressedRowSparseMatrix* InnerProductComputer::CreateResultMatrix(
    const CompressedRowSparseMatrix::StorageType storage_type,
    const int num_nonzeros) {
  CompressedRowSparseMatrix* matrix =
      new CompressedRowSparseMatrix(m_.num_cols(), m_.num_cols(), num_nonzeros);
  matrix->set_storage_type(storage_type);

  const CompressedRowBlockStructure* bs = m_.block_structure();
  const std::vector<Block>& blocks = bs->cols;
  matrix->mutable_row_blocks()->resize(blocks.size());
  matrix->mutable_col_blocks()->resize(blocks.size());
  for (int i = 0; i < blocks.size(); ++i) {
    (*(matrix->mutable_row_blocks()))[i] = blocks[i].size;
    (*(matrix->mutable_col_blocks()))[i] = blocks[i].size;
  }

  return matrix;
}

// Given the set of product terms in the inner product, return the
// total number of non-zeros in the result and for each row block of
// the result matrix, compute the number of non-zeros in any one row
// of the row block.
int InnerProductComputer::ComputeNonzeros(
    const std::vector<InnerProductComputer::ProductTerm>& product_terms,
    std::vector<int>* row_nnz) {
  const CompressedRowBlockStructure* bs = m_.block_structure();
  const std::vector<Block>& blocks = bs->cols;

  row_nnz->resize(blocks.size());
  std::fill(row_nnz->begin(), row_nnz->end(), 0);

  // First product term.
  (*row_nnz)[product_terms[0].row] = blocks[product_terms[0].col].size;
  int num_nonzeros =
      blocks[product_terms[0].row].size * blocks[product_terms[0].col].size;

  // Remaining product terms.
  for (int i = 1; i < product_terms.size(); ++i) {
    const ProductTerm& previous = product_terms[i - 1];
    const ProductTerm& current = product_terms[i];

    // Each (row, col) block counts only once.
    // This check depends on product sorted on (row, col).
    if (current.row != previous.row || current.col != previous.col) {
      (*row_nnz)[current.row] += blocks[current.col].size;
      num_nonzeros += blocks[current.row].size * blocks[current.col].size;
    }
  }

  return num_nonzeros;
}

InnerProductComputer::InnerProductComputer(const BlockSparseMatrix& m,
                                           const int start_row_block,
                                           const int end_row_block)
    : m_(m), start_row_block_(start_row_block), end_row_block_(end_row_block) {}

// Compute the sparsity structure of the product m.transpose() * m
// and create a CompressedRowSparseMatrix corresponding to it.
//
// Also compute the "program" vector, which for every term in the
// block outer product provides the information for the entry in the
// values array of the result matrix where it should be accumulated.
//
// Since the entries of the program are the same for rows with the
// same sparsity structure, the program only stores the result for one
// row per row block. The Compute function reuses this information for
// each row in the row block.
//
// product_storage_type controls the form of the output matrix. It
// can be LOWER_TRIANGULAR or UPPER_TRIANGULAR.
InnerProductComputer* InnerProductComputer::Create(
    const BlockSparseMatrix& m,
    CompressedRowSparseMatrix::StorageType product_storage_type) {
  return InnerProductComputer::Create(
      m, 0, m.block_structure()->rows.size(), product_storage_type);
}

InnerProductComputer* InnerProductComputer::Create(
    const BlockSparseMatrix& m,
    const int start_row_block,
    const int end_row_block,
    CompressedRowSparseMatrix::StorageType product_storage_type) {
  CHECK(product_storage_type == CompressedRowSparseMatrix::LOWER_TRIANGULAR ||
        product_storage_type == CompressedRowSparseMatrix::UPPER_TRIANGULAR);
  CHECK_GT(m.num_nonzeros(), 0)
      << "Congratulations, you found a bug in Ceres. Please report it.";
  InnerProductComputer* inner_product_computer =
      new InnerProductComputer(m, start_row_block, end_row_block);
  inner_product_computer->Init(product_storage_type);
  return inner_product_computer;
}

void InnerProductComputer::Init(
    const CompressedRowSparseMatrix::StorageType product_storage_type) {
  std::vector<InnerProductComputer::ProductTerm> product_terms;
  const CompressedRowBlockStructure* bs = m_.block_structure();

  // Give input matrix m in Block Sparse format
  //     (row_block, col_block)
  // represent each block multiplication
  //     (row_block, col_block1)' X (row_block, col_block2)
  // by its product term:
  //     (col_block1, col_block2, index)
  for (int row_block = start_row_block_; row_block < end_row_block_;
       ++row_block) {
    const CompressedRow& row = bs->rows[row_block];
    for (int c1 = 0; c1 < row.cells.size(); ++c1) {
      const Cell& cell1 = row.cells[c1];
      int c2_begin, c2_end;
      if (product_storage_type == CompressedRowSparseMatrix::LOWER_TRIANGULAR) {
        c2_begin = 0;
        c2_end = c1 + 1;
      } else {
        c2_begin = c1;
        c2_end = row.cells.size();
      }

      for (int c2 = c2_begin; c2 < c2_end; ++c2) {
        const Cell& cell2 = row.cells[c2];
        product_terms.push_back(InnerProductComputer::ProductTerm(
            cell1.block_id, cell2.block_id, product_terms.size()));
      }
    }
  }

  std::sort(product_terms.begin(), product_terms.end());
  ComputeOffsetsAndCreateResultMatrix(product_storage_type, product_terms);
}

void InnerProductComputer::ComputeOffsetsAndCreateResultMatrix(
    const CompressedRowSparseMatrix::StorageType product_storage_type,
    const std::vector<InnerProductComputer::ProductTerm>& product_terms) {
  const std::vector<Block>& col_blocks = m_.block_structure()->cols;

  std::vector<int> row_block_nnz;
  const int num_nonzeros = ComputeNonzeros(product_terms, &row_block_nnz);

  result_.reset(CreateResultMatrix(product_storage_type, num_nonzeros));

  // Populate the row non-zero counts in the result matrix.
  int* crsm_rows = result_->mutable_rows();
  crsm_rows[0] = 0;
  for (int i = 0; i < col_blocks.size(); ++i) {
    for (int j = 0; j < col_blocks[i].size; ++j, ++crsm_rows) {
      *(crsm_rows + 1) = *crsm_rows + row_block_nnz[i];
    }
  }

  // The following macro FILL_CRSM_COL_BLOCK is key to understanding
  // how this class works.
  //
  // It does two things.
  //
  // Sets the value for the current term in the result_offsets_ array
  // and populates the cols array of the result matrix.
  //
  // row_block and col_block as the names imply, refer to the row and
  // column blocks of the current term.
  //
  // row_nnz is the number of nonzeros in the result_matrix at the
  // beginning of the first row of row_block.
  //
  // col_nnz is the number of nonzeros in the first row of the row
  // block that occur before the current column block, i.e. this is
  // sum of the sizes of all the column blocks in this row block that
  // came before this column block.
  //
  // Given these two numbers and the total number of nonzeros in this
  // row (nnz_in_row), we can now populate the cols array as follows:
  //
  // nnz + j * nnz_in_row is the beginning of the j^th row.
  //
  // nnz + j * nnz_in_row + col_nnz is the beginning of the column
  // block in the j^th row.
  //
  // nnz + j * nnz_in_row + col_nnz + k is then the j^th row and the
  // k^th column of the product block, whose value is
  //
  // col_blocks[col_block].position + k, which is the column number of
  // the k^th column of the current column block.
#define FILL_CRSM_COL_BLOCK                                \
  const int row_block = current->row;                      \
  const int col_block = current->col;                      \
  const int nnz_in_row = row_block_nnz[row_block];         \
  int* crsm_cols = result_->mutable_cols();                \
  result_offsets_[current->index] = nnz + col_nnz;         \
  for (int j = 0; j < col_blocks[row_block].size; ++j) {   \
    for (int k = 0; k < col_blocks[col_block].size; ++k) { \
      crsm_cols[nnz + j * nnz_in_row + col_nnz + k] =      \
          col_blocks[col_block].position + k;              \
    }                                                      \
  }

  result_offsets_.resize(product_terms.size());
  int col_nnz = 0;
  int nnz = 0;

  // Process the first term.
  const InnerProductComputer::ProductTerm* current = &product_terms[0];
  FILL_CRSM_COL_BLOCK;

  // Process the rest of the terms.
  for (int i = 1; i < product_terms.size(); ++i) {
    current = &product_terms[i];
    const InnerProductComputer::ProductTerm* previous = &product_terms[i - 1];

    // If the current term is the same as the previous term, then it
    // stores its product at the same location as the previous term.
    if (previous->row == current->row && previous->col == current->col) {
      result_offsets_[current->index] = result_offsets_[previous->index];
      continue;
    }

    if (previous->row == current->row) {
      // if the current and previous terms are in the same row block,
      // then they differ in the column block, in which case advance
      // col_nnz by the column size of the prevous term.
      col_nnz += col_blocks[previous->col].size;
    } else {
      // If we have moved to a new row-block , then col_nnz is zero,
      // and nnz is set to the beginning of the row block.
      col_nnz = 0;
      nnz += row_block_nnz[previous->row] * col_blocks[previous->row].size;
    }

    FILL_CRSM_COL_BLOCK;
  }
}

// Use the results_offsets_ array to numerically compute the product
// m' * m and store it in result_.
//
// TODO(sameeragarwal): Multithreading support.
void InnerProductComputer::Compute() {
  const double* m_values = m_.values();
  const CompressedRowBlockStructure* bs = m_.block_structure();

  const CompressedRowSparseMatrix::StorageType storage_type =
      result_->storage_type();
  result_->SetZero();
  double* values = result_->mutable_values();
  const int* rows = result_->rows();
  int cursor = 0;

  // Iterate row blocks.
  for (int r = start_row_block_; r < end_row_block_; ++r) {
    const CompressedRow& m_row = bs->rows[r];
    for (int c1 = 0; c1 < m_row.cells.size(); ++c1) {
      const Cell& cell1 = m_row.cells[c1];
      const int c1_size = bs->cols[cell1.block_id].size;
      const int row_nnz = rows[bs->cols[cell1.block_id].position + 1] -
          rows[bs->cols[cell1.block_id].position];

      int c2_begin, c2_end;
      if (storage_type == CompressedRowSparseMatrix::LOWER_TRIANGULAR) {
        c2_begin = 0;
        c2_end = c1 + 1;
      } else {
        c2_begin = c1;
        c2_end = m_row.cells.size();
      }

      for (int c2 = c2_begin; c2 < c2_end; ++c2, ++cursor) {
        const Cell& cell2 = m_row.cells[c2];
        const int c2_size = bs->cols[cell2.block_id].size;
        MatrixTransposeMatrixMultiply<Eigen::Dynamic, Eigen::Dynamic,
                                      Eigen::Dynamic, Eigen::Dynamic, 1>(
                                          m_values + cell1.position,
                                          m_row.block.size, c1_size,
                                          m_values + cell2.position,
                                          m_row.block.size, c2_size,
                                          values + result_offsets_[cursor],
                                          0, 0, c1_size, row_nnz);
      }
    }
  }

  CHECK_EQ(cursor, result_offsets_.size());
}

}  // namespace internal
}  // namespace ceres
