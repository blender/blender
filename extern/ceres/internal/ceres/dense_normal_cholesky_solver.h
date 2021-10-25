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
//
// Solve dense rectangular systems Ax = b by forming the normal
// equations and solving them using the Cholesky factorization.

#ifndef CERES_INTERNAL_DENSE_NORMAL_CHOLESKY_SOLVER_H_
#define CERES_INTERNAL_DENSE_NORMAL_CHOLESKY_SOLVER_H_

#include "ceres/linear_solver.h"
#include "ceres/internal/macros.h"

namespace ceres {
namespace internal {

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
// Setting the gradient of the above optimization problem to zero
// gives us the normal equations
//
//   A'Ax = A'b
//
// A'A is a positive definite matrix (hopefully), and the resulting
// linear system can be solved using Cholesky factorization.
//
// If the PerSolveOptions struct has a non-null array D, then the
// augmented/regularized linear system
//
//   [    A    ]x = [b]
//   [ diag(D) ]    [0]
//
// is solved.
//
// This class uses the LDLT factorization routines from the Eigen
// library. This solver always returns a solution, it is the user's
// responsibility to judge if the solution is good enough for their
// purposes.
class DenseNormalCholeskySolver: public DenseSparseMatrixSolver {
 public:
  explicit DenseNormalCholeskySolver(const LinearSolver::Options& options);

 private:
  virtual LinearSolver::Summary SolveImpl(
      DenseSparseMatrix* A,
      const double* b,
      const LinearSolver::PerSolveOptions& per_solve_options,
      double* x);

  LinearSolver::Summary SolveUsingLAPACK(
      DenseSparseMatrix* A,
      const double* b,
      const LinearSolver::PerSolveOptions& per_solve_options,
      double* x);

  LinearSolver::Summary SolveUsingEigen(
      DenseSparseMatrix* A,
      const double* b,
      const LinearSolver::PerSolveOptions& per_solve_options,
      double* x);

  const LinearSolver::Options options_;
  CERES_DISALLOW_COPY_AND_ASSIGN(DenseNormalCholeskySolver);
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_DENSE_NORMAL_CHOLESKY_SOLVER_H_
