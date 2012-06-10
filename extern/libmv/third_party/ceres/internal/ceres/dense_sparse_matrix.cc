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
// Author: keir@google.com (Keir Mierle)

#include "ceres/dense_sparse_matrix.h"

#include <algorithm>
#include "ceres/matrix_proto.h"
#include "ceres/triplet_sparse_matrix.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/port.h"

namespace ceres {
namespace internal {

DenseSparseMatrix::DenseSparseMatrix(int num_rows, int num_cols)
    : has_diagonal_appended_(false),
      has_diagonal_reserved_(false) {
  // Allocate enough space for the diagonal.
  m_.resize(num_rows, num_cols);
  m_.setZero();
}

DenseSparseMatrix::DenseSparseMatrix(const TripletSparseMatrix& m)
    : m_(Eigen::MatrixXd::Zero(m.num_rows(), m.num_cols())),
      has_diagonal_appended_(false),
      has_diagonal_reserved_(false) {
  const double *values = m.values();
  const int *rows = m.rows();
  const int *cols = m.cols();
  int num_nonzeros = m.num_nonzeros();

  for (int i = 0; i < num_nonzeros; ++i) {
    m_(rows[i], cols[i]) += values[i];
  }
}

DenseSparseMatrix::DenseSparseMatrix(const Matrix& m)
    : m_(m),
      has_diagonal_appended_(false),
      has_diagonal_reserved_(false) {
}

#ifndef CERES_DONT_HAVE_PROTOCOL_BUFFERS
DenseSparseMatrix::DenseSparseMatrix(const SparseMatrixProto& outer_proto)
    : m_(Eigen::MatrixXd::Zero(
        outer_proto.dense_matrix().num_rows(),
        outer_proto.dense_matrix().num_cols())),
      has_diagonal_appended_(false),
      has_diagonal_reserved_(false) {
  const DenseSparseMatrixProto& proto = outer_proto.dense_matrix();
  for (int i = 0; i < m_.rows(); ++i) {
    for (int j = 0; j < m_.cols(); ++j) {
      m_(i, j) = proto.values(m_.cols() * i + j);
    }
  }
}
#endif

void DenseSparseMatrix::SetZero() {
  m_.setZero();
}

void DenseSparseMatrix::RightMultiply(const double* x, double* y) const {
  VectorRef(y, num_rows()) += matrix() * ConstVectorRef(x, num_cols());
}

void DenseSparseMatrix::LeftMultiply(const double* x, double* y) const {
  VectorRef(y, num_cols()) +=
      matrix().transpose() * ConstVectorRef(x, num_rows());
}

void DenseSparseMatrix::SquaredColumnNorm(double* x) const {
  VectorRef(x, num_cols()) = m_.colwise().squaredNorm();
}

void DenseSparseMatrix::ScaleColumns(const double* scale) {
  m_ *= ConstVectorRef(scale, num_cols()).asDiagonal();
}

void DenseSparseMatrix::ToDenseMatrix(Matrix* dense_matrix) const {
  *dense_matrix = m_;
}

#ifndef CERES_DONT_HAVE_PROTOCOL_BUFFERS
void DenseSparseMatrix::ToProto(SparseMatrixProto* outer_proto) const {
  CHECK(!has_diagonal_appended_) << "Not supported.";
  outer_proto->Clear();
  DenseSparseMatrixProto* proto = outer_proto->mutable_dense_matrix();

  proto->set_num_rows(num_rows());
  proto->set_num_cols(num_cols());

  int num_nnz = num_nonzeros();
  for (int i = 0; i < num_nnz; ++i) {
    proto->add_values(m_.data()[i]);
  }
}
#endif

void DenseSparseMatrix::AppendDiagonal(double *d) {
  CHECK(!has_diagonal_appended_);
  if (!has_diagonal_reserved_) {
    Matrix tmp = m_;
    m_.resize(m_.rows() + m_.cols(), m_.cols());
    m_.setZero();
    m_.block(0, 0, tmp.rows(), tmp.cols()) = tmp;
    has_diagonal_reserved_ = true;
  }

  m_.bottomLeftCorner(m_.cols(), m_.cols()) =
      ConstVectorRef(d, m_.cols()).asDiagonal();
  has_diagonal_appended_ = true;
}

void DenseSparseMatrix::RemoveDiagonal() {
  CHECK(has_diagonal_appended_);
  has_diagonal_appended_ = false;
  // Leave the diagonal reserved.
}

int DenseSparseMatrix::num_rows() const {
  if (has_diagonal_reserved_ && !has_diagonal_appended_) {
    return m_.rows() - m_.cols();
  }
  return m_.rows();
}

int DenseSparseMatrix::num_cols() const {
  return m_.cols();
}

int DenseSparseMatrix::num_nonzeros() const {
  if (has_diagonal_reserved_ && !has_diagonal_appended_) {
    return (m_.rows() - m_.cols()) * m_.cols();
  }
  return m_.rows() * m_.cols();
}

ConstAlignedMatrixRef DenseSparseMatrix::matrix() const {
  if (has_diagonal_reserved_ && !has_diagonal_appended_) {
    return ConstAlignedMatrixRef(
        m_.data(), m_.rows() - m_.cols(), m_.cols());
  }
  return ConstAlignedMatrixRef(m_.data(), m_.rows(), m_.cols());
}

AlignedMatrixRef DenseSparseMatrix::mutable_matrix() {
  if (has_diagonal_reserved_ && !has_diagonal_appended_) {
    return AlignedMatrixRef(
        m_.data(), m_.rows() - m_.cols(), m_.cols());
  }
  return AlignedMatrixRef(m_.data(), m_.rows(), m_.cols());
}

void DenseSparseMatrix::ToTextFile(FILE* file) const {
  CHECK_NOTNULL(file);
  const int active_rows =
      (has_diagonal_reserved_ && !has_diagonal_appended_)
      ? (m_.rows() - m_.cols()) 
      : m_.rows();

  for (int r = 0; r < active_rows; ++r) {
    for (int c = 0; c < m_.cols(); ++c) {
      fprintf(file,  "% 10d % 10d %17f\n", r, c, m_(r, c));
    }
  }
}

}  // namespace internal
}  // namespace ceres
