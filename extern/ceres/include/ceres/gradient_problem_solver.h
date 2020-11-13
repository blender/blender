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

#ifndef CERES_PUBLIC_GRADIENT_PROBLEM_SOLVER_H_
#define CERES_PUBLIC_GRADIENT_PROBLEM_SOLVER_H_

#include <cmath>
#include <string>
#include <vector>

#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/port.h"
#include "ceres/iteration_callback.h"
#include "ceres/types.h"

namespace ceres {

class GradientProblem;

class CERES_EXPORT GradientProblemSolver {
 public:
  virtual ~GradientProblemSolver();

  // The options structure contains, not surprisingly, options that control how
  // the solver operates. The defaults should be suitable for a wide range of
  // problems; however, better performance is often obtainable with tweaking.
  //
  // The constants are defined inside types.h
  struct CERES_EXPORT Options {
    // Returns true if the options struct has a valid
    // configuration. Returns false otherwise, and fills in *error
    // with a message describing the problem.
    bool IsValid(std::string* error) const;

    // Minimizer options ----------------------------------------
    LineSearchDirectionType line_search_direction_type = LBFGS;
    LineSearchType line_search_type = WOLFE;
    NonlinearConjugateGradientType nonlinear_conjugate_gradient_type =
        FLETCHER_REEVES;

    // The LBFGS hessian approximation is a low rank approximation to
    // the inverse of the Hessian matrix. The rank of the
    // approximation determines (linearly) the space and time
    // complexity of using the approximation. Higher the rank, the
    // better is the quality of the approximation. The increase in
    // quality is however is bounded for a number of reasons.
    //
    // 1. The method only uses secant information and not actual
    // derivatives.
    //
    // 2. The Hessian approximation is constrained to be positive
    // definite.
    //
    // So increasing this rank to a large number will cost time and
    // space complexity without the corresponding increase in solution
    // quality. There are no hard and fast rules for choosing the
    // maximum rank. The best choice usually requires some problem
    // specific experimentation.
    //
    // For more theoretical and implementation details of the LBFGS
    // method, please see:
    //
    // Nocedal, J. (1980). "Updating Quasi-Newton Matrices with
    // Limited Storage". Mathematics of Computation 35 (151): 773-782.
    int max_lbfgs_rank = 20;

    // As part of the (L)BFGS update step (BFGS) / right-multiply step (L-BFGS),
    // the initial inverse Hessian approximation is taken to be the Identity.
    // However, Oren showed that using instead I * \gamma, where \gamma is
    // chosen to approximate an eigenvalue of the true inverse Hessian can
    // result in improved convergence in a wide variety of cases. Setting
    // use_approximate_eigenvalue_bfgs_scaling to true enables this scaling.
    //
    // It is important to note that approximate eigenvalue scaling does not
    // always improve convergence, and that it can in fact significantly degrade
    // performance for certain classes of problem, which is why it is disabled
    // by default.  In particular it can degrade performance when the
    // sensitivity of the problem to different parameters varies significantly,
    // as in this case a single scalar factor fails to capture this variation
    // and detrimentally downscales parts of the jacobian approximation which
    // correspond to low-sensitivity parameters. It can also reduce the
    // robustness of the solution to errors in the jacobians.
    //
    // Oren S.S., Self-scaling variable metric (SSVM) algorithms
    // Part II: Implementation and experiments, Management Science,
    // 20(5), 863-874, 1974.
    bool use_approximate_eigenvalue_bfgs_scaling = false;

    // Degree of the polynomial used to approximate the objective
    // function. Valid values are BISECTION, QUADRATIC and CUBIC.
    //
    // BISECTION corresponds to pure backtracking search with no
    // interpolation.
    LineSearchInterpolationType line_search_interpolation_type = CUBIC;

    // If during the line search, the step_size falls below this
    // value, it is truncated to zero.
    double min_line_search_step_size = 1e-9;

    // Line search parameters.

    // Solving the line search problem exactly is computationally
    // prohibitive. Fortunately, line search based optimization
    // algorithms can still guarantee convergence if instead of an
    // exact solution, the line search algorithm returns a solution
    // which decreases the value of the objective function
    // sufficiently. More precisely, we are looking for a step_size
    // s.t.
    //
    //   f(step_size) <= f(0) + sufficient_decrease * f'(0) * step_size
    //
    double line_search_sufficient_function_decrease = 1e-4;

    // In each iteration of the line search,
    //
    //  new_step_size >= max_line_search_step_contraction * step_size
    //
    // Note that by definition, for contraction:
    //
    //  0 < max_step_contraction < min_step_contraction < 1
    //
    double max_line_search_step_contraction = 1e-3;

    // In each iteration of the line search,
    //
    //  new_step_size <= min_line_search_step_contraction * step_size
    //
    // Note that by definition, for contraction:
    //
    //  0 < max_step_contraction < min_step_contraction < 1
    //
    double min_line_search_step_contraction = 0.6;

    // Maximum number of trial step size iterations during each line search,
    // if a step size satisfying the search conditions cannot be found within
    // this number of trials, the line search will terminate.
    int max_num_line_search_step_size_iterations = 20;

    // Maximum number of restarts of the line search direction algorithm before
    // terminating the optimization. Restarts of the line search direction
    // algorithm occur when the current algorithm fails to produce a new descent
    // direction. This typically indicates a numerical failure, or a breakdown
    // in the validity of the approximations used.
    int max_num_line_search_direction_restarts = 5;

    // The strong Wolfe conditions consist of the Armijo sufficient
    // decrease condition, and an additional requirement that the
    // step-size be chosen s.t. the _magnitude_ ('strong' Wolfe
    // conditions) of the gradient along the search direction
    // decreases sufficiently. Precisely, this second condition
    // is that we seek a step_size s.t.
    //
    //   |f'(step_size)| <= sufficient_curvature_decrease * |f'(0)|
    //
    // Where f() is the line search objective and f'() is the derivative
    // of f w.r.t step_size (d f / d step_size).
    double line_search_sufficient_curvature_decrease = 0.9;

    // During the bracketing phase of the Wolfe search, the step size is
    // increased until either a point satisfying the Wolfe conditions is
    // found, or an upper bound for a bracket containing a point satisfying
    // the conditions is found.  Precisely, at each iteration of the
    // expansion:
    //
    //   new_step_size <= max_step_expansion * step_size.
    //
    // By definition for expansion, max_step_expansion > 1.0.
    double max_line_search_step_expansion = 10.0;

    // Maximum number of iterations for the minimizer to run for.
    int max_num_iterations = 50;

    // Maximum time for which the minimizer should run for.
    double max_solver_time_in_seconds = 1e9;

    // Minimizer terminates when
    //
    //   (new_cost - old_cost) < function_tolerance * old_cost;
    //
    double function_tolerance = 1e-6;

    // Minimizer terminates when
    //
    //   max_i |x - Project(Plus(x, -g(x))| < gradient_tolerance
    //
    // This value should typically be 1e-4 * function_tolerance.
    double gradient_tolerance = 1e-10;

    // Minimizer terminates when
    //
    //   |step|_2 <= parameter_tolerance * ( |x|_2 +  parameter_tolerance)
    //
    double parameter_tolerance = 1e-8;

    // Logging options ---------------------------------------------------------

    LoggingType logging_type = PER_MINIMIZER_ITERATION;

    // By default the Minimizer progress is logged to VLOG(1), which
    // is sent to STDERR depending on the vlog level. If this flag is
    // set to true, and logging_type is not SILENT, the logging output
    // is sent to STDOUT.
    bool minimizer_progress_to_stdout = false;

    // If true, the user's parameter blocks are updated at the end of
    // every Minimizer iteration, otherwise they are updated when the
    // Minimizer terminates. This is useful if, for example, the user
    // wishes to visualize the state of the optimization every
    // iteration.
    bool update_state_every_iteration = false;

    // Callbacks that are executed at the end of each iteration of the
    // Minimizer. An iteration may terminate midway, either due to
    // numerical failures or because one of the convergence tests has
    // been satisfied. In this case none of the callbacks are
    // executed.

    // Callbacks are executed in the order that they are specified in
    // this vector. By default, parameter blocks are updated only at
    // the end of the optimization, i.e when the Minimizer
    // terminates. This behaviour is controlled by
    // update_state_every_variable. If the user wishes to have access
    // to the update parameter blocks when his/her callbacks are
    // executed, then set update_state_every_iteration to true.
    //
    // The solver does NOT take ownership of these pointers.
    std::vector<IterationCallback*> callbacks;
  };

  struct CERES_EXPORT Summary {
    // A brief one line description of the state of the solver after
    // termination.
    std::string BriefReport() const;

    // A full multiline description of the state of the solver after
    // termination.
    std::string FullReport() const;

    bool IsSolutionUsable() const;

    // Minimizer summary -------------------------------------------------
    TerminationType termination_type = FAILURE;

    // Reason why the solver terminated.
    std::string message = "ceres::GradientProblemSolve was not called.";

    // Cost of the problem (value of the objective function) before
    // the optimization.
    double initial_cost = -1.0;

    // Cost of the problem (value of the objective function) after the
    // optimization.
    double final_cost = -1.0;

    // IterationSummary for each minimizer iteration in order.
    std::vector<IterationSummary> iterations;

    // Number of times the cost (and not the gradient) was evaluated.
    int num_cost_evaluations = -1;

    // Number of times the gradient (and the cost) were evaluated.
    int num_gradient_evaluations = -1;

    // Sum total of all time spent inside Ceres when Solve is called.
    double total_time_in_seconds = -1.0;

    // Time (in seconds) spent evaluating the cost.
    double cost_evaluation_time_in_seconds = -1.0;

    // Time (in seconds) spent evaluating the gradient.
    double gradient_evaluation_time_in_seconds = -1.0;

    // Time (in seconds) spent minimizing the interpolating polynomial
    // to compute the next candidate step size as part of a line search.
    double line_search_polynomial_minimization_time_in_seconds = -1.0;

    // Number of parameters in the problem.
    int num_parameters = -1;

    // Dimension of the tangent space of the problem.
    int num_local_parameters = -1;

    // Type of line search direction used.
    LineSearchDirectionType line_search_direction_type = LBFGS;

    // Type of the line search algorithm used.
    LineSearchType line_search_type = WOLFE;

    //  When performing line search, the degree of the polynomial used
    //  to approximate the objective function.
    LineSearchInterpolationType line_search_interpolation_type = CUBIC;

    // If the line search direction is NONLINEAR_CONJUGATE_GRADIENT,
    // then this indicates the particular variant of non-linear
    // conjugate gradient used.
    NonlinearConjugateGradientType nonlinear_conjugate_gradient_type =
        FLETCHER_REEVES;

    // If the type of the line search direction is LBFGS, then this
    // indicates the rank of the Hessian approximation.
    int max_lbfgs_rank = -1;
  };

  // Once a least squares problem has been built, this function takes
  // the problem and optimizes it based on the values of the options
  // parameters. Upon return, a detailed summary of the work performed
  // by the preprocessor, the non-linear minimizer and the linear
  // solver are reported in the summary object.
  virtual void Solve(const GradientProblemSolver::Options& options,
                     const GradientProblem& problem,
                     double* parameters,
                     GradientProblemSolver::Summary* summary);
};

// Helper function which avoids going through the interface.
CERES_EXPORT void Solve(const GradientProblemSolver::Options& options,
                        const GradientProblem& problem,
                        double* parameters,
                        GradientProblemSolver::Summary* summary);

}  // namespace ceres

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_PUBLIC_GRADIENT_PROBLEM_SOLVER_H_
