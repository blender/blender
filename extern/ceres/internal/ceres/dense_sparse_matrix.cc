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

#include "ceres/dense_sparse_matrix.h"

#include <algorithm>
#include <utility>

#include "ceres/internal/eigen.h"
#include "ceres/internal/export.h"
#include "ceres/triplet_sparse_matrix.h"
#include "glog/logging.h"

namespace ceres::internal {

DenseSparseMatrix::DenseSparseMatrix(int num_rows, int num_cols)
    : m_(Matrix(num_rows, num_cols)) {}

DenseSparseMatrix::DenseSparseMatrix(const TripletSparseMatrix& m)
    : m_(Matrix::Zero(m.num_rows(), m.num_cols())) {
  const double* values = m.values();
  const int* rows = m.rows();
  const int* cols = m.cols();
  int num_nonzeros = m.num_nonzeros();

  for (int i = 0; i < num_nonzeros; ++i) {
    m_(rows[i], cols[i]) += values[i];
  }
}

DenseSparseMatrix::DenseSparseMatrix(Matrix m) : m_(std::move(m)) {}

void DenseSparseMatrix::SetZero() { m_.setZero(); }

void DenseSparseMatrix::RightMultiplyAndAccumulate(const double* x,
                                                   double* y) const {
  VectorRef(y, num_rows()).noalias() += m_ * ConstVectorRef(x, num_cols());
}

void DenseSparseMatrix::LeftMultiplyAndAccumulate(const double* x,
                                                  double* y) const {
  VectorRef(y, num_cols()).noalias() +=
      m_.transpose() * ConstVectorRef(x, num_rows());
}

void DenseSparseMatrix::SquaredColumnNorm(double* x) const {
  // This implementation is 3x faster than the naive version
  // x = m_.colwise().square().sum(), likely because m_
  // is a row major matrix.

  const int num_rows = m_.rows();
  const int num_cols = m_.cols();
  std::fill_n(x, num_cols, 0.0);
  const double* m = m_.data();
  for (int i = 0; i < num_rows; ++i) {
    for (int j = 0; j < num_cols; ++j, ++m) {
      x[j] += (*m) * (*m);
    }
  }
}

void DenseSparseMatrix::ScaleColumns(const double* scale) {
  m_ *= ConstVectorRef(scale, num_cols()).asDiagonal();
}

void DenseSparseMatrix::ToDenseMatrix(Matrix* dense_matrix) const {
  *dense_matrix = m_;
}

int DenseSparseMatrix::num_rows() const { return m_.rows(); }

int DenseSparseMatrix::num_cols() const { return m_.cols(); }

int DenseSparseMatrix::num_nonzeros() const { return m_.rows() * m_.cols(); }

const Matrix& DenseSparseMatrix::matrix() const { return m_; }

Matrix* DenseSparseMatrix::mutable_matrix() { return &m_; }

void DenseSparseMatrix::ToTextFile(FILE* file) const {
  CHECK(file != nullptr);
  for (int r = 0; r < m_.rows(); ++r) {
    for (int c = 0; c < m_.cols(); ++c) {
      fprintf(file, "% 10d % 10d %17f\n", r, c, m_(r, c));
    }
  }
}

}  // namespace ceres::internal
