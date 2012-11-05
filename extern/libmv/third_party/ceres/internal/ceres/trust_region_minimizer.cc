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
// Author: sameeragarwal@google.com (Sameer Agarwal)

#include "ceres/trust_region_minimizer.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "Eigen/Core"
#include "ceres/array_utils.h"
#include "ceres/evaluator.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/linear_least_squares_problems.h"
#include "ceres/sparse_matrix.h"
#include "ceres/trust_region_strategy.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {
namespace {
// Small constant for various floating point issues.
const double kEpsilon = 1e-12;
}  // namespace

// Execute the list of IterationCallbacks sequentially. If any one of
// the callbacks does not return SOLVER_CONTINUE, then stop and return
// its status.
CallbackReturnType TrustRegionMinimizer::RunCallbacks(
    const IterationSummary& iteration_summary) {
  for (int i = 0; i < options_.callbacks.size(); ++i) {
    const CallbackReturnType status =
        (*options_.callbacks[i])(iteration_summary);
    if (status != SOLVER_CONTINUE) {
      return status;
    }
  }
  return SOLVER_CONTINUE;
}

// Compute a scaling vector that is used to improve the conditioning
// of the Jacobian.
void TrustRegionMinimizer::EstimateScale(const SparseMatrix& jacobian,
                                         double* scale) const {
  jacobian.SquaredColumnNorm(scale);
  for (int i = 0; i < jacobian.num_cols(); ++i) {
    scale[i] = 1.0 / (kEpsilon + sqrt(scale[i]));
  }
}

void TrustRegionMinimizer::Init(const Minimizer::Options& options) {
  options_ = options;
  sort(options_.lsqp_iterations_to_dump.begin(),
       options_.lsqp_iterations_to_dump.end());
}

bool TrustRegionMinimizer::MaybeDumpLinearLeastSquaresProblem(
    const int iteration,
    const SparseMatrix* jacobian,
    const double* residuals,
    const double* step) const  {
  // TODO(sameeragarwal): Since the use of trust_region_radius has
  // moved inside TrustRegionStrategy, its not clear how we dump the
  // regularization vector/matrix anymore.
  //
  // Doing this right requires either an API change to the
  // TrustRegionStrategy and/or how LinearLeastSquares problems are
  // stored on disk.
  //
  // For now, we will just not dump the regularizer.
  return (!binary_search(options_.lsqp_iterations_to_dump.begin(),
                         options_.lsqp_iterations_to_dump.end(),
                         iteration) ||
          DumpLinearLeastSquaresProblem(options_.lsqp_dump_directory,
                                        iteration,
                                        options_.lsqp_dump_format_type,
                                        jacobian,
                                        NULL,
                                        residuals,
                                        step,
                                        options_.num_eliminate_blocks));
}

void TrustRegionMinimizer::Minimize(const Minimizer::Options& options,
                                    double* parameters,
                                    Solver::Summary* summary) {
  time_t start_time = time(NULL);
  time_t iteration_start_time =  start_time;
  Init(options);

  summary->termination_type = NO_CONVERGENCE;
  summary->num_successful_steps = 0;
  summary->num_unsuccessful_steps = 0;

  Evaluator* evaluator = CHECK_NOTNULL(options_.evaluator);
  SparseMatrix* jacobian = CHECK_NOTNULL(options_.jacobian);
  TrustRegionStrategy* strategy = CHECK_NOTNULL(options_.trust_region_strategy);

  const int num_parameters = evaluator->NumParameters();
  const int num_effective_parameters = evaluator->NumEffectiveParameters();
  const int num_residuals = evaluator->NumResiduals();

  VectorRef x_min(parameters, num_parameters);
  Vector x = x_min;
  double x_norm = x.norm();

  Vector residuals(num_residuals);
  Vector trust_region_step(num_effective_parameters);
  Vector delta(num_effective_parameters);
  Vector x_plus_delta(num_parameters);
  Vector gradient(num_effective_parameters);
  Vector model_residuals(num_residuals);
  Vector scale(num_effective_parameters);

  IterationSummary iteration_summary;
  iteration_summary.iteration = 0;
  iteration_summary.step_is_valid = false;
  iteration_summary.step_is_successful = false;
  iteration_summary.cost = summary->initial_cost;
  iteration_summary.cost_change = 0.0;
  iteration_summary.gradient_max_norm = 0.0;
  iteration_summary.step_norm = 0.0;
  iteration_summary.relative_decrease = 0.0;
  iteration_summary.trust_region_radius = strategy->Radius();
  // TODO(sameeragarwal): Rename eta to linear_solver_accuracy or
  // something similar across the board.
  iteration_summary.eta = options_.eta;
  iteration_summary.linear_solver_iterations = 0;
  iteration_summary.step_solver_time_in_seconds = 0;

  // Do initial cost and Jacobian evaluation.
  double cost = 0.0;
  if (!evaluator->Evaluate(x.data(), &cost, residuals.data(), NULL, jacobian)) {
    LOG(WARNING) << "Terminating: Residual and Jacobian evaluation failed.";
    summary->termination_type = NUMERICAL_FAILURE;
    return;
  }

  int num_consecutive_nonmonotonic_steps = 0;
  double minimum_cost = cost;
  double reference_cost = cost;
  double accumulated_reference_model_cost_change = 0.0;
  double candidate_cost = cost;
  double accumulated_candidate_model_cost_change = 0.0;

  gradient.setZero();
  jacobian->LeftMultiply(residuals.data(), gradient.data());
  iteration_summary.gradient_max_norm = gradient.lpNorm<Eigen::Infinity>();

  if (options_.jacobi_scaling) {
    EstimateScale(*jacobian, scale.data());
    jacobian->ScaleColumns(scale.data());
  } else {
    scale.setOnes();
  }

  // The initial gradient max_norm is bounded from below so that we do
  // not divide by zero.
  const double gradient_max_norm_0 =
      max(iteration_summary.gradient_max_norm, kEpsilon);
  const double absolute_gradient_tolerance =
      options_.gradient_tolerance * gradient_max_norm_0;

  if (iteration_summary.gradient_max_norm <= absolute_gradient_tolerance) {
    summary->termination_type = GRADIENT_TOLERANCE;
    VLOG(1) << "Terminating: Gradient tolerance reached."
            << "Relative gradient max norm: "
            << iteration_summary.gradient_max_norm / gradient_max_norm_0
            << " <= " << options_.gradient_tolerance;
    return;
  }

  iteration_summary.iteration_time_in_seconds =
      time(NULL) - iteration_start_time;
  iteration_summary.cumulative_time_in_seconds = time(NULL) - start_time +
        summary->preprocessor_time_in_seconds;
  summary->iterations.push_back(iteration_summary);

  // Call the various callbacks.
  switch (RunCallbacks(iteration_summary)) {
    case SOLVER_TERMINATE_SUCCESSFULLY:
      summary->termination_type = USER_SUCCESS;
      VLOG(1) << "Terminating: User callback returned USER_SUCCESS.";
      return;
    case SOLVER_ABORT:
      summary->termination_type = USER_ABORT;
      VLOG(1) << "Terminating: User callback returned  USER_ABORT.";
      return;
    case SOLVER_CONTINUE:
      break;
    default:
      LOG(FATAL) << "Unknown type of user callback status";
  }

  int num_consecutive_invalid_steps = 0;
  while (true) {
    iteration_start_time = time(NULL);
    if (iteration_summary.iteration >= options_.max_num_iterations) {
      summary->termination_type = NO_CONVERGENCE;
      VLOG(1) << "Terminating: Maximum number of iterations reached.";
      break;
    }

    const double total_solver_time = iteration_start_time - start_time +
        summary->preprocessor_time_in_seconds;
    if (total_solver_time >= options_.max_solver_time_in_seconds) {
      summary->termination_type = NO_CONVERGENCE;
      VLOG(1) << "Terminating: Maximum solver time reached.";
      break;
    }

    iteration_summary = IterationSummary();
    iteration_summary = summary->iterations.back();
    iteration_summary.iteration = summary->iterations.back().iteration + 1;
    iteration_summary.step_is_valid = false;
    iteration_summary.step_is_successful = false;

    const time_t strategy_start_time = time(NULL);
    TrustRegionStrategy::PerSolveOptions per_solve_options;
    per_solve_options.eta = options_.eta;
    TrustRegionStrategy::Summary strategy_summary =
        strategy->ComputeStep(per_solve_options,
                              jacobian,
                              residuals.data(),
                              trust_region_step.data());

    iteration_summary.step_solver_time_in_seconds =
        time(NULL) - strategy_start_time;
    iteration_summary.linear_solver_iterations =
        strategy_summary.num_iterations;

    if (!MaybeDumpLinearLeastSquaresProblem(iteration_summary.iteration,
                                            jacobian,
                                            residuals.data(),
                                            trust_region_step.data())) {
      LOG(FATAL) << "Tried writing linear least squares problem: "
                 << options.lsqp_dump_directory << "but failed.";
    }

    double new_model_cost = 0.0;
    if (strategy_summary.termination_type != FAILURE) {
      // new_model_cost = 1/2 |f + J * step|^2
      model_residuals = residuals;
      jacobian->RightMultiply(trust_region_step.data(), model_residuals.data());
      new_model_cost = model_residuals.squaredNorm() / 2.0;

      // In exact arithmetic, this would never be the case. But poorly
      // conditioned matrices can give rise to situations where the
      // new_model_cost can actually be larger than half the squared
      // norm of the residual vector. We allow for small tolerance
      // around cost and beyond that declare the step to be invalid.
      if ((1.0 - new_model_cost / cost) < -kEpsilon) {
        VLOG(1) << "Invalid step: current_cost: " << cost
                << " new_model_cost " << new_model_cost
                << " absolute difference " << (cost - new_model_cost)
                << " relative difference " << (1.0 - new_model_cost/cost);
      } else {
        iteration_summary.step_is_valid = true;
      }
    }

    if (!iteration_summary.step_is_valid) {
      // Invalid steps can happen due to a number of reasons, and we
      // allow a limited number of successive failures, and return with
      // NUMERICAL_FAILURE if this limit is exceeded.
      if (++num_consecutive_invalid_steps >=
          options_.max_num_consecutive_invalid_steps) {
        summary->termination_type = NUMERICAL_FAILURE;
        LOG(WARNING) << "Terminating. Number of successive invalid steps more "
                     << "than "
                     << "Solver::Options::max_num_consecutive_invalid_steps: "
                     << options_.max_num_consecutive_invalid_steps;
        return;
      }

      // We are going to try and reduce the trust region radius and
      // solve again. To do this, we are going to treat this iteration
      // as an unsuccessful iteration. Since the various callbacks are
      // still executed, we are going to fill the iteration summary
      // with data that assumes a step of length zero and no progress.
      iteration_summary.cost = cost;
      iteration_summary.cost_change = 0.0;
      iteration_summary.gradient_max_norm =
          summary->iterations.back().gradient_max_norm;
      iteration_summary.step_norm = 0.0;
      iteration_summary.relative_decrease = 0.0;
      iteration_summary.eta = options_.eta;
    } else {
      // The step is numerically valid, so now we can judge its quality.
      num_consecutive_invalid_steps = 0;

      // We allow some slop around 0, and clamp the model_cost_change
      // at kEpsilon * min(1.0, cost) from below.
      //
      // In exact arithmetic this should never be needed, as we are
      // guaranteed to new_model_cost <= cost. However, due to various
      // numerical issues, it is possible that new_model_cost is
      // nearly equal to cost, and the difference is a small negative
      // number. To make sure that the relative_decrease computation
      // remains sane, as clamp the difference (cost - new_model_cost)
      // from below at a small positive number.
      //
      // This number is the minimum of kEpsilon * (cost, 1.0), which
      // ensures that it will never get too large in absolute value,
      // while scaling down proportionally with the magnitude of the
      // cost. This is important for problems where the minimum of the
      // objective function is near zero.
      const double model_cost_change =
          max(kEpsilon * min(1.0, cost), cost - new_model_cost);

      // Undo the Jacobian column scaling.
      delta = (trust_region_step.array() * scale.array()).matrix();
      iteration_summary.step_norm = delta.norm();

      // Convergence based on parameter_tolerance.
      const double step_size_tolerance =  options_.parameter_tolerance *
          (x_norm + options_.parameter_tolerance);
      if (iteration_summary.step_norm <= step_size_tolerance) {
        VLOG(1) << "Terminating. Parameter tolerance reached. "
                << "relative step_norm: "
                << iteration_summary.step_norm /
            (x_norm + options_.parameter_tolerance)
                << " <= " << options_.parameter_tolerance;
        summary->termination_type = PARAMETER_TOLERANCE;
        return;
      }

      if (!evaluator->Plus(x.data(), delta.data(), x_plus_delta.data())) {
        summary->termination_type = NUMERICAL_FAILURE;
        LOG(WARNING) << "Terminating. Failed to compute "
                     << "Plus(x, delta, x_plus_delta).";
        return;
      }

      // Try this step.
      double new_cost;
      if (!evaluator->Evaluate(x_plus_delta.data(),
                               &new_cost,
                               NULL, NULL, NULL)) {
        // If the evaluation of the new cost fails, treat it as a step
        // with high cost.
        LOG(WARNING) << "Step failed to evaluate. "
                     << "Treating it as step with infinite cost";
        new_cost = numeric_limits<double>::max();
      }

      VLOG(2) << "old cost: " << cost << " new cost: " << new_cost;
      iteration_summary.cost_change =  cost - new_cost;
      const double absolute_function_tolerance =
          options_.function_tolerance * cost;
      if (fabs(iteration_summary.cost_change) < absolute_function_tolerance) {
        VLOG(1) << "Terminating. Function tolerance reached. "
                << "|cost_change|/cost: "
                << fabs(iteration_summary.cost_change) / cost
                << " <= " << options_.function_tolerance;
        summary->termination_type = FUNCTION_TOLERANCE;
        return;
      }

      const double relative_decrease =
          iteration_summary.cost_change / model_cost_change;

      const double historical_relative_decrease =
          (reference_cost - new_cost) /
          (accumulated_reference_model_cost_change + model_cost_change);

      // If monotonic steps are being used, then the relative_decrease
      // is the usual ratio of the change in objective function value
      // divided by the change in model cost.
      //
      // If non-monotonic steps are allowed, then we take the maximum
      // of the relative_decrease and the
      // historical_relative_decrease, which measures the increase
      // from a reference iteration. The model cost change is
      // estimated by accumulating the model cost changes since the
      // reference iteration. The historical relative_decrease offers
      // a boost to a step which is not too bad compared to the
      // reference iteration, allowing for non-monotonic steps.
      iteration_summary.relative_decrease =
          options.use_nonmonotonic_steps
          ? max(relative_decrease, historical_relative_decrease)
          : relative_decrease;

      iteration_summary.step_is_successful =
          iteration_summary.relative_decrease > options_.min_relative_decrease;

      if (iteration_summary.step_is_successful) {
        accumulated_candidate_model_cost_change += model_cost_change;
        accumulated_reference_model_cost_change += model_cost_change;
        if (relative_decrease <= options_.min_relative_decrease) {
          VLOG(2) << "Non-monotonic step! "
                  << " relative_decrease: " << relative_decrease
                  << " historical_relative_decrease: "
                  << historical_relative_decrease;
        }
      }
    }

    if (iteration_summary.step_is_successful) {
      ++summary->num_successful_steps;
      strategy->StepAccepted(iteration_summary.relative_decrease);
      x = x_plus_delta;
      x_norm = x.norm();

      // Step looks good, evaluate the residuals and Jacobian at this
      // point.
      if (!evaluator->Evaluate(x.data(),
                               &cost,
                               residuals.data(),
                               NULL,
                               jacobian)) {
        summary->termination_type = NUMERICAL_FAILURE;
        LOG(WARNING) << "Terminating: Residual and Jacobian evaluation failed.";
        return;
      }

      gradient.setZero();
      jacobian->LeftMultiply(residuals.data(), gradient.data());
      iteration_summary.gradient_max_norm = gradient.lpNorm<Eigen::Infinity>();

      if (iteration_summary.gradient_max_norm <= absolute_gradient_tolerance) {
        summary->termination_type = GRADIENT_TOLERANCE;
        VLOG(1) << "Terminating: Gradient tolerance reached."
                << "Relative gradient max norm: "
                << iteration_summary.gradient_max_norm / gradient_max_norm_0
                << " <= " << options_.gradient_tolerance;
        return;
      }

      if (options_.jacobi_scaling) {
        jacobian->ScaleColumns(scale.data());
      }

      // Update the best, reference and candidate iterates.
      //
      // Based on algorithm 10.1.2 (page 357) of "Trust Region
      // Methods" by Conn Gould & Toint, or equations 33-40 of
      // "Non-monotone trust-region algorithms for nonlinear
      // optimization subject to convex constraints" by Phil Toint,
      // Mathematical Programming, 77, 1997.
      if (cost < minimum_cost) {
        // A step that improves solution quality was found.
        x_min = x;
        minimum_cost = cost;
        // Set the candidate iterate to the current point.
        candidate_cost = cost;
        num_consecutive_nonmonotonic_steps = 0;
        accumulated_candidate_model_cost_change = 0.0;
      } else {
        ++num_consecutive_nonmonotonic_steps;
        if (cost > candidate_cost) {
          // The current iterate is has a higher cost than the
          // candidate iterate. Set the candidate to this point.
          VLOG(2) << "Updating the candidate iterate to the current point.";
          candidate_cost = cost;
          accumulated_candidate_model_cost_change = 0.0;
        }

        // At this point we have made too many non-monotonic steps and
        // we are going to reset the value of the reference iterate so
        // as to force the algorithm to descend.
        //
        // This is the case because the candidate iterate has a value
        // greater than minimum_cost but smaller than the reference
        // iterate.
        if (num_consecutive_nonmonotonic_steps ==
            options.max_consecutive_nonmonotonic_steps) {
          VLOG(2) << "Resetting the reference point to the candidate point";
          reference_cost = candidate_cost;
          accumulated_reference_model_cost_change =
              accumulated_candidate_model_cost_change;
        }
      }
    } else {
      ++summary->num_unsuccessful_steps;
      if (iteration_summary.step_is_valid) {
        strategy->StepRejected(iteration_summary.relative_decrease);
      } else {
        strategy->StepIsInvalid();
      }
    }

    iteration_summary.cost = cost + summary->fixed_cost;
    iteration_summary.trust_region_radius = strategy->Radius();
    if (iteration_summary.trust_region_radius <
        options_.min_trust_region_radius) {
      summary->termination_type = PARAMETER_TOLERANCE;
      VLOG(1) << "Termination. Minimum trust region radius reached.";
      return;
    }

    iteration_summary.iteration_time_in_seconds =
        time(NULL) - iteration_start_time;
    iteration_summary.cumulative_time_in_seconds = time(NULL) - start_time +
        summary->preprocessor_time_in_seconds;
    summary->iterations.push_back(iteration_summary);

    switch (RunCallbacks(iteration_summary)) {
      case SOLVER_TERMINATE_SUCCESSFULLY:
        summary->termination_type = USER_SUCCESS;
        VLOG(1) << "Terminating: User callback returned USER_SUCCESS.";
        return;
      case SOLVER_ABORT:
        summary->termination_type = USER_ABORT;
        VLOG(1) << "Terminating: User callback returned  USER_ABORT.";
        return;
      case SOLVER_CONTINUE:
        break;
      default:
        LOG(FATAL) << "Unknown type of user callback status";
    }
  }
}


}  // namespace internal
}  // namespace ceres
