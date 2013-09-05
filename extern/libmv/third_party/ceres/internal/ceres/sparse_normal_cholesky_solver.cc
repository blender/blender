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

#if !defined(CERES_NO_SUITESPARSE) || !defined(CERES_NO_CXSPARSE)

#include "ceres/sparse_normal_cholesky_solver.h"

#include <algorithm>
#include <cstring>
#include <ctime>

#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/cxsparse.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/linear_solver.h"
#include "ceres/suitesparse.h"
#include "ceres/triplet_sparse_matrix.h"
#include "ceres/types.h"
#include "ceres/wall_time.h"

namespace ceres {
namespace internal {

SparseNormalCholeskySolver::SparseNormalCholeskySolver(
    const LinearSolver::Options& options)
    : factor_(NULL),
      cxsparse_factor_(NULL),
      options_(options) {
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
  switch (options_.sparse_linear_algebra_library_type) {
    case SUITE_SPARSE:
      return SolveImplUsingSuiteSparse(A, b, per_solve_options, x);
    case CX_SPARSE:
      return SolveImplUsingCXSparse(A, b, per_solve_options, x);
    default:
      LOG(FATAL) << "Unknown sparse linear algebra library : "
                 << options_.sparse_linear_algebra_library_type;
  }

  LOG(FATAL) << "Unknown sparse linear algebra library : "
             << options_.sparse_linear_algebra_library_type;
  return LinearSolver::Summary();
}

#ifndef CERES_NO_CXSPARSE
LinearSolver::Summary SparseNormalCholeskySolver::SolveImplUsingCXSparse(
    CompressedRowSparseMatrix* A,
    const double* b,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double * x) {
  EventLogger event_logger("SparseNormalCholeskySolver::CXSparse::Solve");

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
  cs_di* A2 = cxsparse_.TransposeMatrix(&At);
  cs_di* AtA = cxsparse_.MatrixMatrixMultiply(&At, A2);

  cxsparse_.Free(A2);
  if (per_solve_options.D != NULL) {
    A->DeleteRows(num_cols);
  }
  event_logger.AddEvent("Setup");

  // Compute symbolic factorization if not available.
  if (cxsparse_factor_ == NULL) {
    if (options_.use_postordering) {
      cxsparse_factor_ =
          CHECK_NOTNULL(cxsparse_.BlockAnalyzeCholesky(AtA,
                                                       A->col_blocks(),
                                                       A->col_blocks()));
    } else {
      cxsparse_factor_ =
          CHECK_NOTNULL(cxsparse_.AnalyzeCholeskyWithNaturalOrdering(AtA));
    }
  }
  event_logger.AddEvent("Analysis");

  // Solve the linear system.
  if (cxsparse_.SolveCholesky(AtA, cxsparse_factor_, Atb.data())) {
    VectorRef(x, Atb.rows()) = Atb;
    summary.termination_type = TOLERANCE;
  }
  event_logger.AddEvent("Solve");

  cxsparse_.Free(AtA);
  event_logger.AddEvent("Teardown");
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
  EventLogger event_logger("SparseNormalCholeskySolver::SuiteSparse::Solve");

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

  cholmod_sparse lhs = ss_.CreateSparseMatrixTransposeView(A);
  cholmod_dense* rhs = ss_.CreateDenseVector(Atb.data(), num_cols, num_cols);
  event_logger.AddEvent("Setup");

  if (factor_ == NULL) {
    if (options_.use_postordering) {
      factor_ =
          CHECK_NOTNULL(ss_.BlockAnalyzeCholesky(&lhs,
                                                 A->col_blocks(),
                                                 A->row_blocks()));
    } else {
      factor_ =
      CHECK_NOTNULL(ss_.AnalyzeCholeskyWithNaturalOrdering(&lhs));
    }
  }

  event_logger.AddEvent("Analysis");

  cholmod_dense* sol = ss_.SolveCholesky(&lhs, factor_, rhs);
  event_logger.AddEvent("Solve");

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

  event_logger.AddEvent("Teardown");
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

#endif  // !defined(CERES_NO_SUITESPARSE) || !defined(CERES_NO_CXSPARSE)
