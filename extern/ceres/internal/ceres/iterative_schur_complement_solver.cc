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

#include "ceres/iterative_schur_complement_solver.h"

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

#include "Eigen/Dense"
#include "ceres/block_sparse_matrix.h"
#include "ceres/block_structure.h"
#include "ceres/conjugate_gradients_solver.h"
#include "ceres/detect_structure.h"
#include "ceres/implicit_schur_complement.h"
#include "ceres/internal/eigen.h"
#include "ceres/linear_solver.h"
#include "ceres/power_series_expansion_preconditioner.h"
#include "ceres/preconditioner.h"
#include "ceres/schur_jacobi_preconditioner.h"
#include "ceres/triplet_sparse_matrix.h"
#include "ceres/types.h"
#include "ceres/visibility_based_preconditioner.h"
#include "ceres/wall_time.h"
#include "glog/logging.h"

namespace ceres::internal {

IterativeSchurComplementSolver::IterativeSchurComplementSolver(
    LinearSolver::Options options)
    : options_(std::move(options)) {}

IterativeSchurComplementSolver::~IterativeSchurComplementSolver() = default;

LinearSolver::Summary IterativeSchurComplementSolver::SolveImpl(
    BlockSparseMatrix* A,
    const double* b,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double* x) {
  EventLogger event_logger("IterativeSchurComplementSolver::Solve");

  CHECK(A->block_structure() != nullptr);
  CHECK(A->transpose_block_structure() != nullptr);

  const int num_eliminate_blocks = options_.elimination_groups[0];
  // Initialize a ImplicitSchurComplement object.
  if (schur_complement_ == nullptr) {
    DetectStructure(*(A->block_structure()),
                    num_eliminate_blocks,
                    &options_.row_block_size,
                    &options_.e_block_size,
                    &options_.f_block_size);
    schur_complement_ = std::make_unique<ImplicitSchurComplement>(options_);
  }
  schur_complement_->Init(*A, per_solve_options.D, b);

  const int num_schur_complement_blocks =
      A->block_structure()->cols.size() - num_eliminate_blocks;
  if (num_schur_complement_blocks == 0) {
    VLOG(2) << "No parameter blocks left in the schur complement.";
    LinearSolver::Summary summary;
    summary.num_iterations = 0;
    summary.termination_type = LinearSolverTerminationType::SUCCESS;
    schur_complement_->BackSubstitute(nullptr, x);
    return summary;
  }

  // Initialize the solution to the Schur complement system.
  reduced_linear_system_solution_.resize(schur_complement_->num_rows());
  reduced_linear_system_solution_.setZero();
  if (options_.use_spse_initialization) {
    Preconditioner::Options preconditioner_options(options_);
    preconditioner_options.type = SCHUR_POWER_SERIES_EXPANSION;
    PowerSeriesExpansionPreconditioner pse_solver(
        schur_complement_.get(),
        options_.max_num_spse_iterations,
        options_.spse_tolerance,
        preconditioner_options);
    pse_solver.RightMultiplyAndAccumulate(
        schur_complement_->rhs().data(),
        reduced_linear_system_solution_.data());
  }

  CreatePreconditioner(A);
  if (preconditioner_ != nullptr) {
    if (!preconditioner_->Update(*A, per_solve_options.D)) {
      LinearSolver::Summary summary;
      summary.num_iterations = 0;
      summary.termination_type = LinearSolverTerminationType::FAILURE;
      summary.message = "Preconditioner update failed.";
      return summary;
    }
  }

  ConjugateGradientsSolverOptions cg_options;
  cg_options.min_num_iterations = options_.min_num_iterations;
  cg_options.max_num_iterations = options_.max_num_iterations;
  cg_options.residual_reset_period = options_.residual_reset_period;
  cg_options.q_tolerance = per_solve_options.q_tolerance;
  cg_options.r_tolerance = per_solve_options.r_tolerance;

  LinearOperatorAdapter lhs(*schur_complement_);
  LinearOperatorAdapter preconditioner(*preconditioner_);

  Vector scratch[4];
  for (int i = 0; i < 4; ++i) {
    scratch[i].resize(schur_complement_->num_cols());
  }
  Vector* scratch_ptr[4] = {&scratch[0], &scratch[1], &scratch[2], &scratch[3]};

  event_logger.AddEvent("Setup");

  LinearSolver::Summary summary =
      ConjugateGradientsSolver(cg_options,
                               lhs,
                               schur_complement_->rhs(),
                               preconditioner,
                               scratch_ptr,
                               reduced_linear_system_solution_);

  if (summary.termination_type != LinearSolverTerminationType::FAILURE &&
      summary.termination_type != LinearSolverTerminationType::FATAL_ERROR) {
    schur_complement_->BackSubstitute(reduced_linear_system_solution_.data(),
                                      x);
  }
  event_logger.AddEvent("Solve");
  return summary;
}

void IterativeSchurComplementSolver::CreatePreconditioner(
    BlockSparseMatrix* A) {
  if (preconditioner_ != nullptr) {
    return;
  }

  Preconditioner::Options preconditioner_options(options_);
  CHECK(options_.context != nullptr);

  switch (options_.preconditioner_type) {
    case IDENTITY:
      preconditioner_ = std::make_unique<IdentityPreconditioner>(
          schur_complement_->num_cols());
      break;
    case JACOBI:
      preconditioner_ = std::make_unique<SparseMatrixPreconditionerWrapper>(
          schur_complement_->block_diagonal_FtF_inverse(),
          preconditioner_options);
      break;
    case SCHUR_POWER_SERIES_EXPANSION:
      // Ignoring the value of spse_tolerance to ensure preconditioner stays
      // fixed during the iterations of cg.
      preconditioner_ = std::make_unique<PowerSeriesExpansionPreconditioner>(
          schur_complement_.get(),
          options_.max_num_spse_iterations,
          0,
          preconditioner_options);
      break;
    case SCHUR_JACOBI:
      preconditioner_ = std::make_unique<SchurJacobiPreconditioner>(
          *A->block_structure(), preconditioner_options);
      break;
    case CLUSTER_JACOBI:
    case CLUSTER_TRIDIAGONAL:
      preconditioner_ = std::make_unique<VisibilityBasedPreconditioner>(
          *A->block_structure(), preconditioner_options);
      break;
    default:
      LOG(FATAL) << "Unknown Preconditioner Type";
  }
};

}  // namespace ceres::internal
