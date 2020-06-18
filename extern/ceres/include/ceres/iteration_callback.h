// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2019 Google Inc. All rights reserved.
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
// When an iteration callback is specified, Ceres calls the callback
// after each minimizer step (if the minimizer has not converged) and
// passes it an IterationSummary object, defined below.

#ifndef CERES_PUBLIC_ITERATION_CALLBACK_H_
#define CERES_PUBLIC_ITERATION_CALLBACK_H_

#include "ceres/internal/disable_warnings.h"
#include "ceres/types.h"

namespace ceres {

// This struct describes the state of the optimizer after each
// iteration of the minimization.
struct CERES_EXPORT IterationSummary {
  // Current iteration number.
  int iteration = 0;

  // Step was numerically valid, i.e., all values are finite and the
  // step reduces the value of the linearized model.
  //
  // Note: step_is_valid is always true when iteration = 0.
  bool step_is_valid = false;

  // Step did not reduce the value of the objective function
  // sufficiently, but it was accepted because of the relaxed
  // acceptance criterion used by the non-monotonic trust region
  // algorithm.
  //
  // Note: step_is_nonmonotonic is always false when iteration = 0;
  bool step_is_nonmonotonic = false;

  // Whether or not the minimizer accepted this step or not. If the
  // ordinary trust region algorithm is used, this means that the
  // relative reduction in the objective function value was greater
  // than Solver::Options::min_relative_decrease. However, if the
  // non-monotonic trust region algorithm is used
  // (Solver::Options:use_nonmonotonic_steps = true), then even if the
  // relative decrease is not sufficient, the algorithm may accept the
  // step and the step is declared successful.
  //
  // Note: step_is_successful is always true when iteration = 0.
  bool step_is_successful = false;

  // Value of the objective function.
  double cost = 0.90;

  // Change in the value of the objective function in this
  // iteration. This can be positive or negative.
  double cost_change = 0.0;

  // Infinity norm of the gradient vector.
  double gradient_max_norm = 0.0;

  // 2-norm of the gradient vector.
  double gradient_norm = 0.0;

  // 2-norm of the size of the step computed by the optimization
  // algorithm.
  double step_norm = 0.0;

  // For trust region algorithms, the ratio of the actual change in
  // cost and the change in the cost of the linearized approximation.
  double relative_decrease = 0.0;

  // Size of the trust region at the end of the current iteration. For
  // the Levenberg-Marquardt algorithm, the regularization parameter
  // mu = 1.0 / trust_region_radius.
  double trust_region_radius = 0.0;

  // For the inexact step Levenberg-Marquardt algorithm, this is the
  // relative accuracy with which the Newton(LM) step is solved. This
  // number affects only the iterative solvers capable of solving
  // linear systems inexactly. Factorization-based exact solvers
  // ignore it.
  double eta = 0.0;

  // Step sized computed by the line search algorithm.
  double step_size = 0.0;

  // Number of function value evaluations used by the line search algorithm.
  int line_search_function_evaluations = 0;

  // Number of function gradient evaluations used by the line search algorithm.
  int line_search_gradient_evaluations = 0;

  // Number of iterations taken by the line search algorithm.
  int line_search_iterations = 0;

  // Number of iterations taken by the linear solver to solve for the
  // Newton step.
  int linear_solver_iterations = 0;

  // All times reported below are wall times.

  // Time (in seconds) spent inside the minimizer loop in the current
  // iteration.
  double iteration_time_in_seconds = 0.0;

  // Time (in seconds) spent inside the trust region step solver.
  double step_solver_time_in_seconds = 0.0;

  // Time (in seconds) since the user called Solve().
  double cumulative_time_in_seconds = 0.0;
};

// Interface for specifying callbacks that are executed at the end of
// each iteration of the Minimizer. The solver uses the return value
// of operator() to decide whether to continue solving or to
// terminate. The user can return three values.
//
// SOLVER_ABORT indicates that the callback detected an abnormal
// situation. The solver returns without updating the parameter blocks
// (unless Solver::Options::update_state_every_iteration is set
// true). Solver returns with Solver::Summary::termination_type set to
// USER_ABORT.
//
// SOLVER_TERMINATE_SUCCESSFULLY indicates that there is no need to
// optimize anymore (some user specified termination criterion has
// been met). Solver returns with Solver::Summary::termination_type
// set to USER_SUCCESS.
//
// SOLVER_CONTINUE indicates that the solver should continue
// optimizing.
//
// For example, the following Callback is used internally by Ceres to
// log the progress of the optimization.
//
// Callback for logging the state of the minimizer to STDERR or STDOUT
// depending on the user's preferences and logging level.
//
//   class LoggingCallback : public IterationCallback {
//    public:
//     explicit LoggingCallback(bool log_to_stdout)
//         : log_to_stdout_(log_to_stdout) {}
//
//     ~LoggingCallback() {}
//
//     CallbackReturnType operator()(const IterationSummary& summary) {
//       const char* kReportRowFormat =
//           "% 4d: f:% 8e d:% 3.2e g:% 3.2e h:% 3.2e "
//           "rho:% 3.2e mu:% 3.2e eta:% 3.2e li:% 3d";
//       string output = StringPrintf(kReportRowFormat,
//                                    summary.iteration,
//                                    summary.cost,
//                                    summary.cost_change,
//                                    summary.gradient_max_norm,
//                                    summary.step_norm,
//                                    summary.relative_decrease,
//                                    summary.trust_region_radius,
//                                    summary.eta,
//                                    summary.linear_solver_iterations);
//       if (log_to_stdout_) {
//         cout << output << endl;
//       } else {
//         VLOG(1) << output;
//       }
//       return SOLVER_CONTINUE;
//     }
//
//    private:
//     const bool log_to_stdout_;
//   };
//
class CERES_EXPORT IterationCallback {
 public:
  virtual ~IterationCallback() {}
  virtual CallbackReturnType operator()(const IterationSummary& summary) = 0;
};

}  // namespace ceres

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_PUBLIC_ITERATION_CALLBACK_H_
