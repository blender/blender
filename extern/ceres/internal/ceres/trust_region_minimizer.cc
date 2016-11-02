// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2016 Google Inc. All rights reserved.
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

#include "ceres/trust_region_minimizer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "Eigen/Core"
#include "ceres/array_utils.h"
#include "ceres/coordinate_descent_minimizer.h"
#include "ceres/evaluator.h"
#include "ceres/file.h"
#include "ceres/line_search.h"
#include "ceres/stringprintf.h"
#include "ceres/types.h"
#include "ceres/wall_time.h"
#include "glog/logging.h"

// Helper macro to simplify some of the control flow.
#define RETURN_IF_ERROR_AND_LOG(expr)                            \
  do {                                                           \
    if (!(expr)) {                                               \
      LOG(ERROR) << "Terminating: " << solver_summary_->message; \
      return;                                                    \
    }                                                            \
  } while (0)

namespace ceres {
namespace internal {

TrustRegionMinimizer::~TrustRegionMinimizer() {}

void TrustRegionMinimizer::Minimize(const Minimizer::Options& options,
                                    double* parameters,
                                    Solver::Summary* solver_summary) {
  start_time_in_secs_ = WallTimeInSeconds();
  iteration_start_time_in_secs_ = start_time_in_secs_;
  Init(options, parameters, solver_summary);
  RETURN_IF_ERROR_AND_LOG(IterationZero());

  // Create the TrustRegionStepEvaluator. The construction needs to be
  // delayed to this point because we need the cost for the starting
  // point to initialize the step evaluator.
  step_evaluator_.reset(new TrustRegionStepEvaluator(
      x_cost_,
      options_.use_nonmonotonic_steps
          ? options_.max_consecutive_nonmonotonic_steps
          : 0));

  while (FinalizeIterationAndCheckIfMinimizerCanContinue()) {
    iteration_start_time_in_secs_ = WallTimeInSeconds();
    iteration_summary_ = IterationSummary();
    iteration_summary_.iteration =
        solver_summary->iterations.back().iteration + 1;

    RETURN_IF_ERROR_AND_LOG(ComputeTrustRegionStep());
    if (!iteration_summary_.step_is_valid) {
      RETURN_IF_ERROR_AND_LOG(HandleInvalidStep());
      continue;
    }

    if (options_.is_constrained) {
      // Use a projected line search to enforce the bounds constraints
      // and improve the quality of the step.
      DoLineSearch(x_, gradient_, x_cost_, &delta_);
    }

    ComputeCandidatePointAndEvaluateCost();
    DoInnerIterationsIfNeeded();

    if (ParameterToleranceReached()) {
      return;
    }

    if (FunctionToleranceReached()) {
      return;
    }

    if (IsStepSuccessful()) {
      RETURN_IF_ERROR_AND_LOG(HandleSuccessfulStep());
      continue;
    }

    HandleUnsuccessfulStep();
  }
}

// Initialize the minimizer, allocate working space and set some of
// the fields in the solver_summary.
void TrustRegionMinimizer::Init(const Minimizer::Options& options,
                                double* parameters,
                                Solver::Summary* solver_summary) {
  options_ = options;
  sort(options_.trust_region_minimizer_iterations_to_dump.begin(),
       options_.trust_region_minimizer_iterations_to_dump.end());

  parameters_ = parameters;

  solver_summary_ = solver_summary;
  solver_summary_->termination_type = NO_CONVERGENCE;
  solver_summary_->num_successful_steps = 0;
  solver_summary_->num_unsuccessful_steps = 0;
  solver_summary_->is_constrained = options.is_constrained;

  evaluator_ = CHECK_NOTNULL(options_.evaluator.get());
  jacobian_ = CHECK_NOTNULL(options_.jacobian.get());
  strategy_ = CHECK_NOTNULL(options_.trust_region_strategy.get());

  is_not_silent_ = !options.is_silent;
  inner_iterations_are_enabled_ =
      options.inner_iteration_minimizer.get() != NULL;
  inner_iterations_were_useful_ = false;

  num_parameters_ = evaluator_->NumParameters();
  num_effective_parameters_ = evaluator_->NumEffectiveParameters();
  num_residuals_ = evaluator_->NumResiduals();
  num_consecutive_invalid_steps_ = 0;

  x_ = ConstVectorRef(parameters_, num_parameters_);
  x_norm_ = x_.norm();
  residuals_.resize(num_residuals_);
  trust_region_step_.resize(num_effective_parameters_);
  delta_.resize(num_effective_parameters_);
  candidate_x_.resize(num_parameters_);
  gradient_.resize(num_effective_parameters_);
  model_residuals_.resize(num_residuals_);
  negative_gradient_.resize(num_effective_parameters_);
  projected_gradient_step_.resize(num_parameters_);

  // By default scaling is one, if the user requests Jacobi scaling of
  // the Jacobian, we will compute and overwrite this vector.
  jacobian_scaling_ = Vector::Ones(num_effective_parameters_);

  x_norm_ = -1;  // Invalid value
  x_cost_ = std::numeric_limits<double>::max();
  minimum_cost_ = x_cost_;
  model_cost_change_ = 0.0;
}

// 1. Project the initial solution onto the feasible set if needed.
// 2. Compute the initial cost, jacobian & gradient.
//
// Return true if all computations can be performed successfully.
bool TrustRegionMinimizer::IterationZero() {
  iteration_summary_ = IterationSummary();
  iteration_summary_.iteration = 0;
  iteration_summary_.step_is_valid = false;
  iteration_summary_.step_is_successful = false;
  iteration_summary_.cost_change = 0.0;
  iteration_summary_.gradient_max_norm = 0.0;
  iteration_summary_.gradient_norm = 0.0;
  iteration_summary_.step_norm = 0.0;
  iteration_summary_.relative_decrease = 0.0;
  iteration_summary_.eta = options_.eta;
  iteration_summary_.linear_solver_iterations = 0;
  iteration_summary_.step_solver_time_in_seconds = 0;

  if (options_.is_constrained) {
    delta_.setZero();
    if (!evaluator_->Plus(x_.data(), delta_.data(), candidate_x_.data())) {
      solver_summary_->message =
          "Unable to project initial point onto the feasible set.";
      solver_summary_->termination_type = FAILURE;
      return false;
    }

    x_ = candidate_x_;
    x_norm_ = x_.norm();
  }

  if (!EvaluateGradientAndJacobian()) {
    return false;
  }

  solver_summary_->initial_cost = x_cost_ + solver_summary_->fixed_cost;
  iteration_summary_.step_is_valid = true;
  iteration_summary_.step_is_successful = true;
  return true;
}

// For the current x_, compute
//
//  1. Cost
//  2. Jacobian
//  3. Gradient
//  4. Scale the Jacobian if needed (and compute the scaling if we are
//     in iteration zero).
//  5. Compute the 2 and max norm of the gradient.
//
// Returns true if all computations could be performed
// successfully. Any failures are considered fatal and the
// Solver::Summary is updated to indicate this.
bool TrustRegionMinimizer::EvaluateGradientAndJacobian() {
  if (!evaluator_->Evaluate(x_.data(),
                            &x_cost_,
                            residuals_.data(),
                            gradient_.data(),
                            jacobian_)) {
    solver_summary_->message = "Residual and Jacobian evaluation failed.";
    solver_summary_->termination_type = FAILURE;
    return false;
  }

  iteration_summary_.cost = x_cost_ + solver_summary_->fixed_cost;

  if (options_.jacobi_scaling) {
    if (iteration_summary_.iteration == 0) {
      // Compute a scaling vector that is used to improve the
      // conditioning of the Jacobian.
      //
      // jacobian_scaling_ = diag(J'J)^{-1}
      jacobian_->SquaredColumnNorm(jacobian_scaling_.data());
      for (int i = 0; i < jacobian_->num_cols(); ++i) {
        // Add one to the denominator to prevent division by zero.
        jacobian_scaling_[i] = 1.0 / (1.0 + sqrt(jacobian_scaling_[i]));
      }
    }

    // jacobian = jacobian * diag(J'J) ^{-1}
    jacobian_->ScaleColumns(jacobian_scaling_.data());
  }

  // The gradient exists in the local tangent space. To account for
  // the bounds constraints correctly, instead of just computing the
  // norm of the gradient vector, we compute
  //
  // |Plus(x, -gradient) - x|
  //
  // Where the Plus operator lifts the negative gradient to the
  // ambient space, adds it to x and projects it on the hypercube
  // defined by the bounds.
  negative_gradient_ = -gradient_;
  if (!evaluator_->Plus(x_.data(),
                        negative_gradient_.data(),
                        projected_gradient_step_.data())) {
    solver_summary_->message =
        "projected_gradient_step = Plus(x, -gradient) failed.";
    solver_summary_->termination_type = FAILURE;
    return false;
  }

  iteration_summary_.gradient_max_norm =
      (x_ - projected_gradient_step_).lpNorm<Eigen::Infinity>();
  iteration_summary_.gradient_norm = (x_ - projected_gradient_step_).norm();
  return true;
}

// 1. Add the final timing information to the iteration summary.
// 2. Run the callbacks
// 3. Check for termination based on
//    a. Run time
//    b. Iteration count
//    c. Max norm of the gradient
//    d. Size of the trust region radius.
//
// Returns true if user did not terminate the solver and none of these
// termination criterion are met.
bool TrustRegionMinimizer::FinalizeIterationAndCheckIfMinimizerCanContinue() {
  if (iteration_summary_.step_is_successful) {
    ++solver_summary_->num_successful_steps;
    if (x_cost_ < minimum_cost_) {
      minimum_cost_ = x_cost_;
      VectorRef(parameters_, num_parameters_) = x_;
      iteration_summary_.step_is_nonmonotonic = false;
    } else {
      iteration_summary_.step_is_nonmonotonic = true;
    }
  } else {
    ++solver_summary_->num_unsuccessful_steps;
  }

  iteration_summary_.trust_region_radius = strategy_->Radius();
  iteration_summary_.iteration_time_in_seconds =
      WallTimeInSeconds() - iteration_start_time_in_secs_;
  iteration_summary_.cumulative_time_in_seconds =
      WallTimeInSeconds() - start_time_in_secs_ +
      solver_summary_->preprocessor_time_in_seconds;

  solver_summary_->iterations.push_back(iteration_summary_);

  if (!RunCallbacks(options_, iteration_summary_, solver_summary_)) {
    return false;
  }

  if (MaxSolverTimeReached()) {
    return false;
  }

  if (MaxSolverIterationsReached()) {
    return false;
  }

  if (GradientToleranceReached()) {
    return false;
  }

  if (MinTrustRegionRadiusReached()) {
    return false;
  }

  return true;
}

// Compute the trust region step using the TrustRegionStrategy chosen
// by the user.
//
// If the strategy returns with LINEAR_SOLVER_FATAL_ERROR, which
// indicates an unrecoverable error, return false. This is the only
// condition that returns false.
//
// If the strategy returns with LINEAR_SOLVER_FAILURE, which indicates
// a numerical failure that could be recovered from by retrying
// (e.g. by increasing the strength of the regularization), we set
// iteration_summary_.step_is_valid to false and return true.
//
// In all other cases, we compute the decrease in the trust region
// model problem. In exact arithmetic, this should always be
// positive, but due to numerical problems in the TrustRegionStrategy
// or round off error when computing the decrease it may be
// negative. In which case again, we set
// iteration_summary_.step_is_valid to false.
bool TrustRegionMinimizer::ComputeTrustRegionStep() {
  const double strategy_start_time = WallTimeInSeconds();
  iteration_summary_.step_is_valid = false;
  TrustRegionStrategy::PerSolveOptions per_solve_options;
  per_solve_options.eta = options_.eta;
  if (find(options_.trust_region_minimizer_iterations_to_dump.begin(),
           options_.trust_region_minimizer_iterations_to_dump.end(),
           iteration_summary_.iteration) !=
      options_.trust_region_minimizer_iterations_to_dump.end()) {
    per_solve_options.dump_format_type =
        options_.trust_region_problem_dump_format_type;
    per_solve_options.dump_filename_base =
        JoinPath(options_.trust_region_problem_dump_directory,
                 StringPrintf("ceres_solver_iteration_%03d",
                              iteration_summary_.iteration));
  }

  TrustRegionStrategy::Summary strategy_summary =
      strategy_->ComputeStep(per_solve_options,
                             jacobian_,
                             residuals_.data(),
                             trust_region_step_.data());

  if (strategy_summary.termination_type == LINEAR_SOLVER_FATAL_ERROR) {
    solver_summary_->message =
        "Linear solver failed due to unrecoverable "
        "non-numeric causes. Please see the error log for clues. ";
    solver_summary_->termination_type = FAILURE;
    return false;
  }

  iteration_summary_.step_solver_time_in_seconds =
      WallTimeInSeconds() - strategy_start_time;
  iteration_summary_.linear_solver_iterations = strategy_summary.num_iterations;

  if (strategy_summary.termination_type == LINEAR_SOLVER_FAILURE) {
    return true;
  }

  // new_model_cost
  //  = 1/2 [f + J * step]^2
  //  = 1/2 [ f'f + 2f'J * step + step' * J' * J * step ]
  // model_cost_change
  //  = cost - new_model_cost
  //  = f'f/2  - 1/2 [ f'f + 2f'J * step + step' * J' * J * step]
  //  = -f'J * step - step' * J' * J * step / 2
  //  = -(J * step)'(f + J * step / 2)
  model_residuals_.setZero();
  jacobian_->RightMultiply(trust_region_step_.data(), model_residuals_.data());
  model_cost_change_ =
      -model_residuals_.dot(residuals_ + model_residuals_ / 2.0);

  // TODO(sameeragarwal)
  //
  //  1. What happens if model_cost_change_ = 0
  //  2. What happens if -epsilon <= model_cost_change_ < 0 for some
  //     small epsilon due to round off error.
  iteration_summary_.step_is_valid = (model_cost_change_ > 0.0);
  if (iteration_summary_.step_is_valid) {
    // Undo the Jacobian column scaling.
    delta_ = (trust_region_step_.array() * jacobian_scaling_.array()).matrix();
    num_consecutive_invalid_steps_ = 0;
  }

  VLOG_IF(1, is_not_silent_ && !iteration_summary_.step_is_valid)
      << "Invalid step: current_cost: " << x_cost_
      << " absolute model cost change: " << model_cost_change_
      << " relative model cost change: " << (model_cost_change_ / x_cost_);
  return true;
}

// Invalid steps can happen due to a number of reasons, and we allow a
// limited number of consecutive failures, and return false if this
// limit is exceeded.
bool TrustRegionMinimizer::HandleInvalidStep() {
  // TODO(sameeragarwal): Should we be returning FAILURE or
  // NO_CONVERGENCE? The solution value is still usable in many cases,
  // it is not clear if we should declare the solver a failure
  // entirely. For example the case where model_cost_change ~ 0.0, but
  // just slightly negative.
  if (++num_consecutive_invalid_steps_ >=
      options_.max_num_consecutive_invalid_steps) {
    solver_summary_->message = StringPrintf(
        "Number of consecutive invalid steps more "
        "than Solver::Options::max_num_consecutive_invalid_steps: %d",
        options_.max_num_consecutive_invalid_steps);
    solver_summary_->termination_type = FAILURE;
    return false;
  }

  strategy_->StepIsInvalid();

  // We are going to try and reduce the trust region radius and
  // solve again. To do this, we are going to treat this iteration
  // as an unsuccessful iteration. Since the various callbacks are
  // still executed, we are going to fill the iteration summary
  // with data that assumes a step of length zero and no progress.
  iteration_summary_.cost = x_cost_ + solver_summary_->fixed_cost;
  iteration_summary_.cost_change = 0.0;
  iteration_summary_.gradient_max_norm =
      solver_summary_->iterations.back().gradient_max_norm;
  iteration_summary_.gradient_norm =
      solver_summary_->iterations.back().gradient_norm;
  iteration_summary_.step_norm = 0.0;
  iteration_summary_.relative_decrease = 0.0;
  iteration_summary_.eta = options_.eta;
  return true;
}

// Use the supplied coordinate descent minimizer to perform inner
// iterations and compute the improvement due to it. Returns the cost
// after performing the inner iterations.
//
// The optimization is performed with candidate_x_ as the starting
// point, and if the optimization is successful, candidate_x_ will be
// updated with the optimized parameters.
void TrustRegionMinimizer::DoInnerIterationsIfNeeded() {
  inner_iterations_were_useful_ = false;
  if (!inner_iterations_are_enabled_ ||
      candidate_cost_ >= std::numeric_limits<double>::max()) {
    return;
  }

  double inner_iteration_start_time = WallTimeInSeconds();
  ++solver_summary_->num_inner_iteration_steps;
  inner_iteration_x_ = candidate_x_;
  Solver::Summary inner_iteration_summary;
  options_.inner_iteration_minimizer->Minimize(
      options_, inner_iteration_x_.data(), &inner_iteration_summary);
  double inner_iteration_cost;
  if (!evaluator_->Evaluate(
          inner_iteration_x_.data(), &inner_iteration_cost, NULL, NULL, NULL)) {
    VLOG_IF(2, is_not_silent_) << "Inner iteration failed.";
    return;
  }

  VLOG_IF(2, is_not_silent_)
      << "Inner iteration succeeded; Current cost: " << x_cost_
      << " Trust region step cost: " << candidate_cost_
      << " Inner iteration cost: " << inner_iteration_cost;

  candidate_x_ = inner_iteration_x_;

  // Normally, the quality of a trust region step is measured by
  // the ratio
  //
  //              cost_change
  //    r =    -----------------
  //           model_cost_change
  //
  // All the change in the nonlinear objective is due to the trust
  // region step so this ratio is a good measure of the quality of
  // the trust region radius. However, when inner iterations are
  // being used, cost_change includes the contribution of the
  // inner iterations and its not fair to credit it all to the
  // trust region algorithm. So we change the ratio to be
  //
  //                              cost_change
  //    r =    ------------------------------------------------
  //           (model_cost_change + inner_iteration_cost_change)
  //
  // Practically we do this by increasing model_cost_change by
  // inner_iteration_cost_change.

  const double inner_iteration_cost_change =
      candidate_cost_ - inner_iteration_cost;
  model_cost_change_ += inner_iteration_cost_change;
  inner_iterations_were_useful_ = inner_iteration_cost < x_cost_;
  const double inner_iteration_relative_progress =
      1.0 - inner_iteration_cost / candidate_cost_;

  // Disable inner iterations once the relative improvement
  // drops below tolerance.
  inner_iterations_are_enabled_ =
      (inner_iteration_relative_progress > options_.inner_iteration_tolerance);
  VLOG_IF(2, is_not_silent_ && !inner_iterations_are_enabled_)
      << "Disabling inner iterations. Progress : "
      << inner_iteration_relative_progress;
  candidate_cost_ = inner_iteration_cost;

  solver_summary_->inner_iteration_time_in_seconds +=
      WallTimeInSeconds() - inner_iteration_start_time;
}

// Perform a projected line search to improve the objective function
// value along delta.
//
// TODO(sameeragarwal): The current implementation does not do
// anything illegal but is incorrect and not terribly effective.
//
// https://github.com/ceres-solver/ceres-solver/issues/187
void TrustRegionMinimizer::DoLineSearch(const Vector& x,
                                        const Vector& gradient,
                                        const double cost,
                                        Vector* delta) {
  LineSearchFunction line_search_function(evaluator_);

  LineSearch::Options line_search_options;
  line_search_options.is_silent = true;
  line_search_options.interpolation_type =
      options_.line_search_interpolation_type;
  line_search_options.min_step_size = options_.min_line_search_step_size;
  line_search_options.sufficient_decrease =
      options_.line_search_sufficient_function_decrease;
  line_search_options.max_step_contraction =
      options_.max_line_search_step_contraction;
  line_search_options.min_step_contraction =
      options_.min_line_search_step_contraction;
  line_search_options.max_num_iterations =
      options_.max_num_line_search_step_size_iterations;
  line_search_options.sufficient_curvature_decrease =
      options_.line_search_sufficient_curvature_decrease;
  line_search_options.max_step_expansion =
      options_.max_line_search_step_expansion;
  line_search_options.function = &line_search_function;

  std::string message;
  scoped_ptr<LineSearch> line_search(CHECK_NOTNULL(
      LineSearch::Create(ceres::ARMIJO, line_search_options, &message)));
  LineSearch::Summary line_search_summary;
  line_search_function.Init(x, *delta);
  line_search->Search(1.0, cost, gradient.dot(*delta), &line_search_summary);

  solver_summary_->num_line_search_steps += line_search_summary.num_iterations;
  solver_summary_->line_search_cost_evaluation_time_in_seconds +=
      line_search_summary.cost_evaluation_time_in_seconds;
  solver_summary_->line_search_gradient_evaluation_time_in_seconds +=
      line_search_summary.gradient_evaluation_time_in_seconds;
  solver_summary_->line_search_polynomial_minimization_time_in_seconds +=
      line_search_summary.polynomial_minimization_time_in_seconds;
  solver_summary_->line_search_total_time_in_seconds +=
      line_search_summary.total_time_in_seconds;

  if (line_search_summary.success) {
    *delta *= line_search_summary.optimal_step_size;
  }
}

// Check if the maximum amount of time allowed by the user for the
// solver has been exceeded, and if so return false after updating
// Solver::Summary::message.
bool TrustRegionMinimizer::MaxSolverTimeReached() {
  const double total_solver_time =
      WallTimeInSeconds() - start_time_in_secs_ +
      solver_summary_->preprocessor_time_in_seconds;
  if (total_solver_time < options_.max_solver_time_in_seconds) {
    return false;
  }

  solver_summary_->message = StringPrintf("Maximum solver time reached. "
                                          "Total solver time: %e >= %e.",
                                          total_solver_time,
                                          options_.max_solver_time_in_seconds);
  solver_summary_->termination_type = NO_CONVERGENCE;
  VLOG_IF(1, is_not_silent_) << "Terminating: " << solver_summary_->message;
  return true;
}

// Check if the maximum number of iterations allowed by the user for
// the solver has been exceeded, and if so return false after updating
// Solver::Summary::message.
bool TrustRegionMinimizer::MaxSolverIterationsReached() {
  if (iteration_summary_.iteration < options_.max_num_iterations) {
    return false;
  }

  solver_summary_->message =
      StringPrintf("Maximum number of iterations reached. "
                   "Number of iterations: %d.",
                   iteration_summary_.iteration);

  solver_summary_->termination_type = NO_CONVERGENCE;
  VLOG_IF(1, is_not_silent_) << "Terminating: " << solver_summary_->message;
  return true;
}

// Check convergence based on the max norm of the gradient (only for
// iterations where the step was declared successful).
bool TrustRegionMinimizer::GradientToleranceReached() {
  if (!iteration_summary_.step_is_successful ||
      iteration_summary_.gradient_max_norm > options_.gradient_tolerance) {
    return false;
  }

  solver_summary_->message = StringPrintf(
      "Gradient tolerance reached. "
      "Gradient max norm: %e <= %e",
      iteration_summary_.gradient_max_norm,
      options_.gradient_tolerance);
  solver_summary_->termination_type = CONVERGENCE;
  VLOG_IF(1, is_not_silent_) << "Terminating: " << solver_summary_->message;
  return true;
}

// Check convergence based the size of the trust region radius.
bool TrustRegionMinimizer::MinTrustRegionRadiusReached() {
  if (iteration_summary_.trust_region_radius >
      options_.min_trust_region_radius) {
    return false;
  }

  solver_summary_->message =
      StringPrintf("Minimum trust region radius reached. "
                   "Trust region radius: %e <= %e",
                   iteration_summary_.trust_region_radius,
                   options_.min_trust_region_radius);
  solver_summary_->termination_type = CONVERGENCE;
  VLOG_IF(1, is_not_silent_) << "Terminating: " << solver_summary_->message;
  return true;
}

// Solver::Options::parameter_tolerance based convergence check.
bool TrustRegionMinimizer::ParameterToleranceReached() {
  // Compute the norm of the step in the ambient space.
  iteration_summary_.step_norm = (x_ - candidate_x_).norm();
  const double step_size_tolerance =
      options_.parameter_tolerance * (x_norm_ + options_.parameter_tolerance);

  if (iteration_summary_.step_norm > step_size_tolerance) {
    return false;
  }

  solver_summary_->message = StringPrintf(
      "Parameter tolerance reached. "
      "Relative step_norm: %e <= %e.",
      (iteration_summary_.step_norm / (x_norm_ + options_.parameter_tolerance)),
      options_.parameter_tolerance);
  solver_summary_->termination_type = CONVERGENCE;
  VLOG_IF(1, is_not_silent_) << "Terminating: " << solver_summary_->message;
  return true;
}

// Solver::Options::function_tolerance based convergence check.
bool TrustRegionMinimizer::FunctionToleranceReached() {
  iteration_summary_.cost_change = x_cost_ - candidate_cost_;
  const double absolute_function_tolerance =
      options_.function_tolerance * x_cost_;

  if (fabs(iteration_summary_.cost_change) > absolute_function_tolerance) {
    return false;
  }

  solver_summary_->message = StringPrintf(
      "Function tolerance reached. "
      "|cost_change|/cost: %e <= %e",
      fabs(iteration_summary_.cost_change) / x_cost_,
      options_.function_tolerance);
  solver_summary_->termination_type = CONVERGENCE;
  VLOG_IF(1, is_not_silent_) << "Terminating: " << solver_summary_->message;
  return true;
}

// Compute candidate_x_ = Plus(x_, delta_)
// Evaluate the cost of candidate_x_ as candidate_cost_.
//
// Failure to compute the step or the cost mean that candidate_cost_
// is set to std::numeric_limits<double>::max(). Unlike
// EvaluateGradientAndJacobian, failure in this function is not fatal
// as we are only computing and evaluating a candidate point, and if
// for some reason we are unable to evaluate it, we consider it to be
// a point with very high cost. This allows the user to deal with edge
// cases/constraints as part of the LocalParameterization and
// CostFunction objects.
void TrustRegionMinimizer::ComputeCandidatePointAndEvaluateCost() {
  if (!evaluator_->Plus(x_.data(), delta_.data(), candidate_x_.data())) {
    LOG_IF(WARNING, is_not_silent_)
        << "x_plus_delta = Plus(x, delta) failed. "
        << "Treating it as a step with infinite cost";
    candidate_cost_ = std::numeric_limits<double>::max();
    return;
  }

  if (!evaluator_->Evaluate(
          candidate_x_.data(), &candidate_cost_, NULL, NULL, NULL)) {
    LOG_IF(WARNING, is_not_silent_)
        << "Step failed to evaluate. "
        << "Treating it as a step with infinite cost";
    candidate_cost_ = std::numeric_limits<double>::max();
  }
}

bool TrustRegionMinimizer::IsStepSuccessful() {
  iteration_summary_.relative_decrease =
      step_evaluator_->StepQuality(candidate_cost_, model_cost_change_);

  // In most cases, boosting the model_cost_change by the
  // improvement caused by the inner iterations is fine, but it can
  // be the case that the original trust region step was so bad that
  // the resulting improvement in the cost was negative, and the
  // change caused by the inner iterations was large enough to
  // improve the step, but also to make relative decrease quite
  // small.
  //
  // This can cause the trust region loop to reject this step. To
  // get around this, we expicitly check if the inner iterations
  // led to a net decrease in the objective function value. If
  // they did, we accept the step even if the trust region ratio
  // is small.
  //
  // Notice that we do not just check that cost_change is positive
  // which is a weaker condition and would render the
  // min_relative_decrease threshold useless. Instead, we keep
  // track of inner_iterations_were_useful, which is true only
  // when inner iterations lead to a net decrease in the cost.
  return (inner_iterations_were_useful_ ||
          iteration_summary_.relative_decrease >
              options_.min_relative_decrease);
}

// Declare the step successful, move to candidate_x, update the
// derivatives and let the trust region strategy and the step
// evaluator know that the step has been accepted.
bool TrustRegionMinimizer::HandleSuccessfulStep() {
  x_ = candidate_x_;
  x_norm_ = x_.norm();

  if (!EvaluateGradientAndJacobian()) {
    return false;
  }

  iteration_summary_.step_is_successful = true;
  strategy_->StepAccepted(iteration_summary_.relative_decrease);
  step_evaluator_->StepAccepted(candidate_cost_, model_cost_change_);
  return true;
}

// Declare the step unsuccessful and inform the trust region strategy.
void TrustRegionMinimizer::HandleUnsuccessfulStep() {
  iteration_summary_.step_is_successful = false;
  strategy_->StepRejected(iteration_summary_.relative_decrease);
  iteration_summary_.cost = candidate_cost_ + solver_summary_->fixed_cost;
}

}  // namespace internal
}  // namespace ceres
