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
//
// Interface definition for sparse matrices.

#ifndef CERES_INTERNAL_SPARSE_MATRIX_H_
#define CERES_INTERNAL_SPARSE_MATRIX_H_

#include <cstdio>

#include "ceres/internal/eigen.h"
#include "ceres/internal/export.h"
#include "ceres/linear_operator.h"
#include "ceres/types.h"

namespace ceres::internal {
class ContextImpl;

// This class defines the interface for storing and manipulating
// sparse matrices. The key property that differentiates different
// sparse matrices is how they are organized in memory and how the
// information about the sparsity structure of the matrix is
// stored. This has significant implications for linear solvers
// operating on these matrices.
//
// To deal with the different kinds of layouts, we will assume that a
// sparse matrix will have a two part representation. A values array
// that will be used to store the entries of the sparse matrix and
// some sort of a layout object that tells the user the sparsity
// structure and layout of the values array. For example in case of
// the TripletSparseMatrix, this information is carried in the rows
// and cols arrays and for the BlockSparseMatrix, this information is
// carried in the CompressedRowBlockStructure object.
//
// This interface deliberately does not contain any information about
// the structure of the sparse matrix as that seems to be highly
// matrix type dependent and we are at this stage unable to come up
// with an efficient high level interface that spans multiple sparse
// matrix types.
class CERES_NO_EXPORT SparseMatrix : public LinearOperator {
 public:
  ~SparseMatrix() override;

  // y += Ax;
  using LinearOperator::RightMultiplyAndAccumulate;
  void RightMultiplyAndAccumulate(const double* x,
                                  double* y) const override = 0;

  // y += A'x;
  void LeftMultiplyAndAccumulate(const double* x, double* y) const override = 0;

  // In MATLAB notation sum(A.*A, 1)
  virtual void SquaredColumnNorm(double* x) const = 0;
  virtual void SquaredColumnNorm(double* x,
                                 ContextImpl* context,
                                 int num_threads) const;
  // A = A * diag(scale)
  virtual void ScaleColumns(const double* scale) = 0;
  virtual void ScaleColumns(const double* scale,
                            ContextImpl* context,
                            int num_threads);

  // A = 0. A->num_nonzeros() == 0 is true after this call. The
  // sparsity pattern is preserved.
  virtual void SetZero() = 0;
  virtual void SetZero(ContextImpl* /*context*/, int /*num_threads*/) {
    SetZero();
  }

  // Resize and populate dense_matrix with a dense version of the
  // sparse matrix.
  virtual void ToDenseMatrix(Matrix* dense_matrix) const = 0;

  // Write out the matrix as a sequence of (i,j,s) triplets. This
  // format is useful for loading the matrix into MATLAB/octave as a
  // sparse matrix.
  virtual void ToTextFile(FILE* file) const = 0;

  // Accessors for the values array that stores the entries of the
  // sparse matrix. The exact interpretation of the values of this
  // array depends on the particular kind of SparseMatrix being
  // accessed.
  virtual double* mutable_values() = 0;
  virtual const double* values() const = 0;

  int num_rows() const override = 0;
  int num_cols() const override = 0;
  virtual int num_nonzeros() const = 0;
};

}  // namespace ceres::internal

#endif  // CERES_INTERNAL_SPARSE_MATRIX_H_
