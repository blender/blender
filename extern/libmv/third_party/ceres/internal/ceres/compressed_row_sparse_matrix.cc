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
#include <vector>
#include "ceres/matrix_proto.h"
#include "ceres/internal/port.h"

namespace ceres {
namespace internal {
namespace {

// Helper functor used by the constructor for reordering the contents
// of a TripletSparseMatrix.
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
  max_num_nonzeros_ = max_num_nonzeros;

  VLOG(1) << "# of rows: " << num_rows_ << " # of columns: " << num_cols_
          << " max_num_nonzeros: " << max_num_nonzeros_
          << ". Allocating " << (num_rows_ + 1) * sizeof(int) +  // NOLINT
      max_num_nonzeros_ * sizeof(int) +  // NOLINT
      max_num_nonzeros_ * sizeof(double);  // NOLINT

  rows_.reset(new int[num_rows_ + 1]);
  cols_.reset(new int[max_num_nonzeros_]);
  values_.reset(new double[max_num_nonzeros_]);

  fill(rows_.get(), rows_.get() + num_rows_ + 1, 0);
  fill(cols_.get(), cols_.get() + max_num_nonzeros_, 0);
  fill(values_.get(), values_.get() + max_num_nonzeros_, 0);
}

CompressedRowSparseMatrix::CompressedRowSparseMatrix(
    const TripletSparseMatrix& m) {
  num_rows_ = m.num_rows();
  num_cols_ = m.num_cols();
  max_num_nonzeros_ = m.max_num_nonzeros();

  // index is the list of indices into the TripletSparseMatrix m.
  vector<int> index(m.num_nonzeros(), 0);
  for (int i = 0; i < m.num_nonzeros(); ++i) {
    index[i] = i;
  }

  // Sort index such that the entries of m are ordered by row and ties
  // are broken by column.
  sort(index.begin(), index.end(), RowColLessThan(m.rows(), m.cols()));

  VLOG(1) << "# of rows: " << num_rows_ << " # of columns: " << num_cols_
          << " max_num_nonzeros: " << max_num_nonzeros_
          << ". Allocating " << (num_rows_ + 1) * sizeof(int) +  // NOLINT
      max_num_nonzeros_ * sizeof(int) +  // NOLINT
      max_num_nonzeros_ * sizeof(double);  // NOLINT

  rows_.reset(new int[num_rows_ + 1]);
  cols_.reset(new int[max_num_nonzeros_]);
  values_.reset(new double[max_num_nonzeros_]);

  // rows_ = 0
  fill(rows_.get(), rows_.get() + num_rows_ + 1, 0);

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
    rows_[i] += rows_[i-1];
  }

  CHECK_EQ(num_nonzeros(), m.num_nonzeros());
}

#ifndef CERES_DONT_HAVE_PROTOCOL_BUFFERS
CompressedRowSparseMatrix::CompressedRowSparseMatrix(
    const SparseMatrixProto& outer_proto) {
  CHECK(outer_proto.has_compressed_row_matrix());

  const CompressedRowSparseMatrixProto& proto =
      outer_proto.compressed_row_matrix();

  num_rows_ = proto.num_rows();
  num_cols_ = proto.num_cols();

  rows_.reset(new int[proto.rows_size()]);
  cols_.reset(new int[proto.cols_size()]);
  values_.reset(new double[proto.values_size()]);

  for (int i = 0; i < proto.rows_size(); ++i) {
    rows_[i] = proto.rows(i);
  }

  CHECK_EQ(proto.rows_size(), num_rows_ + 1);
  CHECK_EQ(proto.cols_size(), proto.values_size());
  CHECK_EQ(proto.cols_size(), rows_[num_rows_]);

  for (int i = 0; i < proto.cols_size(); ++i) {
    cols_[i] = proto.cols(i);
    values_[i] = proto.values(i);
  }

  max_num_nonzeros_ = proto.cols_size();
}
#endif

CompressedRowSparseMatrix::CompressedRowSparseMatrix(const double* diagonal,
                                                     int num_rows) {
  CHECK_NOTNULL(diagonal);

  num_rows_ = num_rows;
  num_cols_ = num_rows;
  max_num_nonzeros_ = num_rows;

  rows_.reset(new int[num_rows_ + 1]);
  cols_.reset(new int[num_rows_]);
  values_.reset(new double[num_rows_]);

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
  fill(values_.get(), values_.get() + num_nonzeros(), 0.0);
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

#ifndef CERES_DONT_HAVE_PROTOCOL_BUFFERS
void CompressedRowSparseMatrix::ToProto(SparseMatrixProto* outer_proto) const {
  CHECK_NOTNULL(outer_proto);

  outer_proto->Clear();
  CompressedRowSparseMatrixProto* proto
      = outer_proto->mutable_compressed_row_matrix();

  proto->set_num_rows(num_rows_);
  proto->set_num_cols(num_cols_);

  for (int r = 0; r < num_rows_ + 1; ++r) {
    proto->add_rows(rows_[r]);
  }

  for (int idx = 0; idx < rows_[num_rows_]; ++idx) {
    proto->add_cols(cols_[idx]);
    proto->add_values(values_[idx]);
  }
}
#endif

void CompressedRowSparseMatrix::DeleteRows(int delta_rows) {
  CHECK_GE(delta_rows, 0);
  CHECK_LE(delta_rows, num_rows_);

  int new_num_rows = num_rows_ - delta_rows;

  num_rows_ = new_num_rows;
  int* new_rows = new int[num_rows_ + 1];
  copy(rows_.get(), rows_.get() + num_rows_ + 1, new_rows);
  rows_.reset(new_rows);
}

void CompressedRowSparseMatrix::AppendRows(const CompressedRowSparseMatrix& m) {
  CHECK_EQ(m.num_cols(), num_cols_);

  // Check if there is enough space. If not, then allocate new arrays
  // to hold the combined matrix and copy the contents of this matrix
  // into it.
  if (max_num_nonzeros_ < num_nonzeros() + m.num_nonzeros()) {
    int new_max_num_nonzeros =  num_nonzeros() + m.num_nonzeros();

    VLOG(1) << "Reallocating " << sizeof(int) * new_max_num_nonzeros;  // NOLINT

    int* new_cols = new int[new_max_num_nonzeros];
    copy(cols_.get(), cols_.get() + max_num_nonzeros_, new_cols);
    cols_.reset(new_cols);

    double* new_values = new double[new_max_num_nonzeros];
    copy(values_.get(), values_.get() + max_num_nonzeros_, new_values);
    values_.reset(new_values);

    max_num_nonzeros_ = new_max_num_nonzeros;
  }

  // Copy the contents of m into this matrix.
  copy(m.cols(), m.cols() + m.num_nonzeros(), cols_.get() + num_nonzeros());
  copy(m.values(),
       m.values() + m.num_nonzeros(),
       values_.get() + num_nonzeros());

  // Create the new rows array to hold the enlarged matrix.
  int* new_rows = new int[num_rows_ + m.num_rows() + 1];
  // The first num_rows_ entries are the same
  copy(rows_.get(), rows_.get() + num_rows_, new_rows);

  // new_rows = [rows_, m.row() + rows_[num_rows_]]
  fill(new_rows + num_rows_,
       new_rows + num_rows_ + m.num_rows() + 1,
       rows_[num_rows_]);

  for (int r = 0; r < m.num_rows() + 1; ++r) {
    new_rows[num_rows_ + r] += m.rows()[r];
  }

  rows_.reset(new_rows);
  num_rows_ += m.num_rows();
}

void CompressedRowSparseMatrix::ToTextFile(FILE* file) const {
  CHECK_NOTNULL(file);
  for (int r = 0; r < num_rows_; ++r) {
    for (int idx = rows_[r]; idx < rows_[r + 1]; ++idx) {
      fprintf(file, "% 10d % 10d %17f\n", r, cols_[idx], values_[idx]);
    }
  }
}

}  // namespace internal
}  // namespace ceres
