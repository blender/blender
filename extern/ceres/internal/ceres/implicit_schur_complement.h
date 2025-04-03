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
// An iterative solver for solving the Schur complement/reduced camera
// linear system that arise in SfM problems.

#ifndef CERES_INTERNAL_IMPLICIT_SCHUR_COMPLEMENT_H_
#define CERES_INTERNAL_IMPLICIT_SCHUR_COMPLEMENT_H_

#include <memory>

#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/export.h"
#include "ceres/linear_operator.h"
#include "ceres/linear_solver.h"
#include "ceres/partitioned_matrix_view.h"
#include "ceres/types.h"

namespace ceres::internal {

class BlockSparseMatrix;

// This class implements various linear algebraic operations related
// to the Schur complement without explicitly forming it.
//
//
// Given a reactangular linear system Ax = b, where
//
//   A = [E F]
//
// The normal equations are given by
//
//   A'Ax = A'b
//
//  |E'E E'F||y| = |E'b|
//  |F'E F'F||z|   |F'b|
//
// and the Schur complement system is given by
//
//  [F'F - F'E (E'E)^-1 E'F] z = F'b - F'E (E'E)^-1 E'b
//
// Now if we wish to solve Ax = b in the least squares sense, one way
// is to form this Schur complement system and solve it using
// Preconditioned Conjugate Gradients.
//
// The key operation in a conjugate gradient solver is the evaluation of the
// matrix vector product with the Schur complement
//
//   S = F'F - F'E (E'E)^-1 E'F
//
// It is straightforward to see that matrix vector products with S can
// be evaluated without storing S in memory. Instead, given (E'E)^-1
// (which for our purposes is an easily inverted block diagonal
// matrix), it can be done in terms of matrix vector products with E,
// F and (E'E)^-1. This class implements this functionality and other
// auxiliary bits needed to implement a CG solver on the Schur
// complement using the PartitionedMatrixView object.
//
// THREAD SAFETY: This class is not thread safe. In particular, the
// RightMultiplyAndAccumulate (and the LeftMultiplyAndAccumulate) methods are
// not thread safe as they depend on mutable arrays used for the temporaries
// needed to compute the product y += Sx;
class CERES_NO_EXPORT ImplicitSchurComplement final : public LinearOperator {
 public:
  // num_eliminate_blocks is the number of E blocks in the matrix
  // A.
  //
  // preconditioner indicates whether the inverse of the matrix F'F
  // should be computed or not as a preconditioner for the Schur
  // Complement.
  //
  // TODO(sameeragarwal): Get rid of the two bools below and replace
  // them with enums.
  explicit ImplicitSchurComplement(const LinearSolver::Options& options);

  // Initialize the Schur complement for a linear least squares
  // problem of the form
  //
  //   |A      | x = |b|
  //   |diag(D)|     |0|
  //
  // If D is null, then it is treated as a zero dimensional matrix. It
  // is important that the matrix A have a BlockStructure object
  // associated with it and has a block structure that is compatible
  // with the SchurComplement solver.
  void Init(const BlockSparseMatrix& A, const double* D, const double* b);

  // y += Sx, where S is the Schur complement.
  void RightMultiplyAndAccumulate(const double* x, double* y) const final;

  // The Schur complement is a symmetric positive definite matrix,
  // thus the left and right multiply operators are the same.
  void LeftMultiplyAndAccumulate(const double* x, double* y) const final {
    RightMultiplyAndAccumulate(x, y);
  }

  // Following is useful for approximation of S^-1 via power series expansion.
  // Z = (F'F)^-1 F'E (E'E)^-1 E'F
  // y += Zx
  void InversePowerSeriesOperatorRightMultiplyAccumulate(const double* x,
                                                         double* y) const;

  // y = (E'E)^-1 (E'b - E'F x). Given an estimate of the solution to
  // the Schur complement system, this method computes the value of
  // the e_block variables that were eliminated to form the Schur
  // complement.
  void BackSubstitute(const double* x, double* y);

  int num_rows() const final { return A_->num_cols_f(); }
  int num_cols() const final { return A_->num_cols_f(); }
  const Vector& rhs() const { return rhs_; }

  const BlockSparseMatrix* block_diagonal_EtE_inverse() const {
    return block_diagonal_EtE_inverse_.get();
  }

  const BlockSparseMatrix* block_diagonal_FtF_inverse() const {
    CHECK(compute_ftf_inverse_);
    return block_diagonal_FtF_inverse_.get();
  }

 private:
  void AddDiagonalAndInvert(const double* D, BlockSparseMatrix* matrix);
  void UpdateRhs();

  const LinearSolver::Options& options_;
  bool compute_ftf_inverse_ = false;
  std::unique_ptr<PartitionedMatrixViewBase> A_;
  const double* D_ = nullptr;
  const double* b_ = nullptr;

  std::unique_ptr<BlockSparseMatrix> block_diagonal_EtE_inverse_;
  std::unique_ptr<BlockSparseMatrix> block_diagonal_FtF_inverse_;

  Vector rhs_;

  // Temporary storage vectors used to implement RightMultiplyAndAccumulate.
  mutable Vector tmp_rows_;
  mutable Vector tmp_e_cols_;
  mutable Vector tmp_e_cols_2_;
  mutable Vector tmp_f_cols_;
};

}  // namespace ceres::internal

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_INTERNAL_IMPLICIT_SCHUR_COMPLEMENT_H_
