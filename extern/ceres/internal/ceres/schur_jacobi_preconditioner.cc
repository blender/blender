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
// Author: sameeragarwal@google.com (Sameer Agarwal)

#include "ceres/schur_jacobi_preconditioner.h"

#include <utility>
#include <vector>

#include "ceres/block_random_access_diagonal_matrix.h"
#include "ceres/block_sparse_matrix.h"
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
  CHECK_GT(num_blocks, 0) << "Jacobian should have at least 1 f_block for "
                          << "SCHUR_JACOBI preconditioner.";
  CHECK(options_.context != NULL);

  std::vector<int> blocks(num_blocks);
  for (int i = 0; i < num_blocks; ++i) {
    blocks[i] = bs.cols[i + options_.elimination_groups[0]].size;
  }

  m_.reset(new BlockRandomAccessDiagonalMatrix(blocks));
  InitEliminator(bs);
}

SchurJacobiPreconditioner::~SchurJacobiPreconditioner() {}

// Initialize the SchurEliminator.
void SchurJacobiPreconditioner::InitEliminator(
    const CompressedRowBlockStructure& bs) {
  LinearSolver::Options eliminator_options;
  eliminator_options.elimination_groups = options_.elimination_groups;
  eliminator_options.num_threads = options_.num_threads;
  eliminator_options.e_block_size = options_.e_block_size;
  eliminator_options.f_block_size = options_.f_block_size;
  eliminator_options.row_block_size = options_.row_block_size;
  eliminator_options.context = options_.context;
  eliminator_.reset(SchurEliminatorBase::Create(eliminator_options));
  const bool kFullRankETE = true;
  eliminator_->Init(
      eliminator_options.elimination_groups[0], kFullRankETE, &bs);
}

// Update the values of the preconditioner matrix and factorize it.
bool SchurJacobiPreconditioner::UpdateImpl(const BlockSparseMatrix& A,
                                           const double* D) {
  const int num_rows = m_->num_rows();
  CHECK_GT(num_rows, 0);

  // Compute a subset of the entries of the Schur complement.
  eliminator_->Eliminate(
      BlockSparseMatrixData(A), nullptr, D, m_.get(), nullptr);
  m_->Invert();
  return true;
}

void SchurJacobiPreconditioner::RightMultiply(const double* x,
                                              double* y) const {
  m_->RightMultiply(x, y);
}

int SchurJacobiPreconditioner::num_rows() const { return m_->num_rows(); }

}  // namespace internal
}  // namespace ceres
