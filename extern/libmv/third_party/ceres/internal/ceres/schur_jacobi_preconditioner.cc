// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2013 Google Inc. All rights reserved.
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

#include "ceres/schur_jacobi_preconditioner.h"

#include <utility>
#include <vector>
#include "Eigen/Dense"
#include "ceres/block_random_access_diagonal_matrix.h"
#include "ceres/block_sparse_matrix.h"
#include "ceres/collections_port.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/linear_solver.h"
#include "ceres/schur_eliminator.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

SchurJacobiPreconditioner::SchurJacobiPreconditioner(
    const CompressedRowBlockStructure& bs,
    const Preconditioner::Options& options)
    : options_(options) {
  CHECK_GT(options_.elimination_groups.size(), 1);
  CHECK_GT(options_.elimination_groups[0], 0);
  const int num_blocks = bs.cols.size() - options_.elimination_groups[0];
  CHECK_GT(num_blocks, 0)
      << "Jacobian should have atleast 1 f_block for "
      << "SCHUR_JACOBI preconditioner.";

  block_size_.resize(num_blocks);
  for (int i = 0; i < num_blocks; ++i) {
    block_size_[i] = bs.cols[i + options_.elimination_groups[0]].size;
  }

  m_.reset(new BlockRandomAccessDiagonalMatrix(block_size_));
  InitEliminator(bs);
}

SchurJacobiPreconditioner::~SchurJacobiPreconditioner() {
}

// Initialize the SchurEliminator.
void SchurJacobiPreconditioner::InitEliminator(
    const CompressedRowBlockStructure& bs) {
  LinearSolver::Options eliminator_options;
  eliminator_options.elimination_groups = options_.elimination_groups;
  eliminator_options.num_threads = options_.num_threads;
  eliminator_options.e_block_size = options_.e_block_size;
  eliminator_options.f_block_size = options_.f_block_size;
  eliminator_options.row_block_size = options_.row_block_size;
  eliminator_.reset(SchurEliminatorBase::Create(eliminator_options));
  eliminator_->Init(eliminator_options.elimination_groups[0], &bs);
}

// Update the values of the preconditioner matrix and factorize it.
bool SchurJacobiPreconditioner::UpdateImpl(const BlockSparseMatrix& A,
                                           const double* D) {
  const int num_rows = m_->num_rows();
  CHECK_GT(num_rows, 0);

  // We need a dummy rhs vector and a dummy b vector since the Schur
  // eliminator combines the computation of the reduced camera matrix
  // with the computation of the right hand side of that linear
  // system.
  //
  // TODO(sameeragarwal): Perhaps its worth refactoring the
  // SchurEliminator::Eliminate function to allow NULL for the rhs. As
  // of now it does not seem to be worth the effort.
  Vector rhs = Vector::Zero(m_->num_rows());
  Vector b = Vector::Zero(A.num_rows());

  // Compute a subset of the entries of the Schur complement.
  eliminator_->Eliminate(&A, b.data(), D, m_.get(), rhs.data());
  return true;
}

void SchurJacobiPreconditioner::RightMultiply(const double* x,
                                              double* y) const {
  CHECK_NOTNULL(x);
  CHECK_NOTNULL(y);

  const double* lhs_values =
      down_cast<BlockRandomAccessDiagonalMatrix*>(m_.get())->matrix()->values();

  // This loop can be easily multi-threaded with OpenMP if need be.
  for (int i = 0; i < block_size_.size(); ++i) {
    const int block_size = block_size_[i];
    ConstMatrixRef block(lhs_values, block_size, block_size);

    VectorRef(y, block_size) =
        block
        .selfadjointView<Eigen::Upper>()
        .llt()
        .solve(ConstVectorRef(x, block_size));

    x += block_size;
    y += block_size;
    lhs_values += block_size * block_size;
  }
}

int SchurJacobiPreconditioner::num_rows() const {
  return m_->num_rows();
}

}  // namespace internal
}  // namespace ceres
