// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2012 Google Inc. All rights reserved.
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
// Author: keir@google.com (Keir Mierle)

#include "ceres/cgnr_solver.h"

#include "glog/logging.h"
#include "ceres/linear_solver.h"
#include "ceres/cgnr_linear_operator.h"
#include "ceres/conjugate_gradients_solver.h"
#include "ceres/block_jacobi_preconditioner.h"

namespace ceres {
namespace internal {

CgnrSolver::CgnrSolver(const LinearSolver::Options& options)
  : options_(options),
    jacobi_preconditioner_(NULL) {
}

LinearSolver::Summary CgnrSolver::Solve(
    LinearOperator* A,
    const double* b,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double* x) {
  // Form z = Atb.
  scoped_array<double> z(new double[A->num_cols()]);
  std::fill(z.get(), z.get() + A->num_cols(), 0.0);
  A->LeftMultiply(b, z.get());

  // Precondition if necessary.
  LinearSolver::PerSolveOptions cg_per_solve_options = per_solve_options;
  if (options_.preconditioner_type == JACOBI) {
    if (jacobi_preconditioner_.get() == NULL) {
      jacobi_preconditioner_.reset(new BlockJacobiPreconditioner(*A));
    }
    jacobi_preconditioner_->Update(*A, per_solve_options.D);
    cg_per_solve_options.preconditioner = jacobi_preconditioner_.get();
  } else if (options_.preconditioner_type != IDENTITY) {
    LOG(FATAL) << "CGNR only supports IDENTITY and JACOBI preconditioners.";
  }

  // Solve (AtA + DtD)x = z (= Atb).
  std::fill(x, x + A->num_cols(), 0.0);
  CgnrLinearOperator lhs(*A, per_solve_options.D);
  ConjugateGradientsSolver conjugate_gradient_solver(options_);
  return conjugate_gradient_solver.Solve(&lhs,
                                         z.get(),
                                         cg_per_solve_options,
                                         x);
}

}  // namespace internal
}  // namespace ceres
