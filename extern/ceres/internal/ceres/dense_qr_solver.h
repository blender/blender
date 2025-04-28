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
//
// Solve dense rectangular systems Ax = b using the QR factorization.
#ifndef CERES_INTERNAL_DENSE_QR_SOLVER_H_
#define CERES_INTERNAL_DENSE_QR_SOLVER_H_

#include <memory>

#include "ceres/dense_qr.h"
#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/export.h"
#include "ceres/linear_solver.h"

namespace ceres::internal {

class DenseSparseMatrix;

// This class implements the LinearSolver interface for solving
// rectangular/unsymmetric (well constrained) linear systems of the
// form
//
//   Ax = b
//
// Since there does not usually exist a solution that satisfies these
// equations, the solver instead solves the linear least squares
// problem
//
//   min_x |Ax - b|^2
//
// The solution strategy is based on computing the QR decomposition of
// A, i.e.
//
//   A = QR
//
// Where Q is an orthonormal matrix and R is an upper triangular
// matrix. Then
//
//     Ax = b
//    QRx = b
//  Q'QRx = Q'b
//     Rx = Q'b
//      x = R^{-1} Q'b
//
// If the PerSolveOptions struct has a non-null array D, then the
// augmented/regularized linear system
//
//   [    A    ]x = [b]
//   [ diag(D) ]    [0]
//
// is solved.
//
// This class uses the dense QR factorization routines from the Eigen
// library. This solver always returns a solution, it is the user's
// responsibility to judge if the solution is good enough for their
// purposes.
class CERES_NO_EXPORT DenseQRSolver final : public DenseSparseMatrixSolver {
 public:
  explicit DenseQRSolver(const LinearSolver::Options& options);

 private:
  LinearSolver::Summary SolveImpl(
      DenseSparseMatrix* A,
      const double* b,
      const LinearSolver::PerSolveOptions& per_solve_options,
      double* x) final;

  LinearSolver::Summary SolveUsingEigen(
      DenseSparseMatrix* A,
      const double* b,
      const LinearSolver::PerSolveOptions& per_solve_options,
      double* x);

  LinearSolver::Summary SolveUsingLAPACK(
      DenseSparseMatrix* A,
      const double* b,
      const LinearSolver::PerSolveOptions& per_solve_options,
      double* x);

  const LinearSolver::Options options_;
  ColMajorMatrix lhs_;
  Vector rhs_;
  std::unique_ptr<DenseQR> dense_qr_;
};

}  // namespace ceres::internal

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_INTERNAL_DENSE_QR_SOLVER_H_
