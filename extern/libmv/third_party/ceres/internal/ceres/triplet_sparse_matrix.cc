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

#include "ceres/triplet_sparse_matrix.h"

#include <algorithm>
#include <cstddef>
#include "ceres/internal/eigen.h"
#include "ceres/internal/port.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

TripletSparseMatrix::TripletSparseMatrix()
    : num_rows_(0),
      num_cols_(0),
      max_num_nonzeros_(0),
      num_nonzeros_(0),
      rows_(NULL),
      cols_(NULL),
      values_(NULL) {}

TripletSparseMatrix::~TripletSparseMatrix() {}

TripletSparseMatrix::TripletSparseMatrix(int num_rows,
                                         int num_cols,
                                         int max_num_nonzeros)
    : num_rows_(num_rows),
      num_cols_(num_cols),
      max_num_nonzeros_(max_num_nonzeros),
      num_nonzeros_(0),
      rows_(NULL),
      cols_(NULL),
      values_(NULL) {
  // All the sizes should at least be zero
  CHECK_GE(num_rows, 0);
  CHECK_GE(num_cols, 0);
  CHECK_GE(max_num_nonzeros, 0);
  AllocateMemory();
}

TripletSparseMatrix::TripletSparseMatrix(const TripletSparseMatrix& orig)
    : SparseMatrix(),
      num_rows_(orig.num_rows_),
      num_cols_(orig.num_cols_),
      max_num_nonzeros_(orig.max_num_nonzeros_),
      num_nonzeros_(orig.num_nonzeros_),
      rows_(NULL),
      cols_(NULL),
      values_(NULL) {
  AllocateMemory();
  CopyData(orig);
}

TripletSparseMatrix& TripletSparseMatrix::operator=(
    const TripletSparseMatrix& rhs) {
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
    if ((rows_[i] < 0) || (rows_[i] >= num_rows_) ||
        (cols_[i] < 0) || (cols_[i] >= num_cols_))
      return false;
  }
  return true;
}

void TripletSparseMatrix::Reserve(int new_max_num_nonzeros) {
  CHECK_LE(num_nonzeros_, new_max_num_nonzeros)
      << "Reallocation will cause data loss";

  // Nothing to do if we have enough space already.
  if (new_max_num_nonzeros <= max_num_nonzeros_)
    return;

  int* new_rows = new int[new_max_num_nonzeros];
  int* new_cols = new int[new_max_num_nonzeros];
  double* new_values = new double[new_max_num_nonzeros];

  for (int i = 0; i < num_nonzeros_; ++i) {
    new_rows[i] = rows_[i];
    new_cols[i] = cols_[i];
    new_values[i] = values_[i];
  }

  rows_.reset(new_rows);
  cols_.reset(new_cols);
  values_.reset(new_values);

  max_num_nonzeros_ = new_max_num_nonzeros;
}

void TripletSparseMatrix::SetZero() {
  fill(values_.get(), values_.get() + max_num_nonzeros_, 0.0);
  num_nonzeros_ = 0;
}

void TripletSparseMatrix::set_num_nonzeros(int num_nonzeros) {
  CHECK_GE(num_nonzeros, 0);
  CHECK_LE(num_nonzeros, max_num_nonzeros_);
  num_nonzeros_ = num_nonzeros;
};

void TripletSparseMatrix::AllocateMemory() {
  rows_.reset(new int[max_num_nonzeros_]);
  cols_.reset(new int[max_num_nonzeros_]);
  values_.reset(new double[max_num_nonzeros_]);
}

void TripletSparseMatrix::CopyData(const TripletSparseMatrix& orig) {
  for (int i = 0; i < num_nonzeros_; ++i) {
    rows_[i] = orig.rows_[i];
    cols_[i] = orig.cols_[i];
    values_[i] = orig.values_[i];
  }
}

void TripletSparseMatrix::RightMultiply(const double* x,  double* y) const {
  for (int i = 0; i < num_nonzeros_; ++i) {
    y[rows_[i]] += values_[i]*x[cols_[i]];
  }
}

void TripletSparseMatrix::LeftMultiply(const double* x, double* y) const {
  for (int i = 0; i < num_nonzeros_; ++i) {
    y[cols_[i]] += values_[i]*x[rows_[i]];
  }
}

void TripletSparseMatrix::SquaredColumnNorm(double* x) const {
  CHECK_NOTNULL(x);
  VectorRef(x, num_cols_).setZero();
  for (int i = 0; i < num_nonzeros_; ++i) {
    x[cols_[i]] += values_[i] * values_[i];
  }
}

void TripletSparseMatrix::ScaleColumns(const double* scale) {
  CHECK_NOTNULL(scale);
  for (int i = 0; i < num_nonzeros_; ++i) {
    values_[i] = values_[i] * scale[cols_[i]];
  }
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
    num_rows_  = new_num_rows;
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
        r_ptr[i-dropped_terms] = r_ptr[i];
        c_ptr[i-dropped_terms] = c_ptr[i];
        v_ptr[i-dropped_terms] = v_ptr[i];
      }
    } else {
      ++dropped_terms;
    }
  }
  num_nonzeros_ -= dropped_terms;
}

TripletSparseMatrix* TripletSparseMatrix::CreateSparseDiagonalMatrix(
    const double* values, int num_rows) {
  TripletSparseMatrix* m =
      new TripletSparseMatrix(num_rows, num_rows, num_rows);
  for (int i = 0; i < num_rows; ++i) {
    m->mutable_rows()[i] = i;
    m->mutable_cols()[i] = i;
    m->mutable_values()[i] = values[i];
  }
  m->set_num_nonzeros(num_rows);
  return m;
}

void TripletSparseMatrix::ToTextFile(FILE* file) const {
  CHECK_NOTNULL(file);
  for (int i = 0; i < num_nonzeros_; ++i) {
    fprintf(file, "% 10d % 10d %17f\n", rows_[i], cols_[i], values_[i]);
  }
}

}  // namespace internal
}  // namespace ceres
