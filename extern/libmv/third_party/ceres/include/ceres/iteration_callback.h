// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2010, 2011, 2012 Google Inc. All rights reserved.
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
// When an iteration callback is specified, Ceres calls the callback after each
// optimizer step and pass it an IterationSummary object, defined below.

#ifndef CERES_PUBLIC_ITERATION_CALLBACK_H_
#define CERES_PUBLIC_ITERATION_CALLBACK_H_

#include "ceres/types.h"

namespace ceres {

// This struct describes the state of the optimizer after each
// iteration of the minimization.
struct IterationSummary {
  // Current iteration number.
  int32 iteration;

  // Whether or not the algorithm made progress in this iteration.
  bool step_is_successful;

  // Value of the objective function.
  double cost;

  // Change in the value of the objective function in this
  // iteration. This can be positive or negative. Negative change
  // means that the step was not successful.
  double cost_change;

  // Infinity norm of the gradient vector.
  double gradient_max_norm;

  // 2-norm of the size of the step computed by the optimization
  // algorithm.
  double step_norm;

  // For trust region algorithms, the ratio of the actual change in
  // cost and the change in the cost of the linearized approximation.
  double relative_decrease;

  // Value of the regularization parameter for Levenberg-Marquardt
  // algorithm at the end of the current iteration.
  double mu;

  // For the inexact step Levenberg-Marquardt algorithm, this is the
  // relative accuracy with which the Newton(LM) step is solved. This
  // number affects only the iterative solvers capable of solving
  // linear systems inexactly. Factorization-based exact solvers
  // ignore it.
  double eta;

  // Number of iterations taken by the linear solver to solve for the
  // Newton step.
  int linear_solver_iterations;

  // TODO(sameeragarwal): Change to use a higher precision timer using
  // clock_gettime.
  // Time (in seconds) spent inside the linear least squares solver.
  int iteration_time_sec;

  // Time (in seconds) spent inside the linear least squares solver.
  int linear_solver_time_sec;
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
//                                    summary.mu,
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
class IterationCallback {
 public:
  virtual ~IterationCallback() {}
  virtual CallbackReturnType operator()(const IterationSummary& summary) = 0;
};

}  // namespace ceres

#endif  // CERES_PUBLIC_ITERATION_CALLBACK_H_
