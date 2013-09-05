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
//
// Generic loop for line search based optimization algorithms.
//
// This is primarily inpsired by the minFunc packaged written by Mark
// Schmidt.
//
// http://www.di.ens.fr/~mschmidt/Software/minFunc.html
//
// For details on the theory and implementation see "Numerical
// Optimization" by Nocedal & Wright.

#ifndef CERES_NO_LINE_SEARCH_MINIMIZER

#include "ceres/line_search_minimizer.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>

#include "Eigen/Dense"
#include "ceres/array_utils.h"
#include "ceres/evaluator.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/port.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/line_search.h"
#include "ceres/line_search_direction.h"
#include "ceres/stringprintf.h"
#include "ceres/types.h"
#include "ceres/wall_time.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {
namespace {
// Small constant for various floating point issues.
// TODO(sameeragarwal): Change to a better name if this has only one
// use.
const double kEpsilon = 1e-12;

bool Evaluate(Evaluator* evaluator,
              const Vector& x,
              LineSearchMinimizer::State* state) {
  const bool status = evaluator->Evaluate(x.data(),
                                          &(state->cost),
                                          NULL,
                                          state->gradient.data(),
                                          NULL);
  if (status) {
    state->gradient_squared_norm = state->gradient.squaredNorm();
    state->gradient_max_norm = state->gradient.lpNorm<Eigen::Infinity>();
  }

  return status;
}

}  // namespace

void LineSearchMinimizer::Minimize(const Minimizer::Options& options,
                                   double* parameters,
                                   Solver::Summary* summary) {
  double start_time = WallTimeInSeconds();
  double iteration_start_time =  start_time;

  Evaluator* evaluator = CHECK_NOTNULL(options.evaluator);
  const int num_parameters = evaluator->NumParameters();
  const int num_effective_parameters = evaluator->NumEffectiveParameters();

  summary->termination_type = NO_CONVERGENCE;
  summary->num_successful_steps = 0;
  summary->num_unsuccessful_steps = 0;

  VectorRef x(parameters, num_parameters);

  State current_state(num_parameters, num_effective_parameters);
  State previous_state(num_parameters, num_effective_parameters);

  Vector delta(num_effective_parameters);
  Vector x_plus_delta(num_parameters);

  IterationSummary iteration_summary;
  iteration_summary.iteration = 0;
  iteration_summary.step_is_valid = false;
  iteration_summary.step_is_successful = false;
  iteration_summary.cost_change = 0.0;
  iteration_summary.gradient_max_norm = 0.0;
  iteration_summary.step_norm = 0.0;
  iteration_summary.linear_solver_iterations = 0;
  iteration_summary.step_solver_time_in_seconds = 0;

  // Do initial cost and Jacobian evaluation.
  if (!Evaluate(evaluator, x, &current_state)) {
    LOG(WARNING) << "Terminating: Cost and gradient evaluation failed.";
    summary->termination_type = NUMERICAL_FAILURE;
    return;
  }

  summary->initial_cost = current_state.cost + summary->fixed_cost;
  iteration_summary.cost = current_state.cost + summary->fixed_cost;

  iteration_summary.gradient_max_norm = current_state.gradient_max_norm;

  // The initial gradient max_norm is bounded from below so that we do
  // not divide by zero.
  const double initial_gradient_max_norm =
      max(iteration_summary.gradient_max_norm, kEpsilon);
  const double absolute_gradient_tolerance =
      options.gradient_tolerance * initial_gradient_max_norm;

  if (iteration_summary.gradient_max_norm <= absolute_gradient_tolerance) {
    summary->termination_type = GRADIENT_TOLERANCE;
    VLOG(1) << "Terminating: Gradient tolerance reached."
            << "Relative gradient max norm: "
            << iteration_summary.gradient_max_norm / initial_gradient_max_norm
            << " <= " << options.gradient_tolerance;
    return;
  }

  iteration_summary.iteration_time_in_seconds =
      WallTimeInSeconds() - iteration_start_time;
  iteration_summary.cumulative_time_in_seconds =
      WallTimeInSeconds() - start_time
      + summary->preprocessor_time_in_seconds;
  summary->iterations.push_back(iteration_summary);

  LineSearchDirection::Options line_search_direction_options;
  line_search_direction_options.num_parameters = num_effective_parameters;
  line_search_direction_options.type = options.line_search_direction_type;
  line_search_direction_options.nonlinear_conjugate_gradient_type =
      options.nonlinear_conjugate_gradient_type;
  line_search_direction_options.max_lbfgs_rank = options.max_lbfgs_rank;
  line_search_direction_options.use_approximate_eigenvalue_bfgs_scaling =
      options.use_approximate_eigenvalue_bfgs_scaling;
  scoped_ptr<LineSearchDirection> line_search_direction(
      LineSearchDirection::Create(line_search_direction_options));

  LineSearchFunction line_search_function(evaluator);

  LineSearch::Options line_search_options;
  line_search_options.interpolation_type =
      options.line_search_interpolation_type;
  line_search_options.min_step_size = options.min_line_search_step_size;
  line_search_options.sufficient_decrease =
      options.line_search_sufficient_function_decrease;
  line_search_options.max_step_contraction =
      options.max_line_search_step_contraction;
  line_search_options.min_step_contraction =
      options.min_line_search_step_contraction;
  line_search_options.max_num_iterations =
      options.max_num_line_search_step_size_iterations;
  line_search_options.sufficient_curvature_decrease =
      options.line_search_sufficient_curvature_decrease;
  line_search_options.max_step_expansion =
      options.max_line_search_step_expansion;
  line_search_options.function = &line_search_function;

  scoped_ptr<LineSearch>
      line_search(LineSearch::Create(options.line_search_type,
                                     line_search_options,
                                     &summary->error));
  if (line_search.get() == NULL) {
    LOG(ERROR) << "Ceres bug: Unable to create a LineSearch object, please "
               << "contact the developers!, error: " << summary->error;
    summary->termination_type = DID_NOT_RUN;
    return;
  }

  LineSearch::Summary line_search_summary;
  int num_line_search_direction_restarts = 0;

  while (true) {
    if (!RunCallbacks(options.callbacks, iteration_summary, summary)) {
      return;
    }

    iteration_start_time = WallTimeInSeconds();
    if (iteration_summary.iteration >= options.max_num_iterations) {
      summary->termination_type = NO_CONVERGENCE;
      VLOG(1) << "Terminating: Maximum number of iterations reached.";
      break;
    }

    const double total_solver_time = iteration_start_time - start_time +
        summary->preprocessor_time_in_seconds;
    if (total_solver_time >= options.max_solver_time_in_seconds) {
      summary->termination_type = NO_CONVERGENCE;
      VLOG(1) << "Terminating: Maximum solver time reached.";
      break;
    }

    iteration_summary = IterationSummary();
    iteration_summary.iteration = summary->iterations.back().iteration + 1;
    iteration_summary.step_is_valid = false;
    iteration_summary.step_is_successful = false;

    bool line_search_status = true;
    if (iteration_summary.iteration == 1) {
      current_state.search_direction = -current_state.gradient;
    } else {
      line_search_status = line_search_direction->NextDirection(
          previous_state,
          current_state,
          &current_state.search_direction);
    }

    if (!line_search_status &&
        num_line_search_direction_restarts >=
        options.max_num_line_search_direction_restarts) {
      // Line search direction failed to generate a new direction, and we
      // have already reached our specified maximum number of restarts,
      // terminate optimization.
      summary->error =
          StringPrintf("Line search direction failure: specified "
                       "max_num_line_search_direction_restarts: %d reached.",
                       options.max_num_line_search_direction_restarts);
      LOG(WARNING) << summary->error << " terminating optimization.";
      summary->termination_type = NUMERICAL_FAILURE;
      break;

    } else if (!line_search_status) {
      // Restart line search direction with gradient descent on first iteration
      // as we have not yet reached our maximum number of restarts.
      CHECK_LT(num_line_search_direction_restarts,
               options.max_num_line_search_direction_restarts);

      ++num_line_search_direction_restarts;
      LOG(WARNING)
          << "Line search direction algorithm: "
          << LineSearchDirectionTypeToString(options.line_search_direction_type)
          << ", failed to produce a valid new direction at iteration: "
          << iteration_summary.iteration << ". Restarting, number of "
          << "restarts: " << num_line_search_direction_restarts << " / "
          << options.max_num_line_search_direction_restarts << " [max].";
      line_search_direction.reset(
          LineSearchDirection::Create(line_search_direction_options));
      current_state.search_direction = -current_state.gradient;
    }

    line_search_function.Init(x, current_state.search_direction);
    current_state.directional_derivative =
        current_state.gradient.dot(current_state.search_direction);

    // TODO(sameeragarwal): Refactor this into its own object and add
    // explanations for the various choices.
    //
    // Note that we use !line_search_status to ensure that we treat cases when
    // we restarted the line search direction equivalently to the first
    // iteration.
    const double initial_step_size =
        (iteration_summary.iteration == 1 || !line_search_status)
        ? min(1.0, 1.0 / current_state.gradient_max_norm)
        : min(1.0, 2.0 * (current_state.cost - previous_state.cost) /
              current_state.directional_derivative);
    // By definition, we should only ever go forwards along the specified search
    // direction in a line search, most likely cause for this being violated
    // would be a numerical failure in the line search direction calculation.
    if (initial_step_size < 0.0) {
      summary->error =
          StringPrintf("Numerical failure in line search, initial_step_size is "
                       "negative: %.5e, directional_derivative: %.5e, "
                       "(current_cost - previous_cost): %.5e",
                       initial_step_size, current_state.directional_derivative,
                       (current_state.cost - previous_state.cost));
      LOG(WARNING) << summary->error;
      summary->termination_type = NUMERICAL_FAILURE;
      break;
    }

    line_search->Search(initial_step_size,
                        current_state.cost,
                        current_state.directional_derivative,
                        &line_search_summary);

    current_state.step_size = line_search_summary.optimal_step_size;
    delta = current_state.step_size * current_state.search_direction;

    previous_state = current_state;
    iteration_summary.step_solver_time_in_seconds =
        WallTimeInSeconds() - iteration_start_time;

    // TODO(sameeragarwal): Collect stats.
    if (!evaluator->Plus(x.data(), delta.data(), x_plus_delta.data()) ||
        !Evaluate(evaluator, x_plus_delta, &current_state)) {
      LOG(WARNING) << "Evaluation failed.";
    } else {
      x = x_plus_delta;
    }

    iteration_summary.gradient_max_norm = current_state.gradient_max_norm;
    if (iteration_summary.gradient_max_norm <= absolute_gradient_tolerance) {
      summary->termination_type = GRADIENT_TOLERANCE;
      VLOG(1) << "Terminating: Gradient tolerance reached."
              << "Relative gradient max norm: "
              << iteration_summary.gradient_max_norm / initial_gradient_max_norm
              << " <= " << options.gradient_tolerance;
      break;
    }

    iteration_summary.cost_change = previous_state.cost - current_state.cost;
    const double absolute_function_tolerance =
        options.function_tolerance * previous_state.cost;
    if (fabs(iteration_summary.cost_change) < absolute_function_tolerance) {
      VLOG(1) << "Terminating. Function tolerance reached. "
              << "|cost_change|/cost: "
              << fabs(iteration_summary.cost_change) / previous_state.cost
              << " <= " << options.function_tolerance;
      summary->termination_type = FUNCTION_TOLERANCE;
      return;
    }

    iteration_summary.cost = current_state.cost + summary->fixed_cost;
    iteration_summary.step_norm = delta.norm();
    iteration_summary.step_is_valid = true;
    iteration_summary.step_is_successful = true;
    iteration_summary.step_norm = delta.norm();
    iteration_summary.step_size =  current_state.step_size;
    iteration_summary.line_search_function_evaluations =
        line_search_summary.num_function_evaluations;
    iteration_summary.line_search_gradient_evaluations =
        line_search_summary.num_gradient_evaluations;
    iteration_summary.line_search_iterations =
        line_search_summary.num_iterations;
    iteration_summary.iteration_time_in_seconds =
        WallTimeInSeconds() - iteration_start_time;
    iteration_summary.cumulative_time_in_seconds =
        WallTimeInSeconds() - start_time
        + summary->preprocessor_time_in_seconds;

    summary->iterations.push_back(iteration_summary);
    ++summary->num_successful_steps;
  }
}

}  // namespace internal
}  // namespace ceres

#endif  // CERES_NO_LINE_SEARCH_MINIMIZER
