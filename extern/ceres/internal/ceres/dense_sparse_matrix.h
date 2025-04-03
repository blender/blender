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
//
// A dense matrix implemented under the SparseMatrix interface.

#ifndef CERES_INTERNAL_DENSE_SPARSE_MATRIX_H_
#define CERES_INTERNAL_DENSE_SPARSE_MATRIX_H_

#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/export.h"
#include "ceres/sparse_matrix.h"
#include "ceres/types.h"

namespace ceres::internal {

class TripletSparseMatrix;

class CERES_NO_EXPORT DenseSparseMatrix final : public SparseMatrix {
 public:
  // Build a matrix with the same content as the TripletSparseMatrix
  // m. This assumes that m does not have any repeated entries.
  explicit DenseSparseMatrix(const TripletSparseMatrix& m);
  explicit DenseSparseMatrix(Matrix m);
  DenseSparseMatrix(int num_rows, int num_cols);

  // SparseMatrix interface.
  void SetZero() final;
  void RightMultiplyAndAccumulate(const double* x, double* y) const final;
  void LeftMultiplyAndAccumulate(const double* x, double* y) const final;
  void SquaredColumnNorm(double* x) const final;
  void ScaleColumns(const double* scale) final;
  void ToDenseMatrix(Matrix* dense_matrix) const final;
  void ToTextFile(FILE* file) const final;
  int num_rows() const final;
  int num_cols() const final;
  int num_nonzeros() const final;
  const double* values() const final { return m_.data(); }
  double* mutable_values() final { return m_.data(); }

  const Matrix& matrix() const;
  Matrix* mutable_matrix();

 private:
  Matrix m_;
};

}  // namespace ceres::internal

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_INTERNAL_DENSE_SPARSE_MATRIX_H_
