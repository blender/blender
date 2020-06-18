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

#include "ceres/subset_preconditioner.h"

#include <memory>
#include <string>
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/inner_product_computer.h"
#include "ceres/linear_solver.h"
#include "ceres/sparse_cholesky.h"
#include "ceres/types.h"

namespace ceres {
namespace internal {

SubsetPreconditioner::SubsetPreconditioner(
    const Preconditioner::Options& options, const BlockSparseMatrix& A)
    : options_(options), num_cols_(A.num_cols()) {
  CHECK_GE(options_.subset_preconditioner_start_row_block, 0)
      << "Congratulations, you found a bug in Ceres. Please report it.";

  LinearSolver::Options sparse_cholesky_options;
  sparse_cholesky_options.sparse_linear_algebra_library_type =
      options_.sparse_linear_algebra_library_type;
  sparse_cholesky_options.use_postordering =
      options_.use_postordering;
  sparse_cholesky_ = SparseCholesky::Create(sparse_cholesky_options);
}

SubsetPreconditioner::~SubsetPreconditioner() {}

void SubsetPreconditioner::RightMultiply(const double* x, double* y) const {
  CHECK(x != nullptr);
  CHECK(y != nullptr);
  std::string message;
  sparse_cholesky_->Solve(x, y, &message);
}

bool SubsetPreconditioner::UpdateImpl(const BlockSparseMatrix& A,
                                      const double* D) {
  BlockSparseMatrix* m = const_cast<BlockSparseMatrix*>(&A);
  const CompressedRowBlockStructure* bs = m->block_structure();

  // A = [P]
  //     [Q]

  // Now add D to A if needed.
  if (D != NULL) {
    // A = [P]
    //     [Q]
    //     [D]
    std::unique_ptr<BlockSparseMatrix> regularizer(
        BlockSparseMatrix::CreateDiagonalMatrix(D, bs->cols));
    m->AppendRows(*regularizer);
  }

  if (inner_product_computer_.get() == NULL) {
    inner_product_computer_.reset(InnerProductComputer::Create(
        *m,
        options_.subset_preconditioner_start_row_block,
        bs->rows.size(),
        sparse_cholesky_->StorageType()));
  }

  // Compute inner_product = [Q'*Q + D'*D]
  inner_product_computer_->Compute();

  // Unappend D if needed.
  if (D != NULL) {
    // A = [P]
    //     [Q]
    m->DeleteRowBlocks(bs->cols.size());
  }

  std::string message;
  // Compute L. s.t., LL' = Q'*Q + D'*D
  const LinearSolverTerminationType termination_type =
      sparse_cholesky_->Factorize(inner_product_computer_->mutable_result(),
                                  &message);
  if (termination_type != LINEAR_SOLVER_SUCCESS) {
    LOG(ERROR) << "Preconditioner factorization failed: " << message;
    return false;
  }

  return true;
}

}  // namespace internal
}  // namespace ceres
