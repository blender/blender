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

#include "ceres/dynamic_sparse_normal_cholesky_solver.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <memory>
#include <sstream>
#include <utility>

#include "Eigen/SparseCore"
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/internal/config.h"
#include "ceres/internal/eigen.h"
#include "ceres/linear_solver.h"
#include "ceres/suitesparse.h"
#include "ceres/triplet_sparse_matrix.h"
#include "ceres/types.h"
#include "ceres/wall_time.h"

#ifdef CERES_USE_EIGEN_SPARSE
#include "Eigen/SparseCholesky"
#endif

namespace ceres::internal {

DynamicSparseNormalCholeskySolver::DynamicSparseNormalCholeskySolver(
    LinearSolver::Options options)
    : options_(std::move(options)) {}

LinearSolver::Summary DynamicSparseNormalCholeskySolver::SolveImpl(
    CompressedRowSparseMatrix* A,
    const double* b,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double* x) {
  const int num_cols = A->num_cols();
  VectorRef(x, num_cols).setZero();
  A->LeftMultiplyAndAccumulate(b, x);

  if (per_solve_options.D != nullptr) {
    // Temporarily append a diagonal block to the A matrix, but undo
    // it before returning the matrix to the user.
    std::unique_ptr<CompressedRowSparseMatrix> regularizer;
    if (!A->col_blocks().empty()) {
      regularizer = CompressedRowSparseMatrix::CreateBlockDiagonalMatrix(
          per_solve_options.D, A->col_blocks());
    } else {
      regularizer = std::make_unique<CompressedRowSparseMatrix>(
          per_solve_options.D, num_cols);
    }
    A->AppendRows(*regularizer);
  }

  LinearSolver::Summary summary;
  switch (options_.sparse_linear_algebra_library_type) {
    case SUITE_SPARSE:
      summary = SolveImplUsingSuiteSparse(A, x);
      break;
    case EIGEN_SPARSE:
      summary = SolveImplUsingEigen(A, x);
      break;
    default:
      LOG(FATAL) << "Unsupported sparse linear algebra library for "
                 << "dynamic sparsity: "
                 << SparseLinearAlgebraLibraryTypeToString(
                        options_.sparse_linear_algebra_library_type);
  }

  if (per_solve_options.D != nullptr) {
    A->DeleteRows(num_cols);
  }

  return summary;
}

LinearSolver::Summary DynamicSparseNormalCholeskySolver::SolveImplUsingEigen(
    CompressedRowSparseMatrix* A, double* rhs_and_solution) {
#ifndef CERES_USE_EIGEN_SPARSE

  LinearSolver::Summary summary;
  summary.num_iterations = 0;
  summary.termination_type = LinearSolverTerminationType::FATAL_ERROR;
  summary.message =
      "SPARSE_NORMAL_CHOLESKY cannot be used with EIGEN_SPARSE "
      "because Ceres was not built with support for "
      "Eigen's SimplicialLDLT decomposition. "
      "This requires enabling building with -DEIGENSPARSE=ON.";
  return summary;

#else

  EventLogger event_logger("DynamicSparseNormalCholeskySolver::Eigen::Solve");

  Eigen::Map<Eigen::SparseMatrix<double, Eigen::RowMajor>> a(
      A->num_rows(),
      A->num_cols(),
      A->num_nonzeros(),
      A->mutable_rows(),
      A->mutable_cols(),
      A->mutable_values());

  Eigen::SparseMatrix<double> lhs = a.transpose() * a;
  Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> solver;

  LinearSolver::Summary summary;
  summary.num_iterations = 1;
  summary.termination_type = LinearSolverTerminationType::SUCCESS;
  summary.message = "Success.";

  solver.analyzePattern(lhs);
  if (VLOG_IS_ON(2)) {
    std::stringstream ss;
    solver.dumpMemory(ss);
    VLOG(2) << "Symbolic Analysis\n" << ss.str();
  }

  event_logger.AddEvent("Analyze");
  if (solver.info() != Eigen::Success) {
    summary.termination_type = LinearSolverTerminationType::FATAL_ERROR;
    summary.message = "Eigen failure. Unable to find symbolic factorization.";
    return summary;
  }

  solver.factorize(lhs);
  event_logger.AddEvent("Factorize");
  if (solver.info() != Eigen::Success) {
    summary.termination_type = LinearSolverTerminationType::FAILURE;
    summary.message = "Eigen failure. Unable to find numeric factorization.";
    return summary;
  }

  const Vector rhs = VectorRef(rhs_and_solution, lhs.cols());
  VectorRef(rhs_and_solution, lhs.cols()) = solver.solve(rhs);
  event_logger.AddEvent("Solve");
  if (solver.info() != Eigen::Success) {
    summary.termination_type = LinearSolverTerminationType::FAILURE;
    summary.message = "Eigen failure. Unable to do triangular solve.";
    return summary;
  }

  return summary;
#endif  // CERES_USE_EIGEN_SPARSE
}

LinearSolver::Summary
DynamicSparseNormalCholeskySolver::SolveImplUsingSuiteSparse(
    CompressedRowSparseMatrix* A, double* rhs_and_solution) {
#ifdef CERES_NO_SUITESPARSE
  (void)A;
  (void)rhs_and_solution;

  LinearSolver::Summary summary;
  summary.num_iterations = 0;
  summary.termination_type = LinearSolverTerminationType::FATAL_ERROR;
  summary.message =
      "SPARSE_NORMAL_CHOLESKY cannot be used with SUITE_SPARSE "
      "because Ceres was not built with support for SuiteSparse. "
      "This requires enabling building with -DSUITESPARSE=ON.";
  return summary;

#else

  EventLogger event_logger(
      "DynamicSparseNormalCholeskySolver::SuiteSparse::Solve");
  LinearSolver::Summary summary;
  summary.termination_type = LinearSolverTerminationType::SUCCESS;
  summary.num_iterations = 1;
  summary.message = "Success.";

  SuiteSparse ss;
  const int num_cols = A->num_cols();
  cholmod_sparse lhs = ss.CreateSparseMatrixTransposeView(A);
  event_logger.AddEvent("Setup");
  cholmod_factor* factor =
      ss.AnalyzeCholesky(&lhs, options_.ordering_type, &summary.message);
  event_logger.AddEvent("Analysis");

  if (factor == nullptr) {
    summary.termination_type = LinearSolverTerminationType::FATAL_ERROR;
    return summary;
  }

  summary.termination_type = ss.Cholesky(&lhs, factor, &summary.message);
  if (summary.termination_type == LinearSolverTerminationType::SUCCESS) {
    cholmod_dense cholmod_rhs =
        ss.CreateDenseVectorView(rhs_and_solution, num_cols);
    cholmod_dense* solution = ss.Solve(factor, &cholmod_rhs, &summary.message);
    event_logger.AddEvent("Solve");
    if (solution != nullptr) {
      memcpy(
          rhs_and_solution, solution->x, num_cols * sizeof(*rhs_and_solution));
      ss.Free(solution);
    } else {
      summary.termination_type = LinearSolverTerminationType::FAILURE;
    }
  }

  ss.Free(factor);
  event_logger.AddEvent("Teardown");
  return summary;

#endif
}

}  // namespace ceres::internal
