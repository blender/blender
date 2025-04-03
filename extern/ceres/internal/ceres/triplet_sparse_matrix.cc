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

#include "ceres/triplet_sparse_matrix.h"

#include <algorithm>
#include <memory>
#include <random>

#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/crs_matrix.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/export.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres::internal {

TripletSparseMatrix::TripletSparseMatrix()
    : num_rows_(0), num_cols_(0), max_num_nonzeros_(0), num_nonzeros_(0) {}

TripletSparseMatrix::~TripletSparseMatrix() = default;

TripletSparseMatrix::TripletSparseMatrix(int num_rows,
                                         int num_cols,
                                         int max_num_nonzeros)
    : num_rows_(num_rows),
      num_cols_(num_cols),
      max_num_nonzeros_(max_num_nonzeros),
      num_nonzeros_(0) {
  // All the sizes should at least be zero
  CHECK_GE(num_rows, 0);
  CHECK_GE(num_cols, 0);
  CHECK_GE(max_num_nonzeros, 0);
  AllocateMemory();
}

TripletSparseMatrix::TripletSparseMatrix(const int num_rows,
                                         const int num_cols,
                                         const std::vector<int>& rows,
                                         const std::vector<int>& cols,
                                         const std::vector<double>& values)
    : num_rows_(num_rows),
      num_cols_(num_cols),
      max_num_nonzeros_(values.size()),
      num_nonzeros_(values.size()) {
  // All the sizes should at least be zero
  CHECK_GE(num_rows, 0);
  CHECK_GE(num_cols, 0);
  CHECK_EQ(rows.size(), cols.size());
  CHECK_EQ(rows.size(), values.size());
  AllocateMemory();
  std::copy(rows.begin(), rows.end(), rows_.get());
  std::copy(cols.begin(), cols.end(), cols_.get());
  std::copy(values.begin(), values.end(), values_.get());
}

TripletSparseMatrix::TripletSparseMatrix(const TripletSparseMatrix& orig)
    : SparseMatrix(),
      num_rows_(orig.num_rows_),
      num_cols_(orig.num_cols_),
      max_num_nonzeros_(orig.max_num_nonzeros_),
      num_nonzeros_(orig.num_nonzeros_) {
  AllocateMemory();
  CopyData(orig);
}

TripletSparseMatrix& TripletSparseMatrix::operator=(
    const TripletSparseMatrix& rhs) {
  if (this == &rhs) {
    return *this;
  }
  num_rows_ = rhs.num_rows_;
  num_cols_ = rhs.num_cols_;
  num_nonzeros_ = rhs.num_nonzeros_;
  max_num_nonzeros_ = rhs.max_num_nonzeros_;
  AllocateMemory();
  CopyData(rhs);
  return *this;
}

bool TripletSparseMatrix::AllTripletsWithinBounds() const {
  for (int i = 0; i < num_nonzeros_; ++i) {
    // clang-format off
    if ((rows_[i] < 0) || (rows_[i] >= num_rows_) ||
        (cols_[i] < 0) || (cols_[i] >= num_cols_)) {
      return false;
    }
    // clang-format on
  }
  return true;
}

void TripletSparseMatrix::Reserve(int new_max_num_nonzeros) {
  CHECK_LE(num_nonzeros_, new_max_num_nonzeros)
      << "Reallocation will cause data loss";

  // Nothing to do if we have enough space already.
  if (new_max_num_nonzeros <= max_num_nonzeros_) return;

  std::unique_ptr<int[]> new_rows =
      std::make_unique<int[]>(new_max_num_nonzeros);
  std::unique_ptr<int[]> new_cols =
      std::make_unique<int[]>(new_max_num_nonzeros);
  std::unique_ptr<double[]> new_values =
      std::make_unique<double[]>(new_max_num_nonzeros);

  for (int i = 0; i < num_nonzeros_; ++i) {
    new_rows[i] = rows_[i];
    new_cols[i] = cols_[i];
    new_values[i] = values_[i];
  }

  rows_ = std::move(new_rows);
  cols_ = std::move(new_cols);
  values_ = std::move(new_values);
  max_num_nonzeros_ = new_max_num_nonzeros;
}

void TripletSparseMatrix::SetZero() {
  std::fill(values_.get(), values_.get() + max_num_nonzeros_, 0.0);
  num_nonzeros_ = 0;
}

void TripletSparseMatrix::set_num_nonzeros(int num_nonzeros) {
  CHECK_GE(num_nonzeros, 0);
  CHECK_LE(num_nonzeros, max_num_nonzeros_);
  num_nonzeros_ = num_nonzeros;
}

void TripletSparseMatrix::AllocateMemory() {
  rows_ = std::make_unique<int[]>(max_num_nonzeros_);
  cols_ = std::make_unique<int[]>(max_num_nonzeros_);
  values_ = std::make_unique<double[]>(max_num_nonzeros_);
}

void TripletSparseMatrix::CopyData(const TripletSparseMatrix& orig) {
  for (int i = 0; i < num_nonzeros_; ++i) {
    rows_[i] = orig.rows_[i];
    cols_[i] = orig.cols_[i];
    values_[i] = orig.values_[i];
  }
}

void TripletSparseMatrix::RightMultiplyAndAccumulate(const double* x,
                                                     double* y) const {
  for (int i = 0; i < num_nonzeros_; ++i) {
    y[rows_[i]] += values_[i] * x[cols_[i]];
  }
}

void TripletSparseMatrix::LeftMultiplyAndAccumulate(const double* x,
                                                    double* y) const {
  for (int i = 0; i < num_nonzeros_; ++i) {
    y[cols_[i]] += values_[i] * x[rows_[i]];
  }
}

void TripletSparseMatrix::SquaredColumnNorm(double* x) const {
  CHECK(x != nullptr);
  VectorRef(x, num_cols_).setZero();
  for (int i = 0; i < num_nonzeros_; ++i) {
    x[cols_[i]] += values_[i] * values_[i];
  }
}

void TripletSparseMatrix::ScaleColumns(const double* scale) {
  CHECK(scale != nullptr);
  for (int i = 0; i < num_nonzeros_; ++i) {
    values_[i] = values_[i] * scale[cols_[i]];
  }
}

void TripletSparseMatrix::ToCRSMatrix(CRSMatrix* crs_matrix) const {
  CompressedRowSparseMatrix::FromTripletSparseMatrix(*this)->ToCRSMatrix(
      crs_matrix);
}

void TripletSparseMatrix::ToDenseMatrix(Matrix* dense_matrix) const {
  dense_matrix->resize(num_rows_, num_cols_);
  dense_matrix->setZero();
  Matrix& m = *dense_matrix;
  for (int i = 0; i < num_nonzeros_; ++i) {
    m(rows_[i], cols_[i]) += values_[i];
  }
}

void TripletSparseMatrix::AppendRows(const TripletSparseMatrix& B) {
  CHECK_EQ(B.num_cols(), num_cols_);
  Reserve(num_nonzeros_ + B.num_nonzeros_);
  for (int i = 0; i < B.num_nonzeros_; ++i) {
    rows_.get()[num_nonzeros_] = B.rows()[i] + num_rows_;
    cols_.get()[num_nonzeros_] = B.cols()[i];
    values_.get()[num_nonzeros_++] = B.values()[i];
  }
  num_rows_ = num_rows_ + B.num_rows();
}

void TripletSparseMatrix::AppendCols(const TripletSparseMatrix& B) {
  CHECK_EQ(B.num_rows(), num_rows_);
  Reserve(num_nonzeros_ + B.num_nonzeros_);
  for (int i = 0; i < B.num_nonzeros_; ++i, ++num_nonzeros_) {
    rows_.get()[num_nonzeros_] = B.rows()[i];
    cols_.get()[num_nonzeros_] = B.cols()[i] + num_cols_;
    values_.get()[num_nonzeros_] = B.values()[i];
  }
  num_cols_ = num_cols_ + B.num_cols();
}

void TripletSparseMatrix::Resize(int new_num_rows, int new_num_cols) {
  if ((new_num_rows >= num_rows_) && (new_num_cols >= num_cols_)) {
    num_rows_ = new_num_rows;
    num_cols_ = new_num_cols;
    return;
  }

  num_rows_ = new_num_rows;
  num_cols_ = new_num_cols;

  int* r_ptr = rows_.get();
  int* c_ptr = cols_.get();
  double* v_ptr = values_.get();

  int dropped_terms = 0;
  for (int i = 0; i < num_nonzeros_; ++i) {
    if ((r_ptr[i] < num_rows_) && (c_ptr[i] < num_cols_)) {
      if (dropped_terms) {
        r_ptr[i - dropped_terms] = r_ptr[i];
        c_ptr[i - dropped_terms] = c_ptr[i];
        v_ptr[i - dropped_terms] = v_ptr[i];
      }
    } else {
      ++dropped_terms;
    }
  }
  num_nonzeros_ -= dropped_terms;
}

std::unique_ptr<TripletSparseMatrix>
TripletSparseMatrix::CreateSparseDiagonalMatrix(const double* values,
                                                int num_rows) {
  std::unique_ptr<TripletSparseMatrix> m =
      std::make_unique<TripletSparseMatrix>(num_rows, num_rows, num_rows);
  for (int i = 0; i < num_rows; ++i) {
    m->mutable_rows()[i] = i;
    m->mutable_cols()[i] = i;
    m->mutable_values()[i] = values[i];
  }
  m->set_num_nonzeros(num_rows);
  return m;
}

void TripletSparseMatrix::ToTextFile(FILE* file) const {
  CHECK(file != nullptr);
  for (int i = 0; i < num_nonzeros_; ++i) {
    fprintf(file, "% 10d % 10d %17f\n", rows_[i], cols_[i], values_[i]);
  }
}

std::unique_ptr<TripletSparseMatrix> TripletSparseMatrix::CreateFromTextFile(
    FILE* file) {
  CHECK(file != nullptr);
  int num_rows = 0;
  int num_cols = 0;
  std::vector<int> rows;
  std::vector<int> cols;
  std::vector<double> values;
  while (true) {
    int row, col;
    double value;
    if (fscanf(file, "%d %d %lf", &row, &col, &value) != 3) {
      break;
    }
    rows.push_back(row);
    cols.push_back(col);
    values.push_back(value);
    num_rows = std::max(num_rows, row + 1);
    num_cols = std::max(num_cols, col + 1);
  }
  VLOG(1) << "Read " << rows.size() << " nonzeros from file.";
  return std::make_unique<TripletSparseMatrix>(
      num_rows, num_cols, rows, cols, values);
}

std::unique_ptr<TripletSparseMatrix> TripletSparseMatrix::CreateRandomMatrix(
    const TripletSparseMatrix::RandomMatrixOptions& options,
    std::mt19937& prng) {
  CHECK_GT(options.num_rows, 0);
  CHECK_GT(options.num_cols, 0);
  CHECK_GT(options.density, 0.0);
  CHECK_LE(options.density, 1.0);

  std::vector<int> rows;
  std::vector<int> cols;
  std::vector<double> values;
  std::uniform_real_distribution<double> uniform01(0.0, 1.0);
  std::normal_distribution<double> standard_normal;
  while (rows.empty()) {
    rows.clear();
    cols.clear();
    values.clear();
    for (int r = 0; r < options.num_rows; ++r) {
      for (int c = 0; c < options.num_cols; ++c) {
        if (uniform01(prng) <= options.density) {
          rows.push_back(r);
          cols.push_back(c);
          values.push_back(standard_normal(prng));
        }
      }
    }
  }

  return std::make_unique<TripletSparseMatrix>(
      options.num_rows, options.num_cols, rows, cols, values);
}

}  // namespace ceres::internal
