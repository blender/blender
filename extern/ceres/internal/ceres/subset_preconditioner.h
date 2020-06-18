// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2017 Google Inc. All rights reserved.
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

#ifndef CERES_INTERNAL_SUBSET_PRECONDITIONER_H_
#define CERES_INTERNAL_SUBSET_PRECONDITIONER_H_

#include <memory>
#include "ceres/preconditioner.h"

namespace ceres {
namespace internal {

class BlockSparseMatrix;
class SparseCholesky;
class InnerProductComputer;

// Subset preconditioning, uses a subset of the rows of the Jacobian
// to construct a preconditioner for the normal equations.
//
// To keep the interface simple, we assume that the matrix A has
// already been re-ordered that the user wishes to some subset of the
// bottom row blocks of the matrix as the preconditioner. This is
// controlled by
// Preconditioner::Options::subset_preconditioner_start_row_block.
//
// When using the subset preconditioner, all row blocks starting
// from this row block are used to construct the preconditioner.
//
// More precisely the matrix A is horizontally partitioned as
//
// A = [P]
//     [Q]
//
// where P has subset_preconditioner_start_row_block row blocks,
// and the preconditioner is the inverse of the matrix Q'Q.
//
// Obviously, the smaller this number, the more accurate and
// computationally expensive this preconditioner will be.
//
// See the tests for example usage.
class SubsetPreconditioner : public BlockSparseMatrixPreconditioner {
 public:
  SubsetPreconditioner(const Preconditioner::Options& options,
                       const BlockSparseMatrix& A);
  virtual ~SubsetPreconditioner();

  // Preconditioner interface
  void RightMultiply(const double* x, double* y) const final;
  int num_rows() const final { return num_cols_; }
  int num_cols() const final { return num_cols_; }

 private:
  bool UpdateImpl(const BlockSparseMatrix& A, const double* D) final;

  const Preconditioner::Options options_;
  const int num_cols_;
  std::unique_ptr<SparseCholesky> sparse_cholesky_;
  std::unique_ptr<InnerProductComputer> inner_product_computer_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_SUBSET_PRECONDITIONER_H_
