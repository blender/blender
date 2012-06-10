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
//
// An iterative solver for solving the Schur complement/reduced camera
// linear system that arise in SfM problems.

#ifndef CERES_INTERNAL_IMPLICIT_SCHUR_COMPLEMENT_H_
#define CERES_INTERNAL_IMPLICIT_SCHUR_COMPLEMENT_H_

#include "ceres/linear_operator.h"
#include "ceres/partitioned_matrix_view.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/types.h"

namespace ceres {
namespace internal {

class BlockSparseMatrix;
class BlockSparseMatrixBase;

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
// auxilliary bits needed to implement a CG solver on the Schur
// complement using the PartitionedMatrixView object.
//
// THREAD SAFETY: This class is nqot thread safe. In particular, the
// RightMultiply (and the LeftMultiply) methods are not thread safe as
// they depend on mutable arrays used for the temporaries needed to
// compute the product y += Sx;
class ImplicitSchurComplement : public LinearOperator {
 public:
  // num_eliminate_blocks is the number of E blocks in the matrix
  // A.
  //
  // constant_sparsity indicates if across calls to Init, the sparsity
  // structure of the matrix A remains constant or not. This makes for
  // significant savings across multiple matrices A, e.g. when used in
  // conjunction with an optimization algorithm.
  //
  // preconditioner indicates whether the inverse of the matrix F'F
  // should be computed or not as a preconditioner for the Schur
  // Complement.
  //
  // TODO(sameeragarwal): Get rid of the two bools below and replace
  // them with enums.
  ImplicitSchurComplement(int num_eliminate_blocks,
                          bool constant_sparsity,
                          bool preconditioner);
  virtual ~ImplicitSchurComplement();

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
  void Init(const BlockSparseMatrixBase& A, const double* D, const double* b);

  // y += Sx, where S is the Schur complement.
  virtual void RightMultiply(const double* x, double* y) const;

  // The Schur complement is a symmetric positive definite matrix,
  // thus the left and right multiply operators are the same.
  virtual void LeftMultiply(const double* x, double* y) const {
    RightMultiply(x, y);
  }

  // y = (E'E)^-1 (E'b - E'F x). Given an estimate of the solution to
  // the Schur complement system, this method computes the value of
  // the e_block variables that were eliminated to form the Schur
  // complement.
  void BackSubstitute(const double* x, double* y);

  virtual int num_rows() const { return A_->num_cols_f(); }
  virtual int num_cols() const { return A_->num_cols_f(); }
  const Vector& rhs()    const { return rhs_;             }

  const BlockSparseMatrix* block_diagonal_EtE_inverse() const {
    return block_diagonal_EtE_inverse_.get();
  }

  const BlockSparseMatrix* block_diagonal_FtF_inverse() const {
    return block_diagonal_FtF_inverse_.get();
  }

 private:
  void AddDiagonalAndInvert(const double* D, BlockSparseMatrix* matrix);
  void UpdateRhs();

  int num_eliminate_blocks_;
  bool constant_sparsity_;
  bool preconditioner_;

  scoped_ptr<PartitionedMatrixView> A_;
  const double* D_;
  const double* b_;

  scoped_ptr<BlockSparseMatrix> block_diagonal_EtE_inverse_;
  scoped_ptr<BlockSparseMatrix> block_diagonal_FtF_inverse_;

  Vector rhs_;

  // Temporary storage vectors used to implement RightMultiply.
  mutable Vector tmp_rows_;
  mutable Vector tmp_e_cols_;
  mutable Vector tmp_e_cols_2_;
  mutable Vector tmp_f_cols_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_IMPLICIT_SCHUR_COMPLEMENT_H_
