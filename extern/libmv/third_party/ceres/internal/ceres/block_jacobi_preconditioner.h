// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2014 Google Inc. All rights reserved.
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

#ifndef CERES_INTERNAL_BLOCK_JACOBI_PRECONDITIONER_H_
#define CERES_INTERNAL_BLOCK_JACOBI_PRECONDITIONER_H_

#include <vector>
#include "ceres/block_random_access_diagonal_matrix.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/preconditioner.h"

namespace ceres {
namespace internal {

class BlockSparseMatrix;
struct CompressedRowBlockStructure;

// A block Jacobi preconditioner. This is intended for use with
// conjugate gradients, or other iterative symmetric solvers. To use
// the preconditioner, create one by passing a BlockSparseMatrix "A"
// to the constructor. This fixes the sparsity pattern to the pattern
// of the matrix A^TA.
//
// Before each use of the preconditioner in a solve with conjugate gradients,
// update the matrix by running Update(A, D). The values of the matrix A are
// inspected to construct the preconditioner. The vector D is applied as the
// D^TD diagonal term.
class BlockJacobiPreconditioner : public BlockSparseMatrixPreconditioner {
 public:
  // A must remain valid while the BlockJacobiPreconditioner is.
  explicit BlockJacobiPreconditioner(const BlockSparseMatrix& A);
  virtual ~BlockJacobiPreconditioner();

  // Preconditioner interface
  virtual void RightMultiply(const double* x, double* y) const;
  virtual int num_rows() const { return m_->num_rows(); }
  virtual int num_cols() const { return m_->num_rows(); }

  const BlockRandomAccessDiagonalMatrix& matrix() const { return *m_; }
 private:
  virtual bool UpdateImpl(const BlockSparseMatrix& A, const double* D);

  scoped_ptr<BlockRandomAccessDiagonalMatrix> m_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_BLOCK_JACOBI_PRECONDITIONER_H_
