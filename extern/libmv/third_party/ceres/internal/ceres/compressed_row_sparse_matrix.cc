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

#include "ceres/compressed_row_sparse_matrix.h"

#include <algorithm>
#include <numeric>
#include <vector>
#include "ceres/crs_matrix.h"
#include "ceres/internal/port.h"
#include "ceres/triplet_sparse_matrix.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {
namespace {

// Helper functor used by the constructor for reordering the contents
// of a TripletSparseMatrix. This comparator assumes thay there are no
// duplicates in the pair of arrays rows and cols, i.e., there is no
// indices i and j (not equal to each other) s.t.
//
//  rows[i] == rows[j] && cols[i] == cols[j]
//
// If this is the case, this functor will not be a StrictWeakOrdering.
struct RowColLessThan {
  RowColLessThan(const int* rows, const int* cols)
      : rows(rows), cols(cols) {
  }

  bool operator()(const int x, const int y) const {
    if (rows[x] == rows[y]) {
      return (cols[x] < cols[y]);
    }
    return (rows[x] < rows[y]);
  }

  const int* rows;
  const int* cols;
};

}  // namespace

// This constructor gives you a semi-initialized CompressedRowSparseMatrix.
CompressedRowSparseMatrix::CompressedRowSparseMatrix(int num_rows,
                                                     int num_cols,
                                                     int max_num_nonzeros) {
  num_rows_ = num_rows;
  num_cols_ = num_cols;
  rows_.resize(num_rows + 1, 0);
  cols_.resize(max_num_nonzeros, 0);
  values_.resize(max_num_nonzeros, 0.0);


  VLOG(1) << "# of rows: " << num_rows_
          << " # of columns: " << num_cols_
          << " max_num_nonzeros: " << cols_.size()
          << ". Allocating " << (num_rows_ + 1) * sizeof(int) +  // NOLINT
      cols_.size() * sizeof(int) +  // NOLINT
      cols_.size() * sizeof(double);  // NOLINT
}

CompressedRowSparseMatrix::CompressedRowSparseMatrix(
    const TripletSparseMatrix& m) {
  num_rows_ = m.num_rows();
  num_cols_ = m.num_cols();

  rows_.resize(num_rows_ + 1, 0);
  cols_.resize(m.num_nonzeros(), 0);
  values_.resize(m.max_num_nonzeros(), 0.0);

  // index is the list of indices into the TripletSparseMatrix m.
  vector<int> index(m.num_nonzeros(), 0);
  for (int i = 0; i < m.num_nonzeros(); ++i) {
    index[i] = i;
  }

  // Sort index such that the entries of m are ordered by row and ties
  // are broken by column.
  sort(index.begin(), index.end(), RowColLessThan(m.rows(), m.cols()));

  VLOG(1) << "# of rows: " << num_rows_
          << " # of columns: " << num_cols_
          << " max_num_nonzeros: " << cols_.size()
          << ". Allocating "
          << ((num_rows_ + 1) * sizeof(int) +  // NOLINT
              cols_.size() * sizeof(int) +     // NOLINT
              cols_.size() * sizeof(double));  // NOLINT

  // Copy the contents of the cols and values array in the order given
  // by index and count the number of entries in each row.
  for (int i = 0; i < m.num_nonzeros(); ++i) {
    const int idx = index[i];
    ++rows_[m.rows()[idx] + 1];
    cols_[i] = m.cols()[idx];
    values_[i] = m.values()[idx];
  }

  // Find the cumulative sum of the row counts.
  for (int i = 1; i < num_rows_ + 1; ++i) {
    rows_[i] += rows_[i - 1];
  }

  CHECK_EQ(num_nonzeros(), m.num_nonzeros());
}

CompressedRowSparseMatrix::CompressedRowSparseMatrix(const double* diagonal,
                                                     int num_rows) {
  CHECK_NOTNULL(diagonal);

  num_rows_ = num_rows;
  num_cols_ = num_rows;
  rows_.resize(num_rows + 1);
  cols_.resize(num_rows);
  values_.resize(num_rows);

  rows_[0] = 0;
  for (int i = 0; i < num_rows_; ++i) {
    cols_[i] = i;
    values_[i] = diagonal[i];
    rows_[i + 1] = i + 1;
  }

  CHECK_EQ(num_nonzeros(), num_rows);
}

CompressedRowSparseMatrix::~CompressedRowSparseMatrix() {
}

void CompressedRowSparseMatrix::SetZero() {
  fill(values_.begin(), values_.end(), 0);
}

void CompressedRowSparseMatrix::RightMultiply(const double* x,
                                              double* y) const {
  CHECK_NOTNULL(x);
  CHECK_NOTNULL(y);

  for (int r = 0; r < num_rows_; ++r) {
    for (int idx = rows_[r]; idx < rows_[r + 1]; ++idx) {
      y[r] += values_[idx] * x[cols_[idx]];
    }
  }
}

void CompressedRowSparseMatrix::LeftMultiply(const double* x, double* y) const {
  CHECK_NOTNULL(x);
  CHECK_NOTNULL(y);

  for (int r = 0; r < num_rows_; ++r) {
    for (int idx = rows_[r]; idx < rows_[r + 1]; ++idx) {
      y[cols_[idx]] += values_[idx] * x[r];
    }
  }
}

void CompressedRowSparseMatrix::SquaredColumnNorm(double* x) const {
  CHECK_NOTNULL(x);

  fill(x, x + num_cols_, 0.0);
  for (int idx = 0; idx < rows_[num_rows_]; ++idx) {
    x[cols_[idx]] += values_[idx] * values_[idx];
  }
}

void CompressedRowSparseMatrix::ScaleColumns(const double* scale) {
  CHECK_NOTNULL(scale);

  for (int idx = 0; idx < rows_[num_rows_]; ++idx) {
    values_[idx] *= scale[cols_[idx]];
  }
}

void CompressedRowSparseMatrix::ToDenseMatrix(Matrix* dense_matrix) const {
  CHECK_NOTNULL(dense_matrix);
  dense_matrix->resize(num_rows_, num_cols_);
  dense_matrix->setZero();

  for (int r = 0; r < num_rows_; ++r) {
    for (int idx = rows_[r]; idx < rows_[r + 1]; ++idx) {
      (*dense_matrix)(r, cols_[idx]) = values_[idx];
    }
  }
}

void CompressedRowSparseMatrix::DeleteRows(int delta_rows) {
  CHECK_GE(delta_rows, 0);
  CHECK_LE(delta_rows, num_rows_);

  num_rows_ -= delta_rows;
  rows_.resize(num_rows_ + 1);

  // Walk the list of row blocks until we reach the new number of rows
  // and the drop the rest of the row blocks.
  int num_row_blocks = 0;
  int num_rows = 0;
  while (num_row_blocks < row_blocks_.size() && num_rows < num_rows_) {
    num_rows += row_blocks_[num_row_blocks];
    ++num_row_blocks;
  }

  row_blocks_.resize(num_row_blocks);
}

void CompressedRowSparseMatrix::AppendRows(const CompressedRowSparseMatrix& m) {
  CHECK_EQ(m.num_cols(), num_cols_);

  CHECK(row_blocks_.size() == 0 || m.row_blocks().size() !=0)
      << "Cannot append a matrix with row blocks to one without and vice versa."
      << "This matrix has : " << row_blocks_.size() << " row blocks."
      << "The matrix being appended has: " << m.row_blocks().size()
      << " row blocks.";

  if (cols_.size() < num_nonzeros() + m.num_nonzeros()) {
    cols_.resize(num_nonzeros() + m.num_nonzeros());
    values_.resize(num_nonzeros() + m.num_nonzeros());
  }

  // Copy the contents of m into this matrix.
  copy(m.cols(), m.cols() + m.num_nonzeros(), &cols_[num_nonzeros()]);
  copy(m.values(), m.values() + m.num_nonzeros(), &values_[num_nonzeros()]);
  rows_.resize(num_rows_ + m.num_rows() + 1);
  // new_rows = [rows_, m.row() + rows_[num_rows_]]
  fill(rows_.begin() + num_rows_,
       rows_.begin() + num_rows_ + m.num_rows() + 1,
       rows_[num_rows_]);

  for (int r = 0; r < m.num_rows() + 1; ++r) {
    rows_[num_rows_ + r] += m.rows()[r];
  }

  num_rows_ += m.num_rows();
  row_blocks_.insert(row_blocks_.end(), m.row_blocks().begin(), m.row_blocks().end());
}

void CompressedRowSparseMatrix::ToTextFile(FILE* file) const {
  CHECK_NOTNULL(file);
  for (int r = 0; r < num_rows_; ++r) {
    for (int idx = rows_[r]; idx < rows_[r + 1]; ++idx) {
      fprintf(file,
              "% 10d % 10d %17f\n",
              r,
              cols_[idx],
              values_[idx]);
    }
  }
}

void CompressedRowSparseMatrix::ToCRSMatrix(CRSMatrix* matrix) const {
  matrix->num_rows = num_rows_;
  matrix->num_cols = num_cols_;
  matrix->rows = rows_;
  matrix->cols = cols_;
  matrix->values = values_;

  // Trim.
  matrix->rows.resize(matrix->num_rows + 1);
  matrix->cols.resize(matrix->rows[matrix->num_rows]);
  matrix->values.resize(matrix->rows[matrix->num_rows]);
}

void CompressedRowSparseMatrix::SolveLowerTriangularInPlace(
    double* solution) const {
  for (int r = 0; r < num_rows_; ++r) {
    for (int idx = rows_[r]; idx < rows_[r + 1] - 1; ++idx) {
      solution[r] -= values_[idx] * solution[cols_[idx]];
    }
    solution[r] /=  values_[rows_[r + 1] - 1];
  }
}

void CompressedRowSparseMatrix::SolveLowerTriangularTransposeInPlace(
    double* solution) const {
  for (int r = num_rows_ - 1; r >= 0; --r) {
    solution[r] /= values_[rows_[r + 1] - 1];
    for (int idx = rows_[r + 1] - 2; idx >= rows_[r]; --idx) {
      solution[cols_[idx]] -= values_[idx] * solution[r];
    }
  }
}

CompressedRowSparseMatrix* CompressedRowSparseMatrix::CreateBlockDiagonalMatrix(
    const double* diagonal,
    const vector<int>& blocks) {
  int num_rows = 0;
  int num_nonzeros = 0;
  for (int i = 0; i < blocks.size(); ++i) {
    num_rows += blocks[i];
    num_nonzeros += blocks[i] * blocks[i];
  }

  CompressedRowSparseMatrix* matrix =
      new CompressedRowSparseMatrix(num_rows, num_rows, num_nonzeros);

  int* rows = matrix->mutable_rows();
  int* cols = matrix->mutable_cols();
  double* values = matrix->mutable_values();
  fill(values, values + num_nonzeros, 0.0);

  int idx_cursor = 0;
  int col_cursor = 0;
  for (int i = 0; i < blocks.size(); ++i) {
    const int block_size = blocks[i];
    for (int r = 0; r < block_size; ++r) {
      *(rows++) = idx_cursor;
      values[idx_cursor + r] = diagonal[col_cursor + r];
      for (int c = 0; c < block_size; ++c, ++idx_cursor) {
        *(cols++) = col_cursor + c;
      }
    }
    col_cursor += block_size;
  }
  *rows = idx_cursor;

  *matrix->mutable_row_blocks() = blocks;
  *matrix->mutable_col_blocks() = blocks;

  CHECK_EQ(idx_cursor, num_nonzeros);
  CHECK_EQ(col_cursor, num_rows);
  return matrix;
}

CompressedRowSparseMatrix* CompressedRowSparseMatrix::Transpose() const {
  CompressedRowSparseMatrix* transpose =
      new CompressedRowSparseMatrix(num_cols_, num_rows_, num_nonzeros());

  int* transpose_rows = transpose->mutable_rows();
  int* transpose_cols = transpose->mutable_cols();
  double* transpose_values = transpose->mutable_values();

  for (int idx = 0; idx < num_nonzeros(); ++idx) {
    ++transpose_rows[cols_[idx] + 1];
  }

  for (int i = 1; i < transpose->num_rows() + 1; ++i) {
    transpose_rows[i] += transpose_rows[i - 1];
  }

  for (int r = 0; r < num_rows(); ++r) {
    for (int idx = rows_[r]; idx < rows_[r + 1]; ++idx) {
      const int c = cols_[idx];
      const int transpose_idx = transpose_rows[c]++;
      transpose_cols[transpose_idx] = r;
      transpose_values[transpose_idx] = values_[idx];
    }
  }

  for (int i = transpose->num_rows() - 1; i > 0 ; --i) {
    transpose_rows[i] = transpose_rows[i - 1];
  }
  transpose_rows[0] = 0;

  return transpose;
}

namespace {
// A ProductTerm is a term in the outer product of a matrix with
// itself.
struct ProductTerm {
  ProductTerm(const int row, const int col, const int index)
      : row(row), col(col), index(index) {
  }

  bool operator<(const ProductTerm& right) const {
    if (row == right.row) {
      if (col == right.col) {
        return index < right.index;
      }
      return col < right.col;
    }
    return row < right.row;
  }

  int row;
  int col;
  int index;
};

CompressedRowSparseMatrix*
CompressAndFillProgram(const int num_rows,
                       const int num_cols,
                       const vector<ProductTerm>& product,
                       vector<int>* program) {
  CHECK_GT(product.size(), 0);

  // Count the number of unique product term, which in turn is the
  // number of non-zeros in the outer product.
  int num_nonzeros = 1;
  for (int i = 1; i < product.size(); ++i) {
    if (product[i].row != product[i - 1].row ||
        product[i].col != product[i - 1].col) {
      ++num_nonzeros;
    }
  }

  CompressedRowSparseMatrix* matrix =
      new CompressedRowSparseMatrix(num_rows, num_cols, num_nonzeros);

  int* crsm_rows = matrix->mutable_rows();
  std::fill(crsm_rows, crsm_rows + num_rows + 1, 0);
  int* crsm_cols = matrix->mutable_cols();
  std::fill(crsm_cols, crsm_cols + num_nonzeros, 0);

  CHECK_NOTNULL(program)->clear();
  program->resize(product.size());

  // Iterate over the sorted product terms. This means each row is
  // filled one at a time, and we are able to assign a position in the
  // values array to each term.
  //
  // If terms repeat, i.e., they contribute to the same entry in the
  // result matrix), then they do not affect the sparsity structure of
  // the result matrix.
  int nnz = 0;
  crsm_cols[0] = product[0].col;
  crsm_rows[product[0].row + 1]++;
  (*program)[product[0].index] = nnz;
  for (int i = 1; i < product.size(); ++i) {
    const ProductTerm& previous = product[i - 1];
    const ProductTerm& current = product[i];

    // Sparsity structure is updated only if the term is not a repeat.
    if (previous.row != current.row || previous.col != current.col) {
      crsm_cols[++nnz] = current.col;
      crsm_rows[current.row + 1]++;
    }

    // All terms get assigned the position in the values array where
    // their value is accumulated.
    (*program)[current.index] = nnz;
  }

  for (int i = 1; i < num_rows + 1; ++i) {
    crsm_rows[i] += crsm_rows[i - 1];
  }

  return matrix;
}

}  // namespace

CompressedRowSparseMatrix*
CompressedRowSparseMatrix::CreateOuterProductMatrixAndProgram(
      const CompressedRowSparseMatrix& m,
      vector<int>* program) {
  CHECK_NOTNULL(program)->clear();
  CHECK_GT(m.num_nonzeros(), 0) << "Congratulations, "
                                << "you found a bug in Ceres. Please report it.";

  vector<ProductTerm> product;
  const vector<int>& row_blocks = m.row_blocks();
  int row_block_begin = 0;
  // Iterate over row blocks
  for (int row_block = 0; row_block < row_blocks.size(); ++row_block) {
    const int row_block_end = row_block_begin + row_blocks[row_block];
    // Compute the outer product terms for just one row per row block.
    const int r = row_block_begin;
    // Compute the lower triangular part of the product.
    for (int idx1 = m.rows()[r]; idx1 < m.rows()[r + 1]; ++idx1) {
      for (int idx2 = m.rows()[r]; idx2 <= idx1; ++idx2) {
        product.push_back(ProductTerm(m.cols()[idx1], m.cols()[idx2], product.size()));
      }
    }
    row_block_begin = row_block_end;
  }
  CHECK_EQ(row_block_begin, m.num_rows());
  sort(product.begin(), product.end());
  return CompressAndFillProgram(m.num_cols(), m.num_cols(), product, program);
}

void CompressedRowSparseMatrix::ComputeOuterProduct(
    const CompressedRowSparseMatrix& m,
    const vector<int>& program,
    CompressedRowSparseMatrix* result) {
  result->SetZero();
  double* values = result->mutable_values();
  const vector<int>& row_blocks = m.row_blocks();

  int cursor = 0;
  int row_block_begin = 0;
  const double* m_values = m.values();
  const int* m_rows = m.rows();
  // Iterate over row blocks.
  for (int row_block = 0; row_block < row_blocks.size(); ++row_block) {
    const int row_block_end = row_block_begin + row_blocks[row_block];
    const int saved_cursor = cursor;
    for (int r = row_block_begin; r < row_block_end; ++r) {
      // Reuse the program segment for each row in this row block.
      cursor = saved_cursor;
      const int row_begin = m_rows[r];
      const int row_end = m_rows[r + 1];
      for (int idx1 = row_begin; idx1 < row_end; ++idx1) {
        const double v1 =  m_values[idx1];
        for (int idx2 = row_begin; idx2 <= idx1; ++idx2, ++cursor) {
          values[program[cursor]] += v1 * m_values[idx2];
        }
      }
    }
    row_block_begin = row_block_end;
  }

  CHECK_EQ(row_block_begin, m.num_rows());
  CHECK_EQ(cursor, program.size());
}

}  // namespace internal
}  // namespace ceres
