// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2015 Google Inc. All rights reserved.
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

#include "ceres/internal/eigen.h"
#include "ceres/sparse_matrix.h"
#include "ceres/types.h"

namespace ceres {
namespace internal {

class TripletSparseMatrix;

class DenseSparseMatrix : public SparseMatrix {
 public:
  // Build a matrix with the same content as the TripletSparseMatrix
  // m. This assumes that m does not have any repeated entries.
  explicit DenseSparseMatrix(const TripletSparseMatrix& m);
  explicit DenseSparseMatrix(const ColMajorMatrix& m);

  DenseSparseMatrix(int num_rows, int num_cols);
  DenseSparseMatrix(int num_rows, int num_cols, bool reserve_diagonal);

  virtual ~DenseSparseMatrix() {}

  // SparseMatrix interface.
  void SetZero() final;
  void RightMultiply(const double* x, double* y) const final;
  void LeftMultiply(const double* x, double* y) const final;
  void SquaredColumnNorm(double* x) const final;
  void ScaleColumns(const double* scale) final;
  void ToDenseMatrix(Matrix* dense_matrix) const final;
  void ToTextFile(FILE* file) const final;
  int num_rows() const final;
  int num_cols() const final;
  int num_nonzeros() const final;
  const double* values() const final { return m_.data(); }
  double* mutable_values() final { return m_.data(); }

  ConstColMajorMatrixRef matrix() const;
  ColMajorMatrixRef mutable_matrix();

  // Only one diagonal can be appended at a time. The diagonal is appended to
  // as a new set of rows, e.g.
  //
  // Original matrix:
  //
  //   x x x
  //   x x x
  //   x x x
  //
  // After append diagonal (1, 2, 3):
  //
  //   x x x
  //   x x x
  //   x x x
  //   1 0 0
  //   0 2 0
  //   0 0 3
  //
  // Calling RemoveDiagonal removes the block. It is a fatal error to append a
  // diagonal to a matrix that already has an appended diagonal, and it is also
  // a fatal error to remove a diagonal from a matrix that has none.
  void AppendDiagonal(double *d);
  void RemoveDiagonal();

 private:
  ColMajorMatrix m_;
  bool has_diagonal_appended_;
  bool has_diagonal_reserved_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_DENSE_SPARSE_MATRIX_H_
