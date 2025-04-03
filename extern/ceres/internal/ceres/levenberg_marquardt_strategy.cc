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

#include "ceres/levenberg_marquardt_strategy.h"

#include <algorithm>
#include <cmath>

#include "Eigen/Core"
#include "ceres/array_utils.h"
#include "ceres/internal/eigen.h"
#include "ceres/linear_least_squares_problems.h"
#include "ceres/linear_solver.h"
#include "ceres/parallel_vector_ops.h"
#include "ceres/sparse_matrix.h"
#include "ceres/trust_region_strategy.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres::internal {

LevenbergMarquardtStrategy::LevenbergMarquardtStrategy(
    const TrustRegionStrategy::Options& options)
    : linear_solver_(options.linear_solver),
      radius_(options.initial_radius),
      max_radius_(options.max_radius),
      min_diagonal_(options.min_lm_diagonal),
      max_diagonal_(options.max_lm_diagonal),
      decrease_factor_(2.0),
      reuse_diagonal_(false),
      context_(options.context),
      num_threads_(options.num_threads) {
  CHECK(linear_solver_ != nullptr);
  CHECK_GT(min_diagonal_, 0.0);
  CHECK_LE(min_diagonal_, max_diagonal_);
  CHECK_GT(max_radius_, 0.0);
}

LevenbergMarquardtStrategy::~LevenbergMarquardtStrategy() = default;

TrustRegionStrategy::Summary LevenbergMarquardtStrategy::ComputeStep(
    const TrustRegionStrategy::PerSolveOptions& per_solve_options,
    SparseMatrix* jacobian,
    const double* residuals,
    double* step) {
  CHECK(jacobian != nullptr);
  CHECK(residuals != nullptr);
  CHECK(step != nullptr);

  const int num_parameters = jacobian->num_cols();
  if (!reuse_diagonal_) {
    if (diagonal_.rows() != num_parameters) {
      diagonal_.resize(num_parameters, 1);
    }

    jacobian->SquaredColumnNorm(diagonal_.data(), context_, num_threads_);
    ParallelAssign(context_,
                   num_threads_,
                   diagonal_,
                   diagonal_.array().max(min_diagonal_).min(max_diagonal_));
  }

  if (lm_diagonal_.size() == 0) {
    lm_diagonal_.resize(num_parameters);
  }
  ParallelAssign(
      context_, num_threads_, lm_diagonal_, (diagonal_ / radius_).cwiseSqrt());

  LinearSolver::PerSolveOptions solve_options;
  solve_options.D = lm_diagonal_.data();
  solve_options.q_tolerance = per_solve_options.eta;
  // Disable r_tolerance checking. Since we only care about
  // termination via the q_tolerance. As Nash and Sofer show,
  // r_tolerance based termination is essentially useless in
  // Truncated Newton methods.
  solve_options.r_tolerance = -1.0;

  // Invalidate the output array lm_step, so that we can detect if
  // the linear solver generated numerical garbage.  This is known
  // to happen for the DENSE_QR and then DENSE_SCHUR solver when
  // the Jacobian is severely rank deficient and mu is too small.
  InvalidateArray(num_parameters, step);

  // Instead of solving Jx = -r, solve Jy = r.
  // Then x can be found as x = -y, but the inputs jacobian and residuals
  // do not need to be modified.
  LinearSolver::Summary linear_solver_summary =
      linear_solver_->Solve(jacobian, residuals, solve_options, step);

  if (linear_solver_summary.termination_type ==
      LinearSolverTerminationType::FATAL_ERROR) {
    LOG(WARNING) << "Linear solver fatal error: "
                 << linear_solver_summary.message;
  } else if (linear_solver_summary.termination_type ==
             LinearSolverTerminationType::FAILURE) {
    LOG(WARNING) << "Linear solver failure. Failed to compute a step: "
                 << linear_solver_summary.message;
  } else if (!IsArrayValid(num_parameters, step)) {
    LOG(WARNING) << "Linear solver failure. Failed to compute a finite step.";
    linear_solver_summary.termination_type =
        LinearSolverTerminationType::FAILURE;
  } else {
    VectorRef step_vec(step, num_parameters);
    ParallelAssign(context_, num_threads_, step_vec, -step_vec);
  }
  reuse_diagonal_ = true;

  if (per_solve_options.dump_format_type == CONSOLE ||
      (per_solve_options.dump_format_type != CONSOLE &&
       !per_solve_options.dump_filename_base.empty())) {
    if (!DumpLinearLeastSquaresProblem(per_solve_options.dump_filename_base,
                                       per_solve_options.dump_format_type,
                                       jacobian,
                                       solve_options.D,
                                       residuals,
                                       step,
                                       0)) {
      LOG(ERROR) << "Unable to dump trust region problem."
                 << " Filename base: " << per_solve_options.dump_filename_base;
    }
  }

  TrustRegionStrategy::Summary summary;
  summary.residual_norm = linear_solver_summary.residual_norm;
  summary.num_iterations = linear_solver_summary.num_iterations;
  summary.termination_type = linear_solver_summary.termination_type;
  return summary;
}

void LevenbergMarquardtStrategy::StepAccepted(double step_quality) {
  CHECK_GT(step_quality, 0.0);
  radius_ =
      radius_ / std::max(1.0 / 3.0, 1.0 - pow(2.0 * step_quality - 1.0, 3));
  radius_ = std::min(max_radius_, radius_);
  decrease_factor_ = 2.0;
  reuse_diagonal_ = false;
}

void LevenbergMarquardtStrategy::StepRejected(double /*step_quality*/) {
  radius_ = radius_ / decrease_factor_;
  decrease_factor_ *= 2.0;
  reuse_diagonal_ = true;
}

double LevenbergMarquardtStrategy::Radius() const { return radius_; }

}  // namespace ceres::internal
