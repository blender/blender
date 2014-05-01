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

// This include must come before any #ifndef check on Ceres compile options.
#include "ceres/internal/port.h"

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

void SparseNormalCholeskySolver::FreeFactorization() {
#ifndef CERES_NO_SUITESPARSE
  if (factor_ != NULL) {
    ss_.Free(factor_);
    factor_ = NULL;
  }
#endif  // CERES_NO_SUITESPARSE

#ifndef CERES_NO_CXSPARSE
  if (cxsparse_factor_ != NULL) {
    cxsparse_.Free(cxsparse_factor_);
    cxsparse_factor_ = NULL;
  }
#endif  // CERES_NO_CXSPARSE
}

SparseNormalCholeskySolver::~SparseNormalCholeskySolver() {
  FreeFactorization();
}

LinearSolver::Summary SparseNormalCholeskySolver::SolveImpl(
    CompressedRowSparseMatrix* A,
    const double* b,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double * x) {

  const int num_cols = A->num_cols();
  VectorRef(x, num_cols).setZero();
  A->LeftMultiply(b, x);

  if (per_solve_options.D != NULL) {
    // Temporarily append a diagonal block to the A matrix, but undo
    // it before returning the matrix to the user.
    scoped_ptr<CompressedRowSparseMatrix> regularizer;
    if (A->col_blocks().size() > 0) {
      regularizer.reset(CompressedRowSparseMatrix::CreateBlockDiagonalMatrix(
                            per_solve_options.D, A->col_blocks()));
    } else {
      regularizer.reset(new CompressedRowSparseMatrix(
                            per_solve_options.D, num_cols));
    }
    A->AppendRows(*regularizer);
  }

  LinearSolver::Summary summary;
  switch (options_.sparse_linear_algebra_library_type) {
    case SUITE_SPARSE:
      summary = SolveImplUsingSuiteSparse(A, per_solve_options, x);
      break;
    case CX_SPARSE:
      summary = SolveImplUsingCXSparse(A, per_solve_options, x);
      break;
    default:
      LOG(FATAL) << "Unknown sparse linear algebra library : "
                 << options_.sparse_linear_algebra_library_type;
  }

  if (per_solve_options.D != NULL) {
    A->DeleteRows(num_cols);
  }

  return summary;
}

#ifndef CERES_NO_CXSPARSE
LinearSolver::Summary SparseNormalCholeskySolver::SolveImplUsingCXSparse(
    CompressedRowSparseMatrix* A,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double * rhs_and_solution) {
  EventLogger event_logger("SparseNormalCholeskySolver::CXSparse::Solve");

  LinearSolver::Summary summary;
  summary.num_iterations = 1;
  summary.termination_type = LINEAR_SOLVER_SUCCESS;
  summary.message = "Success.";

  // Compute the normal equations. J'J delta = J'f and solve them
  // using a sparse Cholesky factorization. Notice that when compared
  // to SuiteSparse we have to explicitly compute the transpose of Jt,
  // and then the normal equations before they can be
  // factorized. CHOLMOD/SuiteSparse on the other hand can just work
  // off of Jt to compute the Cholesky factorization of the normal
  // equations.
  if (outer_product_.get() == NULL) {
    outer_product_.reset(
        CompressedRowSparseMatrix::CreateOuterProductMatrixAndProgram(
            *A, &pattern_));
  }

  CompressedRowSparseMatrix::ComputeOuterProduct(
      *A, pattern_, outer_product_.get());
  cs_di AtA_view =
      cxsparse_.CreateSparseMatrixTransposeView(outer_product_.get());
  cs_di* AtA = &AtA_view;

  event_logger.AddEvent("Setup");

  // Compute symbolic factorization if not available.
  if (options_.dynamic_sparsity) {
    FreeFactorization();
  }
  if (cxsparse_factor_ == NULL) {
    if (options_.use_postordering) {
      cxsparse_factor_ = cxsparse_.BlockAnalyzeCholesky(AtA,
                                                        A->col_blocks(),
                                                        A->col_blocks());
    } else {
      if (options_.dynamic_sparsity) {
        cxsparse_factor_ = cxsparse_.AnalyzeCholesky(AtA);
      } else {
        cxsparse_factor_ = cxsparse_.AnalyzeCholeskyWithNaturalOrdering(AtA);
      }
    }
  }
  event_logger.AddEvent("Analysis");

  if (cxsparse_factor_ == NULL) {
    summary.termination_type = LINEAR_SOLVER_FATAL_ERROR;
    summary.message =
        "CXSparse failure. Unable to find symbolic factorization.";
  } else if (!cxsparse_.SolveCholesky(AtA, cxsparse_factor_, rhs_and_solution)) {
    summary.termination_type = LINEAR_SOLVER_FAILURE;
  }
  event_logger.AddEvent("Solve");

  return summary;
}
#else
LinearSolver::Summary SparseNormalCholeskySolver::SolveImplUsingCXSparse(
    CompressedRowSparseMatrix* A,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double * rhs_and_solution) {
  LOG(FATAL) << "No CXSparse support in Ceres.";

  // Unreachable but MSVC does not know this.
  return LinearSolver::Summary();
}
#endif

#ifndef CERES_NO_SUITESPARSE
LinearSolver::Summary SparseNormalCholeskySolver::SolveImplUsingSuiteSparse(
    CompressedRowSparseMatrix* A,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double * rhs_and_solution) {
  EventLogger event_logger("SparseNormalCholeskySolver::SuiteSparse::Solve");
  LinearSolver::Summary summary;
  summary.termination_type = LINEAR_SOLVER_SUCCESS;
  summary.num_iterations = 1;
  summary.message = "Success.";

  const int num_cols = A->num_cols();
  cholmod_sparse lhs = ss_.CreateSparseMatrixTransposeView(A);
  event_logger.AddEvent("Setup");

  if (options_.dynamic_sparsity) {
    FreeFactorization();
  }
  if (factor_ == NULL) {
    if (options_.use_postordering) {
      factor_ = ss_.BlockAnalyzeCholesky(&lhs,
                                         A->col_blocks(),
                                         A->row_blocks(),
                                         &summary.message);
    } else {
      if (options_.dynamic_sparsity) {
        factor_ = ss_.AnalyzeCholesky(&lhs, &summary.message);
      } else {
        factor_ = ss_.AnalyzeCholeskyWithNaturalOrdering(&lhs, &summary.message);
      }
    }
  }
  event_logger.AddEvent("Analysis");

  if (factor_ == NULL) {
    summary.termination_type = LINEAR_SOLVER_FATAL_ERROR;
    return summary;
  }

  summary.termination_type = ss_.Cholesky(&lhs, factor_, &summary.message);
  if (summary.termination_type != LINEAR_SOLVER_SUCCESS) {
    return summary;
  }

  cholmod_dense* rhs = ss_.CreateDenseVector(rhs_and_solution, num_cols, num_cols);
  cholmod_dense* solution = ss_.Solve(factor_, rhs, &summary.message);
  event_logger.AddEvent("Solve");

  ss_.Free(rhs);
  if (solution != NULL) {
    memcpy(rhs_and_solution, solution->x, num_cols * sizeof(*rhs_and_solution));
    ss_.Free(solution);
  } else {
    summary.termination_type = LINEAR_SOLVER_FAILURE;
  }

  event_logger.AddEvent("Teardown");
  return summary;
}
#else
LinearSolver::Summary SparseNormalCholeskySolver::SolveImplUsingSuiteSparse(
    CompressedRowSparseMatrix* A,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double * rhs_and_solution) {
  LOG(FATAL) << "No SuiteSparse support in Ceres.";

  // Unreachable but MSVC does not know this.
  return LinearSolver::Summary();
}
#endif

}   // namespace internal
}   // namespace ceres

#endif  // !defined(CERES_NO_SUITESPARSE) || !defined(CERES_NO_CXSPARSE)
