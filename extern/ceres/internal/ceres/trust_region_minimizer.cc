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
#include "ceres/internal/eigen.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/line_search.h"
#include "ceres/linear_least_squares_problems.h"
#include "ceres/sparse_matrix.h"
#include "ceres/stringprintf.h"
#include "ceres/trust_region_strategy.h"
#include "ceres/types.h"
#include "ceres/wall_time.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {
namespace {

LineSearch::Summary DoLineSearch(const Minimizer::Options& options,
                                 const Vector& x,
                                 const Vector& gradient,
                                 const double cost,
                                 const Vector& delta,
                                 Evaluator* evaluator) {
  LineSearchFunction line_search_function(evaluator);

  LineSearch::Options line_search_options;
  line_search_options.is_silent = true;
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

  std::string message;
  scoped_ptr<LineSearch> line_search(
      CHECK_NOTNULL(LineSearch::Create(ceres::ARMIJO,
                                       line_search_options,
                                       &message)));
  LineSearch::Summary summary;
  line_search_function.Init(x, delta);
  line_search->Search(1.0, cost, gradient.dot(delta), &summary);
  return summary;
}

}  // namespace

// Compute a scaling vector that is used to improve the conditioning
// of the Jacobian.
void TrustRegionMinimizer::EstimateScale(const SparseMatrix& jacobian,
                                         double* scale) const {
  jacobian.SquaredColumnNorm(scale);
  for (int i = 0; i < jacobian.num_cols(); ++i) {
    scale[i] = 1.0 / (1.0 + sqrt(scale[i]));
  }
}

void TrustRegionMinimizer::Init(const Minimizer::Options& options) {
  options_ = options;
  sort(options_.trust_region_minimizer_iterations_to_dump.begin(),
       options_.trust_region_minimizer_iterations_to_dump.end());
}

void TrustRegionMinimizer::Minimize(const Minimizer::Options& options,
                                    double* parameters,
                                    Solver::Summary* summary) {
  double start_time = WallTimeInSeconds();
  double iteration_start_time =  start_time;
  Init(options);

  Evaluator* evaluator = CHECK_NOTNULL(options_.evaluator.get());
  SparseMatrix* jacobian = CHECK_NOTNULL(options_.jacobian.get());
  TrustRegionStrategy* strategy =
      CHECK_NOTNULL(options_.trust_region_strategy.get());

  const bool is_not_silent = !options.is_silent;

  // If the problem is bounds constrained, then enable the use of a
  // line search after the trust region step has been computed. This
  // line search will automatically use a projected test point onto
  // the feasible set, there by guaranteeing the feasibility of the
  // final output.
  //
  // TODO(sameeragarwal): Make line search available more generally.
  const bool use_line_search = options.is_constrained;

  summary->termination_type = NO_CONVERGENCE;
  summary->num_successful_steps = 0;
  summary->num_unsuccessful_steps = 0;
  summary->is_constrained = options.is_constrained;

  const int num_parameters = evaluator->NumParameters();
  const int num_effective_parameters = evaluator->NumEffectiveParameters();
  const int num_residuals = evaluator->NumResiduals();

  Vector residuals(num_residuals);
  Vector trust_region_step(num_effective_parameters);
  Vector delta(num_effective_parameters);
  Vector x_plus_delta(num_parameters);
  Vector gradient(num_effective_parameters);
  Vector model_residuals(num_residuals);
  Vector scale(num_effective_parameters);
  Vector negative_gradient(num_effective_parameters);
  Vector projected_gradient_step(num_parameters);

  IterationSummary iteration_summary;
  iteration_summary.iteration = 0;
  iteration_summary.step_is_valid = false;
  iteration_summary.step_is_successful = false;
  iteration_summary.cost_change = 0.0;
  iteration_summary.gradient_max_norm = 0.0;
  iteration_summary.gradient_norm = 0.0;
  iteration_summary.step_norm = 0.0;
  iteration_summary.relative_decrease = 0.0;
  iteration_summary.trust_region_radius = strategy->Radius();
  iteration_summary.eta = options_.eta;
  iteration_summary.linear_solver_iterations = 0;
  iteration_summary.step_solver_time_in_seconds = 0;

  VectorRef x_min(parameters, num_parameters);
  Vector x = x_min;
  // Project onto the feasible set.
  if (options.is_constrained) {
    delta.setZero();
    if (!evaluator->Plus(x.data(), delta.data(), x_plus_delta.data())) {
      summary->message =
          "Unable to project initial point onto the feasible set.";
      summary->termination_type = FAILURE;
      LOG_IF(WARNING, is_not_silent) << "Terminating: " << summary->message;
      return;
    }
    x_min = x_plus_delta;
    x = x_plus_delta;
  }

  double x_norm = x.norm();

  // Do initial cost and Jacobian evaluation.
  double cost = 0.0;
  if (!evaluator->Evaluate(x.data(),
                           &cost,
                           residuals.data(),
                           gradient.data(),
                           jacobian)) {
    summary->message = "Residual and Jacobian evaluation failed.";
    summary->termination_type = FAILURE;
    LOG_IF(WARNING, is_not_silent) << "Terminating: " << summary->message;
    return;
  }

  negative_gradient = -gradient;
  if (!evaluator->Plus(x.data(),
                       negative_gradient.data(),
                       projected_gradient_step.data())) {
    summary->message = "Unable to compute gradient step.";
    summary->termination_type = FAILURE;
    LOG(ERROR) << "Terminating: " << summary->message;
    return;
  }

  summary->initial_cost = cost + summary->fixed_cost;
  iteration_summary.cost = cost + summary->fixed_cost;
  iteration_summary.gradient_max_norm =
    (x - projected_gradient_step).lpNorm<Eigen::Infinity>();
  iteration_summary.gradient_norm = (x - projected_gradient_step).norm();

  if (iteration_summary.gradient_max_norm <= options.gradient_tolerance) {
    summary->message = StringPrintf("Gradient tolerance reached. "
                                    "Gradient max norm: %e <= %e",
                                    iteration_summary.gradient_max_norm,
                                    options_.gradient_tolerance);
    summary->termination_type = CONVERGENCE;
    VLOG_IF(1, is_not_silent) << "Terminating: " << summary->message;

    // Ensure that there is an iteration summary object for iteration
    // 0 in Summary::iterations.
    iteration_summary.iteration_time_in_seconds =
        WallTimeInSeconds() - iteration_start_time;
    iteration_summary.cumulative_time_in_seconds =
        WallTimeInSeconds() - start_time +
        summary->preprocessor_time_in_seconds;
    summary->iterations.push_back(iteration_summary);
    return;
  }

  if (options_.jacobi_scaling) {
    EstimateScale(*jacobian, scale.data());
    jacobian->ScaleColumns(scale.data());
  } else {
    scale.setOnes();
  }

  iteration_summary.iteration_time_in_seconds =
      WallTimeInSeconds() - iteration_start_time;
  iteration_summary.cumulative_time_in_seconds =
      WallTimeInSeconds() - start_time
      + summary->preprocessor_time_in_seconds;
  summary->iterations.push_back(iteration_summary);

  int num_consecutive_nonmonotonic_steps = 0;
  double minimum_cost = cost;
  double reference_cost = cost;
  double accumulated_reference_model_cost_change = 0.0;
  double candidate_cost = cost;
  double accumulated_candidate_model_cost_change = 0.0;
  int num_consecutive_invalid_steps = 0;
  bool inner_iterations_are_enabled =
      options.inner_iteration_minimizer.get() != NULL;
  while (true) {
    bool inner_iterations_were_useful = false;
    if (!RunCallbacks(options, iteration_summary, summary)) {
      return;
    }

    iteration_start_time = WallTimeInSeconds();
    if (iteration_summary.iteration >= options_.max_num_iterations) {
      summary->message = "Maximum number of iterations reached.";
      summary->termination_type = NO_CONVERGENCE;
      VLOG_IF(1, is_not_silent) << "Terminating: " << summary->message;
      return;
    }

    const double total_solver_time = iteration_start_time - start_time +
        summary->preprocessor_time_in_seconds;
    if (total_solver_time >= options_.max_solver_time_in_seconds) {
      summary->message = "Maximum solver time reached.";
      summary->termination_type = NO_CONVERGENCE;
      VLOG_IF(1, is_not_silent) << "Terminating: " << summary->message;
      return;
    }

    const double strategy_start_time = WallTimeInSeconds();
    TrustRegionStrategy::PerSolveOptions per_solve_options;
    per_solve_options.eta = options_.eta;
    if (find(options_.trust_region_minimizer_iterations_to_dump.begin(),
             options_.trust_region_minimizer_iterations_to_dump.end(),
             iteration_summary.iteration) !=
        options_.trust_region_minimizer_iterations_to_dump.end()) {
      per_solve_options.dump_format_type =
          options_.trust_region_problem_dump_format_type;
      per_solve_options.dump_filename_base =
          JoinPath(options_.trust_region_problem_dump_directory,
                   StringPrintf("ceres_solver_iteration_%03d",
                                iteration_summary.iteration));
    } else {
      per_solve_options.dump_format_type = TEXTFILE;
      per_solve_options.dump_filename_base.clear();
    }

    TrustRegionStrategy::Summary strategy_summary =
        strategy->ComputeStep(per_solve_options,
                              jacobian,
                              residuals.data(),
                              trust_region_step.data());

    if (strategy_summary.termination_type == LINEAR_SOLVER_FATAL_ERROR) {
      summary->message =
          "Linear solver failed due to unrecoverable "
          "non-numeric causes. Please see the error log for clues. ";
      summary->termination_type = FAILURE;
      LOG_IF(WARNING, is_not_silent) << "Terminating: " << summary->message;
      return;
    }

    iteration_summary = IterationSummary();
    iteration_summary.iteration = summary->iterations.back().iteration + 1;
    iteration_summary.step_solver_time_in_seconds =
        WallTimeInSeconds() - strategy_start_time;
    iteration_summary.linear_solver_iterations =
        strategy_summary.num_iterations;
    iteration_summary.step_is_valid = false;
    iteration_summary.step_is_successful = false;

    double model_cost_change = 0.0;
    if (strategy_summary.termination_type != LINEAR_SOLVER_FAILURE) {
      // new_model_cost
      //  = 1/2 [f + J * step]^2
      //  = 1/2 [ f'f + 2f'J * step + step' * J' * J * step ]
      // model_cost_change
      //  = cost - new_model_cost
      //  = f'f/2  - 1/2 [ f'f + 2f'J * step + step' * J' * J * step]
      //  = -f'J * step - step' * J' * J * step / 2
      model_residuals.setZero();
      jacobian->RightMultiply(trust_region_step.data(), model_residuals.data());
      model_cost_change =
          - model_residuals.dot(residuals + model_residuals / 2.0);

      if (model_cost_change < 0.0) {
        VLOG_IF(1, is_not_silent)
            << "Invalid step: current_cost: " << cost
            << " absolute difference " << model_cost_change
            << " relative difference " << (model_cost_change / cost);
      } else {
        iteration_summary.step_is_valid = true;
      }
    }

    if (!iteration_summary.step_is_valid) {
      // Invalid steps can happen due to a number of reasons, and we
      // allow a limited number of successive failures, and return with
      // FAILURE if this limit is exceeded.
      if (++num_consecutive_invalid_steps >=
          options_.max_num_consecutive_invalid_steps) {
        summary->message = StringPrintf(
            "Number of successive invalid steps more "
            "than Solver::Options::max_num_consecutive_invalid_steps: %d",
            options_.max_num_consecutive_invalid_steps);
        summary->termination_type = FAILURE;
        LOG_IF(WARNING, is_not_silent) << "Terminating: " << summary->message;
        return;
      }

      // We are going to try and reduce the trust region radius and
      // solve again. To do this, we are going to treat this iteration
      // as an unsuccessful iteration. Since the various callbacks are
      // still executed, we are going to fill the iteration summary
      // with data that assumes a step of length zero and no progress.
      iteration_summary.cost = cost + summary->fixed_cost;
      iteration_summary.cost_change = 0.0;
      iteration_summary.gradient_max_norm =
          summary->iterations.back().gradient_max_norm;
      iteration_summary.gradient_norm =
          summary->iterations.back().gradient_norm;
      iteration_summary.step_norm = 0.0;
      iteration_summary.relative_decrease = 0.0;
      iteration_summary.eta = options_.eta;
    } else {
      // The step is numerically valid, so now we can judge its quality.
      num_consecutive_invalid_steps = 0;

      // Undo the Jacobian column scaling.
      delta = (trust_region_step.array() * scale.array()).matrix();

      // Try improving the step further by using an ARMIJO line
      // search.
      //
      // TODO(sameeragarwal): What happens to trust region sizing as
      // it interacts with the line search ?
      if (use_line_search) {
        const LineSearch::Summary line_search_summary =
            DoLineSearch(options, x, gradient, cost, delta, evaluator);

        summary->line_search_cost_evaluation_time_in_seconds +=
            line_search_summary.cost_evaluation_time_in_seconds;
        summary->line_search_gradient_evaluation_time_in_seconds +=
            line_search_summary.gradient_evaluation_time_in_seconds;
        summary->line_search_polynomial_minimization_time_in_seconds +=
            line_search_summary.polynomial_minimization_time_in_seconds;
        summary->line_search_total_time_in_seconds +=
            line_search_summary.total_time_in_seconds;

        if (line_search_summary.success) {
          delta *= line_search_summary.optimal_step_size;
        }
      }

      double new_cost = std::numeric_limits<double>::max();
      if (evaluator->Plus(x.data(), delta.data(), x_plus_delta.data())) {
        if (!evaluator->Evaluate(x_plus_delta.data(),
                                 &new_cost,
                                 NULL,
                                 NULL,
                                 NULL)) {
          LOG_IF(WARNING, is_not_silent)
              << "Step failed to evaluate. "
              << "Treating it as a step with infinite cost";
          new_cost = std::numeric_limits<double>::max();
        }
      } else {
        LOG_IF(WARNING, is_not_silent)
            << "x_plus_delta = Plus(x, delta) failed. "
            << "Treating it as a step with infinite cost";
      }

      if (new_cost < std::numeric_limits<double>::max()) {
        // Check if performing an inner iteration will make it better.
        if (inner_iterations_are_enabled) {
          ++summary->num_inner_iteration_steps;
          double inner_iteration_start_time = WallTimeInSeconds();
          const double x_plus_delta_cost = new_cost;
          Vector inner_iteration_x = x_plus_delta;
          Solver::Summary inner_iteration_summary;
          options.inner_iteration_minimizer->Minimize(options,
                                                      inner_iteration_x.data(),
                                                      &inner_iteration_summary);
          if (!evaluator->Evaluate(inner_iteration_x.data(),
                                   &new_cost,
                                   NULL, NULL, NULL)) {
            VLOG_IF(2, is_not_silent) << "Inner iteration failed.";
            new_cost = x_plus_delta_cost;
          } else {
            x_plus_delta = inner_iteration_x;
            // Boost the model_cost_change, since the inner iteration
            // improvements are not accounted for by the trust region.
            model_cost_change +=  x_plus_delta_cost - new_cost;
            VLOG_IF(2, is_not_silent)
                << "Inner iteration succeeded; Current cost: " << cost
                << " Trust region step cost: " << x_plus_delta_cost
                << " Inner iteration cost: " << new_cost;

            inner_iterations_were_useful = new_cost < cost;

            const double inner_iteration_relative_progress =
                1.0 - new_cost / x_plus_delta_cost;
            // Disable inner iterations once the relative improvement
            // drops below tolerance.
            inner_iterations_are_enabled =
                (inner_iteration_relative_progress >
                 options.inner_iteration_tolerance);
            VLOG_IF(2, is_not_silent && !inner_iterations_are_enabled)
                << "Disabling inner iterations. Progress : "
                << inner_iteration_relative_progress;
          }
          summary->inner_iteration_time_in_seconds +=
              WallTimeInSeconds() - inner_iteration_start_time;
        }
      }

      iteration_summary.step_norm = (x - x_plus_delta).norm();

      // Convergence based on parameter_tolerance.
      const double step_size_tolerance =  options_.parameter_tolerance *
          (x_norm + options_.parameter_tolerance);
      if (iteration_summary.step_norm <= step_size_tolerance) {
        summary->message =
            StringPrintf("Parameter tolerance reached. "
                         "Relative step_norm: %e <= %e.",
                         (iteration_summary.step_norm /
                          (x_norm + options_.parameter_tolerance)),
                         options_.parameter_tolerance);
        summary->termination_type = CONVERGENCE;
        VLOG_IF(1, is_not_silent) << "Terminating: " << summary->message;
        return;
      }

      iteration_summary.cost_change =  cost - new_cost;
      const double absolute_function_tolerance =
          options_.function_tolerance * cost;
      if (fabs(iteration_summary.cost_change) <= absolute_function_tolerance) {
        summary->message =
            StringPrintf("Function tolerance reached. "
                         "|cost_change|/cost: %e <= %e",
                         fabs(iteration_summary.cost_change) / cost,
                         options_.function_tolerance);
        summary->termination_type = CONVERGENCE;
        VLOG_IF(1, is_not_silent) << "Terminating: " << summary->message;
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
          ? std::max(relative_decrease, historical_relative_decrease)
          : relative_decrease;

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
      // In most cases this is fine, but it can be the case that the
      // change in solution quality due to inner iterations is so large
      // and the trust region step is so bad, that this ratio can become
      // quite small.
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
      iteration_summary.step_is_successful =
          (inner_iterations_were_useful ||
           iteration_summary.relative_decrease >
           options_.min_relative_decrease);

      if (iteration_summary.step_is_successful) {
        accumulated_candidate_model_cost_change += model_cost_change;
        accumulated_reference_model_cost_change += model_cost_change;

        if (!inner_iterations_were_useful &&
            relative_decrease <= options_.min_relative_decrease) {
          iteration_summary.step_is_nonmonotonic = true;
          VLOG_IF(2, is_not_silent)
              << "Non-monotonic step! "
              << " relative_decrease: "
              << relative_decrease
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
                               gradient.data(),
                               jacobian)) {
        summary->message = "Residual and Jacobian evaluation failed.";
        summary->termination_type = FAILURE;
        LOG_IF(WARNING, is_not_silent) << "Terminating: " << summary->message;
        return;
      }

      negative_gradient = -gradient;
      if (!evaluator->Plus(x.data(),
                           negative_gradient.data(),
                           projected_gradient_step.data())) {
        summary->message =
            "projected_gradient_step = Plus(x, -gradient) failed.";
        summary->termination_type = FAILURE;
        LOG(ERROR) << "Terminating: " << summary->message;
        return;
      }

      iteration_summary.gradient_max_norm =
          (x - projected_gradient_step).lpNorm<Eigen::Infinity>();
      iteration_summary.gradient_norm = (x - projected_gradient_step).norm();

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
          VLOG_IF(2, is_not_silent)
              << "Updating the candidate iterate to the current point.";
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
          VLOG_IF(2, is_not_silent)
              << "Resetting the reference point to the candidate point";
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
    iteration_summary.iteration_time_in_seconds =
        WallTimeInSeconds() - iteration_start_time;
    iteration_summary.cumulative_time_in_seconds =
        WallTimeInSeconds() - start_time
        + summary->preprocessor_time_in_seconds;
    summary->iterations.push_back(iteration_summary);

    // If the step was successful, check for the gradient norm
    // collapsing to zero, and if the step is unsuccessful then check
    // if the trust region radius has collapsed to zero.
    //
    // For correctness (Number of IterationSummary objects, correct
    // final cost, and state update) these convergence tests need to
    // be performed at the end of the iteration.
    if (iteration_summary.step_is_successful) {
      // Gradient norm can only go down in successful steps.
      if (iteration_summary.gradient_max_norm <= options.gradient_tolerance) {
        summary->message = StringPrintf("Gradient tolerance reached. "
                                        "Gradient max norm: %e <= %e",
                                        iteration_summary.gradient_max_norm,
                                        options_.gradient_tolerance);
        summary->termination_type = CONVERGENCE;
        VLOG_IF(1, is_not_silent) << "Terminating: " << summary->message;
        return;
      }
    } else {
      // Trust region radius can only go down if the step if
      // unsuccessful.
      if (iteration_summary.trust_region_radius <
          options_.min_trust_region_radius) {
        summary->message = "Termination. Minimum trust region radius reached.";
        summary->termination_type = CONVERGENCE;
        VLOG_IF(1, is_not_silent) << summary->message;
        return;
      }
    }
  }
}


}  // namespace internal
}  // namespace ceres
