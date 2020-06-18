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
// Author: keir@google.com (Keir Mierle)

#include "ceres/cgnr_solver.h"

#include "ceres/block_jacobi_preconditioner.h"
#include "ceres/cgnr_linear_operator.h"
#include "ceres/conjugate_gradients_solver.h"
#include "ceres/internal/eigen.h"
#include "ceres/linear_solver.h"
#include "ceres/subset_preconditioner.h"
#include "ceres/wall_time.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

CgnrSolver::CgnrSolver(const LinearSolver::Options& options)
    : options_(options) {
  if (options_.preconditioner_type != JACOBI &&
      options_.preconditioner_type != IDENTITY &&
      options_.preconditioner_type != SUBSET) {
    LOG(FATAL)
        << "Preconditioner = "
        << PreconditionerTypeToString(options_.preconditioner_type) << ". "
        << "Congratulations, you found a bug in Ceres. Please report it.";
  }
}

CgnrSolver::~CgnrSolver() {}

LinearSolver::Summary CgnrSolver::SolveImpl(
    BlockSparseMatrix* A,
    const double* b,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double* x) {
  EventLogger event_logger("CgnrSolver::Solve");

  // Form z = Atb.
  Vector z(A->num_cols());
  z.setZero();
  A->LeftMultiply(b, z.data());

  if (!preconditioner_) {
    if (options_.preconditioner_type == JACOBI) {
      preconditioner_.reset(new BlockJacobiPreconditioner(*A));
    } else if (options_.preconditioner_type == SUBSET) {
      Preconditioner::Options preconditioner_options;
      preconditioner_options.type = SUBSET;
      preconditioner_options.subset_preconditioner_start_row_block =
          options_.subset_preconditioner_start_row_block;
      preconditioner_options.sparse_linear_algebra_library_type =
          options_.sparse_linear_algebra_library_type;
      preconditioner_options.use_postordering = options_.use_postordering;
      preconditioner_options.num_threads = options_.num_threads;
      preconditioner_options.context = options_.context;
      preconditioner_.reset(
          new SubsetPreconditioner(preconditioner_options, *A));
    }
  }

  if (preconditioner_) {
    preconditioner_->Update(*A, per_solve_options.D);
  }

  LinearSolver::PerSolveOptions cg_per_solve_options = per_solve_options;
  cg_per_solve_options.preconditioner = preconditioner_.get();

  // Solve (AtA + DtD)x = z (= Atb).
  VectorRef(x, A->num_cols()).setZero();
  CgnrLinearOperator lhs(*A, per_solve_options.D);
  event_logger.AddEvent("Setup");

  ConjugateGradientsSolver conjugate_gradient_solver(options_);
  LinearSolver::Summary summary =
      conjugate_gradient_solver.Solve(&lhs, z.data(), cg_per_solve_options, x);
  event_logger.AddEvent("Solve");
  return summary;
}

}  // namespace internal
}  // namespace ceres
