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

#include "ceres/compressed_row_sparse_matrix.h"

#include <algorithm>
#include <numeric>
#include <vector>
#include "ceres/crs_matrix.h"
#include "ceres/internal/port.h"
#include "ceres/random.h"
#include "ceres/triplet_sparse_matrix.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

using std::vector;

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
  RowColLessThan(const int* rows, const int* cols) : rows(rows), cols(cols) {}

  bool operator()(const int x, const int y) const {
    if (rows[x] == rows[y]) {
      return (cols[x] < cols[y]);
    }
    return (rows[x] < rows[y]);
  }

  const int* rows;
  const int* cols;
};

void TransposeForCompressedRowSparseStructure(const int num_rows,
                                              const int num_cols,
                                              const int num_nonzeros,
                                              const int* rows,
                                              const int* cols,
                                              const double* values,
                                              int* transpose_rows,
                                              int* transpose_cols,
                                              double* transpose_values) {
  // Explicitly zero out transpose_rows.
  std::fill(transpose_rows, transpose_rows + num_cols + 1, 0);

  // Count the number of entries in each column of the original matrix
  // and assign to transpose_rows[col + 1].
  for (int idx = 0; idx < num_nonzeros; ++idx) {
    ++transpose_rows[cols[idx] + 1];
  }

  // Compute the starting position for each row in the transpose by
  // computing the cumulative sum of the entries of transpose_rows.
  for (int i = 1; i < num_cols + 1; ++i) {
    transpose_rows[i] += transpose_rows[i - 1];
  }

  // Populate transpose_cols and (optionally) transpose_values by
  // walking the entries of the source matrices. For each entry that
  // is added, the value of transpose_row is incremented allowing us
  // to keep track of where the next entry for that row should go.
  //
  // As a result transpose_row is shifted to the left by one entry.
  for (int r = 0; r < num_rows; ++r) {
    for (int idx = rows[r]; idx < rows[r + 1]; ++idx) {
      const int c = cols[idx];
      const int transpose_idx = transpose_rows[c]++;
      transpose_cols[transpose_idx] = r;
      if (values != NULL && transpose_values != NULL) {
        transpose_values[transpose_idx] = values[idx];
      }
    }
  }

  // This loop undoes the left shift to transpose_rows introduced by
  // the previous loop.
  for (int i = num_cols - 1; i > 0; --i) {
    transpose_rows[i] = transpose_rows[i - 1];
  }
  transpose_rows[0] = 0;
}

void AddRandomBlock(const int num_rows,
                    const int num_cols,
                    const int row_block_begin,
                    const int col_block_begin,
                    std::vector<int>* rows,
                    std::vector<int>* cols,
                    std::vector<double>* values) {
  for (int r = 0; r < num_rows; ++r) {
    for (int c = 0; c < num_cols; ++c) {
      rows->push_back(row_block_begin + r);
      cols->push_back(col_block_begin + c);
      values->push_back(RandNormal());
    }
  }
}

void AddSymmetricRandomBlock(const int num_rows,
                             const int row_block_begin,
                             std::vector<int>* rows,
                             std::vector<int>* cols,
                             std::vector<double>* values) {
  for (int r = 0; r < num_rows; ++r) {
    for (int c = r; c < num_rows; ++c) {
      const double v = RandNormal();
      rows->push_back(row_block_begin + r);
      cols->push_back(row_block_begin + c);
      values->push_back(v);
      if (r != c) {
        rows->push_back(row_block_begin + c);
        cols->push_back(row_block_begin + r);
        values->push_back(v);
      }
    }
  }
}

}  // namespace

// This constructor gives you a semi-initialized CompressedRowSparseMatrix.
CompressedRowSparseMatrix::CompressedRowSparseMatrix(int num_rows,
                                                     int num_cols,
                                                     int max_num_nonzeros) {
  num_rows_ = num_rows;
  num_cols_ = num_cols;
  storage_type_ = UNSYMMETRIC;
  rows_.resize(num_rows + 1, 0);
  cols_.resize(max_num_nonzeros, 0);
  values_.resize(max_num_nonzeros, 0.0);

  VLOG(1) << "# of rows: " << num_rows_ << " # of columns: " << num_cols_
          << " max_num_nonzeros: " << cols_.size() << ". Allocating "
          << (num_rows_ + 1) * sizeof(int) +     // NOLINT
                 cols_.size() * sizeof(int) +    // NOLINT
                 cols_.size() * sizeof(double);  // NOLINT
}

CompressedRowSparseMatrix* CompressedRowSparseMatrix::FromTripletSparseMatrix(
    const TripletSparseMatrix& input) {
  return CompressedRowSparseMatrix::FromTripletSparseMatrix(input, false);
}

CompressedRowSparseMatrix*
CompressedRowSparseMatrix::FromTripletSparseMatrixTransposed(
    const TripletSparseMatrix& input) {
  return CompressedRowSparseMatrix::FromTripletSparseMatrix(input, true);
}

CompressedRowSparseMatrix* CompressedRowSparseMatrix::FromTripletSparseMatrix(
    const TripletSparseMatrix& input, bool transpose) {
  int num_rows = input.num_rows();
  int num_cols = input.num_cols();
  const int* rows = input.rows();
  const int* cols = input.cols();
  const double* values = input.values();

  if (transpose) {
    std::swap(num_rows, num_cols);
    std::swap(rows, cols);
  }

  // index is the list of indices into the TripletSparseMatrix input.
  vector<int> index(input.num_nonzeros(), 0);
  for (int i = 0; i < input.num_nonzeros(); ++i) {
    index[i] = i;
  }

  // Sort index such that the entries of m are ordered by row and ties
  // are broken by column.
  std::sort(index.begin(), index.end(), RowColLessThan(rows, cols));

  VLOG(1) << "# of rows: " << num_rows << " # of columns: " << num_cols
          << " num_nonzeros: " << input.num_nonzeros() << ". Allocating "
          << ((num_rows + 1) * sizeof(int) +           // NOLINT
              input.num_nonzeros() * sizeof(int) +     // NOLINT
              input.num_nonzeros() * sizeof(double));  // NOLINT

  CompressedRowSparseMatrix* output =
      new CompressedRowSparseMatrix(num_rows, num_cols, input.num_nonzeros());

  if (num_rows == 0) {
    // No data to copy.
    return output;
  }

  // Copy the contents of the cols and values array in the order given
  // by index and count the number of entries in each row.
  int* output_rows = output->mutable_rows();
  int* output_cols = output->mutable_cols();
  double* output_values = output->mutable_values();

  output_rows[0] = 0;
  for (int i = 0; i < index.size(); ++i) {
    const int idx = index[i];
    ++output_rows[rows[idx] + 1];
    output_cols[i] = cols[idx];
    output_values[i] = values[idx];
  }

  // Find the cumulative sum of the row counts.
  for (int i = 1; i < num_rows + 1; ++i) {
    output_rows[i] += output_rows[i - 1];
  }

  CHECK_EQ(output->num_nonzeros(), input.num_nonzeros());
  return output;
}

CompressedRowSparseMatrix::CompressedRowSparseMatrix(const double* diagonal,
                                                     int num_rows) {
  CHECK(diagonal != nullptr);

  num_rows_ = num_rows;
  num_cols_ = num_rows;
  storage_type_ = UNSYMMETRIC;
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

CompressedRowSparseMatrix::~CompressedRowSparseMatrix() {}

void CompressedRowSparseMatrix::SetZero() {
  std::fill(values_.begin(), values_.end(), 0);
}

// TODO(sameeragarwal): Make RightMultiply and LeftMultiply
// block-aware for higher performance.
void CompressedRowSparseMatrix::RightMultiply(const double* x,
                                              double* y) const {
  CHECK(x != nullptr);
  CHECK(y != nullptr);

  if (storage_type_ == UNSYMMETRIC) {
    for (int r = 0; r < num_rows_; ++r) {
      for (int idx = rows_[r]; idx < rows_[r + 1]; ++idx) {
        const int c = cols_[idx];
        const double v = values_[idx];
        y[r] += v * x[c];
      }
    }
  } else if (storage_type_ == UPPER_TRIANGULAR) {
    // Because of their block structure, we will have entries that lie
    // above (below) the diagonal for lower (upper) triangular matrices,
    // so the loops below need to account for this.
    for (int r = 0; r < num_rows_; ++r) {
      int idx = rows_[r];
      const int idx_end = rows_[r + 1];

      // For upper triangular matrices r <= c, so skip entries with r
      // > c.
      while (idx < idx_end && r > cols_[idx]) {
        ++idx;
      }

      for (; idx < idx_end; ++idx) {
        const int c = cols_[idx];
        const double v = values_[idx];
        y[r] += v * x[c];
        // Since we are only iterating over the upper triangular part
        // of the matrix, add contributions for the strictly lower
        // triangular part.
        if (r != c) {
          y[c] += v * x[r];
        }
      }
    }
  } else if (storage_type_ == LOWER_TRIANGULAR) {
    for (int r = 0; r < num_rows_; ++r) {
      int idx = rows_[r];
      const int idx_end = rows_[r + 1];
      // For lower triangular matrices, we only iterate till we are r >=
      // c.
      for (; idx < idx_end && r >= cols_[idx]; ++idx) {
        const int c = cols_[idx];
        const double v = values_[idx];
        y[r] += v * x[c];
        // Since we are only iterating over the lower triangular part
        // of the matrix, add contributions for the strictly upper
        // triangular part.
        if (r != c) {
          y[c] += v * x[r];
        }
      }
    }
  } else {
    LOG(FATAL) << "Unknown storage type: " << storage_type_;
  }
}

void CompressedRowSparseMatrix::LeftMultiply(const double* x, double* y) const {
  CHECK(x != nullptr);
  CHECK(y != nullptr);

  if (storage_type_ == UNSYMMETRIC) {
    for (int r = 0; r < num_rows_; ++r) {
      for (int idx = rows_[r]; idx < rows_[r + 1]; ++idx) {
        y[cols_[idx]] += values_[idx] * x[r];
      }
    }
  } else {
    // Since the matrix is symmetric, LeftMultiply = RightMultiply.
    RightMultiply(x, y);
  }
}

void CompressedRowSparseMatrix::SquaredColumnNorm(double* x) const {
  CHECK(x != nullptr);

  std::fill(x, x + num_cols_, 0.0);
  if (storage_type_ == UNSYMMETRIC) {
    for (int idx = 0; idx < rows_[num_rows_]; ++idx) {
      x[cols_[idx]] += values_[idx] * values_[idx];
    }
  } else if (storage_type_ == UPPER_TRIANGULAR) {
    // Because of their block structure, we will have entries that lie
    // above (below) the diagonal for lower (upper) triangular
    // matrices, so the loops below need to account for this.
    for (int r = 0; r < num_rows_; ++r) {
      int idx = rows_[r];
      const int idx_end = rows_[r + 1];

      // For upper triangular matrices r <= c, so skip entries with r
      // > c.
      while (idx < idx_end && r > cols_[idx]) {
        ++idx;
      }

      for (; idx < idx_end; ++idx) {
        const int c = cols_[idx];
        const double v2 = values_[idx] * values_[idx];
        x[c] += v2;
        // Since we are only iterating over the upper triangular part
        // of the matrix, add contributions for the strictly lower
        // triangular part.
        if (r != c) {
          x[r] += v2;
        }
      }
    }
  } else if (storage_type_ == LOWER_TRIANGULAR) {
    for (int r = 0; r < num_rows_; ++r) {
      int idx = rows_[r];
      const int idx_end = rows_[r + 1];
      // For lower triangular matrices, we only iterate till we are r >=
      // c.
      for (; idx < idx_end && r >= cols_[idx]; ++idx) {
        const int c = cols_[idx];
        const double v2 = values_[idx] * values_[idx];
        x[c] += v2;
        // Since we are only iterating over the lower triangular part
        // of the matrix, add contributions for the strictly upper
        // triangular part.
        if (r != c) {
          x[r] += v2;
        }
      }
    }
  } else {
    LOG(FATAL) << "Unknown storage type: " << storage_type_;
  }
}
void CompressedRowSparseMatrix::ScaleColumns(const double* scale) {
  CHECK(scale != nullptr);

  for (int idx = 0; idx < rows_[num_rows_]; ++idx) {
    values_[idx] *= scale[cols_[idx]];
  }
}

void CompressedRowSparseMatrix::ToDenseMatrix(Matrix* dense_matrix) const {
  CHECK(dense_matrix != nullptr);
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
  CHECK_EQ(storage_type_, UNSYMMETRIC);

  num_rows_ -= delta_rows;
  rows_.resize(num_rows_ + 1);

  // The rest of the code updates the block information. Immediately
  // return in case of no block information.
  if (row_blocks_.empty()) {
    return;
  }

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
  CHECK_EQ(storage_type_, UNSYMMETRIC);
  CHECK_EQ(m.num_cols(), num_cols_);

  CHECK((row_blocks_.empty() && m.row_blocks().empty()) ||
        (!row_blocks_.empty() && !m.row_blocks().empty()))
      << "Cannot append a matrix with row blocks to one without and vice versa."
      << "This matrix has : " << row_blocks_.size() << " row blocks."
      << "The matrix being appended has: " << m.row_blocks().size()
      << " row blocks.";

  if (m.num_rows() == 0) {
    return;
  }

  if (cols_.size() < num_nonzeros() + m.num_nonzeros()) {
    cols_.resize(num_nonzeros() + m.num_nonzeros());
    values_.resize(num_nonzeros() + m.num_nonzeros());
  }

  // Copy the contents of m into this matrix.
  DCHECK_LT(num_nonzeros(), cols_.size());
  if (m.num_nonzeros() > 0) {
    std::copy(m.cols(), m.cols() + m.num_nonzeros(), &cols_[num_nonzeros()]);
    std::copy(
        m.values(), m.values() + m.num_nonzeros(), &values_[num_nonzeros()]);
  }

  rows_.resize(num_rows_ + m.num_rows() + 1);
  // new_rows = [rows_, m.row() + rows_[num_rows_]]
  std::fill(rows_.begin() + num_rows_,
            rows_.begin() + num_rows_ + m.num_rows() + 1,
            rows_[num_rows_]);

  for (int r = 0; r < m.num_rows() + 1; ++r) {
    rows_[num_rows_ + r] += m.rows()[r];
  }

  num_rows_ += m.num_rows();

  // The rest of the code updates the block information. Immediately
  // return in case of no block information.
  if (row_blocks_.empty()) {
    return;
  }

  row_blocks_.insert(
      row_blocks_.end(), m.row_blocks().begin(), m.row_blocks().end());
}

void CompressedRowSparseMatrix::ToTextFile(FILE* file) const {
  CHECK(file != nullptr);
  for (int r = 0; r < num_rows_; ++r) {
    for (int idx = rows_[r]; idx < rows_[r + 1]; ++idx) {
      fprintf(file, "% 10d % 10d %17f\n", r, cols_[idx], values_[idx]);
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

void CompressedRowSparseMatrix::SetMaxNumNonZeros(int num_nonzeros) {
  CHECK_GE(num_nonzeros, 0);

  cols_.resize(num_nonzeros);
  values_.resize(num_nonzeros);
}

CompressedRowSparseMatrix* CompressedRowSparseMatrix::CreateBlockDiagonalMatrix(
    const double* diagonal, const vector<int>& blocks) {
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
  std::fill(values, values + num_nonzeros, 0.0);

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

  switch (storage_type_) {
    case UNSYMMETRIC:
      transpose->set_storage_type(UNSYMMETRIC);
      break;
    case LOWER_TRIANGULAR:
      transpose->set_storage_type(UPPER_TRIANGULAR);
      break;
    case UPPER_TRIANGULAR:
      transpose->set_storage_type(LOWER_TRIANGULAR);
      break;
    default:
      LOG(FATAL) << "Unknown storage type: " << storage_type_;
  };

  TransposeForCompressedRowSparseStructure(num_rows(),
                                           num_cols(),
                                           num_nonzeros(),
                                           rows(),
                                           cols(),
                                           values(),
                                           transpose->mutable_rows(),
                                           transpose->mutable_cols(),
                                           transpose->mutable_values());

  // The rest of the code updates the block information. Immediately
  // return in case of no block information.
  if (row_blocks_.empty()) {
    return transpose;
  }

  *(transpose->mutable_row_blocks()) = col_blocks_;
  *(transpose->mutable_col_blocks()) = row_blocks_;
  return transpose;
}

CompressedRowSparseMatrix* CompressedRowSparseMatrix::CreateRandomMatrix(
    CompressedRowSparseMatrix::RandomMatrixOptions options) {
  CHECK_GT(options.num_row_blocks, 0);
  CHECK_GT(options.min_row_block_size, 0);
  CHECK_GT(options.max_row_block_size, 0);
  CHECK_LE(options.min_row_block_size, options.max_row_block_size);

  if (options.storage_type == UNSYMMETRIC) {
    CHECK_GT(options.num_col_blocks, 0);
    CHECK_GT(options.min_col_block_size, 0);
    CHECK_GT(options.max_col_block_size, 0);
    CHECK_LE(options.min_col_block_size, options.max_col_block_size);
  } else {
    // Symmetric matrices (LOWER_TRIANGULAR or UPPER_TRIANGULAR);
    options.num_col_blocks = options.num_row_blocks;
    options.min_col_block_size = options.min_row_block_size;
    options.max_col_block_size = options.max_row_block_size;
  }

  CHECK_GT(options.block_density, 0.0);
  CHECK_LE(options.block_density, 1.0);

  vector<int> row_blocks;
  vector<int> col_blocks;

  // Generate the row block structure.
  for (int i = 0; i < options.num_row_blocks; ++i) {
    // Generate a random integer in [min_row_block_size, max_row_block_size]
    const int delta_block_size =
        Uniform(options.max_row_block_size - options.min_row_block_size);
    row_blocks.push_back(options.min_row_block_size + delta_block_size);
  }

  if (options.storage_type == UNSYMMETRIC) {
    // Generate the col block structure.
    for (int i = 0; i < options.num_col_blocks; ++i) {
      // Generate a random integer in [min_col_block_size, max_col_block_size]
      const int delta_block_size =
          Uniform(options.max_col_block_size - options.min_col_block_size);
      col_blocks.push_back(options.min_col_block_size + delta_block_size);
    }
  } else {
    // Symmetric matrices (LOWER_TRIANGULAR or UPPER_TRIANGULAR);
    col_blocks = row_blocks;
  }

  vector<int> tsm_rows;
  vector<int> tsm_cols;
  vector<double> tsm_values;

  // For ease of construction, we are going to generate the
  // CompressedRowSparseMatrix by generating it as a
  // TripletSparseMatrix and then converting it to a
  // CompressedRowSparseMatrix.

  // It is possible that the random matrix is empty which is likely
  // not what the user wants, so do the matrix generation till we have
  // at least one non-zero entry.
  while (tsm_values.empty()) {
    tsm_rows.clear();
    tsm_cols.clear();
    tsm_values.clear();

    int row_block_begin = 0;
    for (int r = 0; r < options.num_row_blocks; ++r) {
      int col_block_begin = 0;
      for (int c = 0; c < options.num_col_blocks; ++c) {
        if (((options.storage_type == UPPER_TRIANGULAR) && (r > c)) ||
            ((options.storage_type == LOWER_TRIANGULAR) && (r < c))) {
          col_block_begin += col_blocks[c];
          continue;
        }

        // Randomly determine if this block is present or not.
        if (RandDouble() <= options.block_density) {
          // If the matrix is symmetric, then we take care to generate
          // symmetric diagonal blocks.
          if (options.storage_type == UNSYMMETRIC || r != c) {
            AddRandomBlock(row_blocks[r],
                           col_blocks[c],
                           row_block_begin,
                           col_block_begin,
                           &tsm_rows,
                           &tsm_cols,
                           &tsm_values);
          } else {
            AddSymmetricRandomBlock(row_blocks[r],
                                    row_block_begin,
                                    &tsm_rows,
                                    &tsm_cols,
                                    &tsm_values);
          }
        }
        col_block_begin += col_blocks[c];
      }
      row_block_begin += row_blocks[r];
    }
  }

  const int num_rows = std::accumulate(row_blocks.begin(), row_blocks.end(), 0);
  const int num_cols = std::accumulate(col_blocks.begin(), col_blocks.end(), 0);
  const bool kDoNotTranspose = false;
  CompressedRowSparseMatrix* matrix =
      CompressedRowSparseMatrix::FromTripletSparseMatrix(
          TripletSparseMatrix(
              num_rows, num_cols, tsm_rows, tsm_cols, tsm_values),
          kDoNotTranspose);
  (*matrix->mutable_row_blocks()) = row_blocks;
  (*matrix->mutable_col_blocks()) = col_blocks;
  matrix->set_storage_type(options.storage_type);
  return matrix;
}

}  // namespace internal
}  // namespace ceres
