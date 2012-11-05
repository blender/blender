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

#include "ceres/sparse_normal_cholesky_solver.h"

#include <algorithm>
#include <cstring>
#include <ctime>

#ifndef CERES_NO_CXSPARSE
#include "cs.h"
#endif

#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/linear_solver.h"
#include "ceres/suitesparse.h"
#include "ceres/triplet_sparse_matrix.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/types.h"

namespace ceres {
namespace internal {

SparseNormalCholeskySolver::SparseNormalCholeskySolver(
    const LinearSolver::Options& options)
    : options_(options) {
#ifndef CERES_NO_SUITESPARSE
  factor_ = NULL;
#endif

#ifndef CERES_NO_CXSPARSE
  cxsparse_factor_ = NULL;
#endif  // CERES_NO_CXSPARSE
}

SparseNormalCholeskySolver::~SparseNormalCholeskySolver() {
#ifndef CERES_NO_SUITESPARSE
  if (factor_ != NULL) {
    ss_.Free(factor_);
    factor_ = NULL;
  }
#endif

#ifndef CERES_NO_CXSPARSE
  if (cxsparse_factor_ != NULL) {
    cxsparse_.Free(cxsparse_factor_);
    cxsparse_factor_ = NULL;
  }
#endif  // CERES_NO_CXSPARSE
}

LinearSolver::Summary SparseNormalCholeskySolver::SolveImpl(
    CompressedRowSparseMatrix* A,
    const double* b,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double * x) {
  switch (options_.sparse_linear_algebra_library) {
    case SUITE_SPARSE:
      return SolveImplUsingSuiteSparse(A, b, per_solve_options, x);
    case CX_SPARSE:
      return SolveImplUsingCXSparse(A, b, per_solve_options, x);
    default:
      LOG(FATAL) << "Unknown sparse linear algebra library : "
                 << options_.sparse_linear_algebra_library;
  }

  LOG(FATAL) << "Unknown sparse linear algebra library : "
             << options_.sparse_linear_algebra_library;
  return LinearSolver::Summary();
}

#ifndef CERES_NO_CXSPARSE
LinearSolver::Summary SparseNormalCholeskySolver::SolveImplUsingCXSparse(
    CompressedRowSparseMatrix* A,
    const double* b,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double * x) {
  LinearSolver::Summary summary;
  summary.num_iterations = 1;
  const int num_cols = A->num_cols();
  Vector Atb = Vector::Zero(num_cols);
  A->LeftMultiply(b, Atb.data());

  if (per_solve_options.D != NULL) {
    // Temporarily append a diagonal block to the A matrix, but undo
    // it before returning the matrix to the user.
    CompressedRowSparseMatrix D(per_solve_options.D, num_cols);
    A->AppendRows(D);
  }

  VectorRef(x, num_cols).setZero();

  // Wrap the augmented Jacobian in a compressed sparse column matrix.
  cs_di At = cxsparse_.CreateSparseMatrixTransposeView(A);

  // Compute the normal equations. J'J delta = J'f and solve them
  // using a sparse Cholesky factorization. Notice that when compared
  // to SuiteSparse we have to explicitly compute the transpose of Jt,
  // and then the normal equations before they can be
  // factorized. CHOLMOD/SuiteSparse on the other hand can just work
  // off of Jt to compute the Cholesky factorization of the normal
  // equations.
  cs_di* A2 = cs_transpose(&At, 1);
  cs_di* AtA = cs_multiply(&At,A2);

  cxsparse_.Free(A2);
  if (per_solve_options.D != NULL) {
    A->DeleteRows(num_cols);
  }

  // Compute symbolic factorization if not available.
  if (cxsparse_factor_ == NULL) {
    cxsparse_factor_ = CHECK_NOTNULL(cxsparse_.AnalyzeCholesky(AtA));
  }

  // Solve the linear system.
  if (cxsparse_.SolveCholesky(AtA, cxsparse_factor_, Atb.data())) {
    VectorRef(x, Atb.rows()) = Atb;
    summary.termination_type = TOLERANCE;
  }

  cxsparse_.Free(AtA);
  return summary;
}
#else
LinearSolver::Summary SparseNormalCholeskySolver::SolveImplUsingCXSparse(
    CompressedRowSparseMatrix* A,
    const double* b,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double * x) {
  LOG(FATAL) << "No CXSparse support in Ceres.";

  // Unreachable but MSVC does not know this.
  return LinearSolver::Summary();
}
#endif

#ifndef CERES_NO_SUITESPARSE
LinearSolver::Summary SparseNormalCholeskySolver::SolveImplUsingSuiteSparse(
    CompressedRowSparseMatrix* A,
    const double* b,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double * x) {
  const time_t start_time = time(NULL);
  const int num_cols = A->num_cols();

  LinearSolver::Summary summary;
  Vector Atb = Vector::Zero(num_cols);
  A->LeftMultiply(b, Atb.data());

  if (per_solve_options.D != NULL) {
    // Temporarily append a diagonal block to the A matrix, but undo it before
    // returning the matrix to the user.
    CompressedRowSparseMatrix D(per_solve_options.D, num_cols);
    A->AppendRows(D);
  }

  VectorRef(x, num_cols).setZero();

  scoped_ptr<cholmod_sparse> lhs(ss_.CreateSparseMatrixTransposeView(A));
  CHECK_NOTNULL(lhs.get());

  cholmod_dense* rhs = ss_.CreateDenseVector(Atb.data(), num_cols, num_cols);
  const time_t init_time = time(NULL);

  if (factor_ == NULL) {
    if (options_.use_block_amd) {
      factor_ = ss_.BlockAnalyzeCholesky(lhs.get(),
                                         A->col_blocks(),
                                         A->row_blocks());
    } else {
      factor_ = ss_.AnalyzeCholesky(lhs.get());
    }

    if (VLOG_IS_ON(2)) {
      cholmod_print_common("Symbolic Analysis", ss_.mutable_cc());
    }
  }

  CHECK_NOTNULL(factor_);

  const time_t symbolic_time = time(NULL);

  cholmod_dense* sol = ss_.SolveCholesky(lhs.get(), factor_, rhs);
  const time_t solve_time = time(NULL);

  ss_.Free(rhs);
  rhs = NULL;

  if (per_solve_options.D != NULL) {
    A->DeleteRows(num_cols);
  }

  summary.num_iterations = 1;
  if (sol != NULL) {
    memcpy(x, sol->x, num_cols * sizeof(*x));

    ss_.Free(sol);
    sol = NULL;
    summary.termination_type = TOLERANCE;
  }

  const time_t cleanup_time = time(NULL);
  VLOG(2) << "time (sec) total: " << (cleanup_time - start_time)
          << " init: " << (init_time - start_time)
          << " symbolic: " << (symbolic_time - init_time)
          << " solve: " << (solve_time - symbolic_time)
          << " cleanup: " << (cleanup_time - solve_time);
  return summary;
}
#else
LinearSolver::Summary SparseNormalCholeskySolver::SolveImplUsingSuiteSparse(
    CompressedRowSparseMatrix* A,
    const double* b,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double * x) {
  LOG(FATAL) << "No SuiteSparse support in Ceres.";

  // Unreachable but MSVC does not know this.
  return LinearSolver::Summary();
}
#endif

}   // namespace internal
}   // namespace ceres
