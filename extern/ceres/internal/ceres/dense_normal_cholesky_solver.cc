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

#include "ceres/dense_normal_cholesky_solver.h"

#include <cstddef>

#include "Eigen/Dense"
#include "ceres/blas.h"
#include "ceres/dense_sparse_matrix.h"
#include "ceres/internal/eigen.h"
#include "ceres/lapack.h"
#include "ceres/linear_solver.h"
#include "ceres/types.h"
#include "ceres/wall_time.h"

namespace ceres {
namespace internal {

DenseNormalCholeskySolver::DenseNormalCholeskySolver(
    const LinearSolver::Options& options)
    : options_(options) {}

LinearSolver::Summary DenseNormalCholeskySolver::SolveImpl(
    DenseSparseMatrix* A,
    const double* b,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double* x) {
  if (options_.dense_linear_algebra_library_type == EIGEN) {
    return SolveUsingEigen(A, b, per_solve_options, x);
  } else {
    return SolveUsingLAPACK(A, b, per_solve_options, x);
  }
}

LinearSolver::Summary DenseNormalCholeskySolver::SolveUsingEigen(
    DenseSparseMatrix* A,
    const double* b,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double* x) {
  EventLogger event_logger("DenseNormalCholeskySolver::Solve");

  const int num_rows = A->num_rows();
  const int num_cols = A->num_cols();

  ConstColMajorMatrixRef Aref = A->matrix();
  Matrix lhs(num_cols, num_cols);
  lhs.setZero();

  event_logger.AddEvent("Setup");

  //   lhs += A'A
  //
  // Using rankUpdate instead of GEMM, exposes the fact that its the
  // same matrix being multiplied with itself and that the product is
  // symmetric.
  lhs.selfadjointView<Eigen::Upper>().rankUpdate(Aref.transpose());

  //   rhs = A'b
  Vector rhs = Aref.transpose() * ConstVectorRef(b, num_rows);

  if (per_solve_options.D != NULL) {
    ConstVectorRef D(per_solve_options.D, num_cols);
    lhs += D.array().square().matrix().asDiagonal();
  }
  event_logger.AddEvent("Product");

  LinearSolver::Summary summary;
  summary.num_iterations = 1;
  summary.termination_type = LINEAR_SOLVER_SUCCESS;
  Eigen::LLT<Matrix, Eigen::Upper> llt =
      lhs.selfadjointView<Eigen::Upper>().llt();

  if (llt.info() != Eigen::Success) {
    summary.termination_type = LINEAR_SOLVER_FAILURE;
    summary.message = "Eigen LLT decomposition failed.";
  } else {
    summary.termination_type = LINEAR_SOLVER_SUCCESS;
    summary.message = "Success.";
  }

  VectorRef(x, num_cols) = llt.solve(rhs);
  event_logger.AddEvent("Solve");
  return summary;
}

LinearSolver::Summary DenseNormalCholeskySolver::SolveUsingLAPACK(
    DenseSparseMatrix* A,
    const double* b,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double* x) {
  EventLogger event_logger("DenseNormalCholeskySolver::Solve");

  if (per_solve_options.D != NULL) {
    // Temporarily append a diagonal block to the A matrix, but undo
    // it before returning the matrix to the user.
    A->AppendDiagonal(per_solve_options.D);
  }

  const int num_cols = A->num_cols();
  Matrix lhs(num_cols, num_cols);
  event_logger.AddEvent("Setup");

  // lhs = A'A
  //
  // Note: This is a bit delicate, it assumes that the stride on this
  // matrix is the same as the number of rows.
  BLAS::SymmetricRankKUpdate(A->num_rows(),
                             num_cols,
                             A->values(),
                             true,
                             1.0,
                             0.0,
                             lhs.data());

  if (per_solve_options.D != NULL) {
    // Undo the modifications to the matrix A.
    A->RemoveDiagonal();
  }

  // TODO(sameeragarwal): Replace this with a gemv call for true blasness.
  //   rhs = A'b
  VectorRef(x, num_cols) =
      A->matrix().transpose() * ConstVectorRef(b, A->num_rows());
  event_logger.AddEvent("Product");

  LinearSolver::Summary summary;
  summary.num_iterations = 1;
  summary.termination_type =
      LAPACK::SolveInPlaceUsingCholesky(num_cols,
                                        lhs.data(),
                                        x,
                                        &summary.message);
  event_logger.AddEvent("Solve");
  return summary;
}
}   // namespace internal
}   // namespace ceres
