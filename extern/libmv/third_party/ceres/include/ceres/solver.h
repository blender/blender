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

#ifndef CERES_PUBLIC_SOLVER_H_
#define CERES_PUBLIC_SOLVER_H_

#include <cmath>
#include <string>
#include <vector>

#include "ceres/iteration_callback.h"
#include "ceres/internal/macros.h"
#include "ceres/internal/port.h"
#include "ceres/types.h"

namespace ceres {

class Problem;

// Interface for non-linear least squares solvers.
class Solver {
 public:
  virtual ~Solver();

  // The options structure contains, not surprisingly, options that control how
  // the solver operates. The defaults should be suitable for a wide range of
  // problems; however, better performance is often obtainable with tweaking.
  //
  // The constants are defined inside types.h
  struct Options {
    // Default constructor that sets up a generic sparse problem.
    Options() {
      minimizer_type = LEVENBERG_MARQUARDT;
      max_num_iterations = 50;
      max_solver_time_sec = 1.0e9;
      num_threads = 1;
      tau = 1e-4;
      min_relative_decrease = 1e-3;
      function_tolerance = 1e-6;
      gradient_tolerance = 1e-10;
      parameter_tolerance = 1e-8;
#ifndef CERES_NO_SUITESPARSE
      linear_solver_type = SPARSE_NORMAL_CHOLESKY;
#else
      linear_solver_type = DENSE_QR;
#endif  // CERES_NO_SUITESPARSE
      preconditioner_type = JACOBI;
      num_linear_solver_threads = 1;
      num_eliminate_blocks = 0;
      ordering_type = NATURAL;
      linear_solver_min_num_iterations = 1;
      linear_solver_max_num_iterations = 500;
      eta = 1e-1;
      jacobi_scaling = true;
      logging_type = PER_MINIMIZER_ITERATION;
      minimizer_progress_to_stdout = false;
      return_initial_residuals = false;
      return_final_residuals = false;
      lsqp_dump_directory = "/tmp";
      lsqp_dump_format_type = TEXTFILE;
      crash_and_dump_lsqp_on_failure = false;
      check_gradients = false;
      gradient_check_relative_precision = 1e-8;
      numeric_derivative_relative_step_size = 1e-6;
      update_state_every_iteration = false;
    }

    // Minimizer options ----------------------------------------

    MinimizerType minimizer_type;

    // Maximum number of iterations for the minimizer to run for.
    int max_num_iterations;

    // Maximum time for which the minimizer should run for.
    double max_solver_time_sec;

    // Number of threads used by Ceres for evaluating the cost and
    // jacobians.
    int num_threads;

    // For Levenberg-Marquardt, the initial value for the
    // regularizer. This is the inversely related to the size of the
    // initial trust region.
    double tau;

    // For trust region methods, this is lower threshold for the
    // relative decrease before a step is accepted.
    double min_relative_decrease;

    // Minimizer terminates when
    //
    //   (new_cost - old_cost) < function_tolerance * old_cost;
    //
    double function_tolerance;

    // Minimizer terminates when
    //
    //   max_i |gradient_i| < gradient_tolerance * max_i|initial_gradient_i|
    //
    // This value should typically be 1e-4 * function_tolerance.
    double gradient_tolerance;

    // Minimizer terminates when
    //
    //   |step|_2 <= parameter_tolerance * ( |x|_2 +  parameter_tolerance)
    //
    double parameter_tolerance;

    // Linear least squares solver options -------------------------------------

    LinearSolverType linear_solver_type;

    // Type of preconditioner to use with the iterative linear solvers.
    PreconditionerType preconditioner_type;

    // Number of threads used by Ceres to solve the Newton
    // step. Currently only the SPARSE_SCHUR solver is capable of
    // using this setting.
    int num_linear_solver_threads;

    // For Schur reduction based methods, the first 0 to num blocks are
    // eliminated using the Schur reduction. For example, when solving
    // traditional structure from motion problems where the parameters are in
    // two classes (cameras and points) then num_eliminate_blocks would be the
    // number of points.
    //
    // This parameter is used in conjunction with the ordering.
    // Applies to: Preprocessor and linear least squares solver.
    int num_eliminate_blocks;

    // Internally Ceres reorders the parameter blocks to help the
    // various linear solvers. This parameter allows the user to
    // influence the re-ordering strategy used. For structure from
    // motion problems use SCHUR, for other problems NATURAL (default)
    // is a good choice. In case you wish to specify your own ordering
    // scheme, for example in conjunction with num_eliminate_blocks,
    // use USER.
    OrderingType ordering_type;

    // The ordering of the parameter blocks. The solver pays attention
    // to it if the ordering_type is set to USER and the vector is
    // non-empty.
    vector<double*> ordering;


    // Minimum number of iterations for which the linear solver should
    // run, even if the convergence criterion is satisfied.
    int linear_solver_min_num_iterations;

    // Maximum number of iterations for which the linear solver should
    // run. If the solver does not converge in less than
    // linear_solver_max_num_iterations, then it returns
    // MAX_ITERATIONS, as its termination type.
    int linear_solver_max_num_iterations;

    // Forcing sequence parameter. The truncated Newton solver uses
    // this number to control the relative accuracy with which the
    // Newton step is computed.
    //
    // This constant is passed to ConjugateGradientsSolver which uses
    // it to terminate the iterations when
    //
    //  (Q_i - Q_{i-1})/Q_i < eta/i
    double eta;

    // Normalize the jacobian using Jacobi scaling before calling
    // the linear least squares solver.
    bool jacobi_scaling;

    // Logging options ---------------------------------------------------------

    LoggingType logging_type;

    // By default the Minimizer progress is logged to VLOG(1), which
    // is sent to STDERR depending on the vlog level. If this flag is
    // set to true, and logging_type is not SILENT, the logging output
    // is sent to STDOUT.
    bool minimizer_progress_to_stdout;

    bool return_initial_residuals;
    bool return_final_residuals;

    // List of iterations at which the optimizer should dump the
    // linear least squares problem to disk. Useful for testing and
    // benchmarking. If empty (default), no problems are dumped.
    //
    // This is ignored if protocol buffers are disabled.
    vector<int> lsqp_iterations_to_dump;
    string lsqp_dump_directory;
    DumpFormatType lsqp_dump_format_type;

    // Dump the linear least squares problem to disk if the minimizer
    // fails due to NUMERICAL_FAILURE and crash the process. This flag
    // is useful for generating debugging information. The problem is
    // dumped in a file whose name is determined by
    // Solver::Options::lsqp_dump_format.
    //
    // Note: This requires a version of Ceres built with protocol buffers.
    bool crash_and_dump_lsqp_on_failure;

    // Finite differences options ----------------------------------------------

    // Check all jacobians computed by each residual block with finite
    // differences. This is expensive since it involves computing the
    // derivative by normal means (e.g. user specified, autodiff,
    // etc), then also computing it using finite differences. The
    // results are compared, and if they differ substantially, details
    // are printed to the log.
    bool check_gradients;

    // Relative precision to check for in the gradient checker. If the
    // relative difference between an element in a jacobian exceeds
    // this number, then the jacobian for that cost term is dumped.
    double gradient_check_relative_precision;

    // Relative shift used for taking numeric derivatives. For finite
    // differencing, each dimension is evaluated at slightly shifted
    // values; for the case of central difference, this is what gets
    // evaluated:
    //
    //   delta = numeric_derivative_relative_step_size;
    //   f_initial  = f(x)
    //   f_forward  = f((1 + delta) * x)
    //   f_backward = f((1 - delta) * x)
    //
    // The finite differencing is done along each dimension. The
    // reason to use a relative (rather than absolute) step size is
    // that this way, numeric differentation works for functions where
    // the arguments are typically large (e.g. 1e9) and when the
    // values are small (e.g. 1e-5). It is possible to construct
    // "torture cases" which break this finite difference heuristic,
    // but they do not come up often in practice.
    //
    // TODO(keir): Pick a smarter number than the default above! In
    // theory a good choice is sqrt(eps) * x, which for doubles means
    // about 1e-8 * x. However, I have found this number too
    // optimistic. This number should be exposed for users to change.
    double numeric_derivative_relative_step_size;

    // If true, the user's parameter blocks are updated at the end of
    // every Minimizer iteration, otherwise they are updated when the
    // Minimizer terminates. This is useful if, for example, the user
    // wishes to visualize the state of the optimization every
    // iteration.
    bool update_state_every_iteration;

    // Callbacks that are executed at the end of each iteration of the
    // Minimizer. They are executed in the order that they are
    // specified in this vector. By default, parameter blocks are
    // updated only at the end of the optimization, i.e when the
    // Minimizer terminates. This behaviour is controlled by
    // update_state_every_variable. If the user wishes to have access
    // to the update parameter blocks when his/her callbacks are
    // executed, then set update_state_every_iteration to true.
    //
    // The solver does NOT take ownership of these pointers.
    vector<IterationCallback*> callbacks;
  };

  struct Summary {
    Summary();

    // A brief one line description of the state of the solver after
    // termination.
    string BriefReport() const;

    // A full multiline description of the state of the solver after
    // termination.
    string FullReport() const;

    // Minimizer summary -------------------------------------------------
    SolverTerminationType termination_type;

    // If the solver did not run, or there was a failure, a
    // description of the error.
    string error;

    // Cost of the problem before and after the optimization. See
    // problem.h for definition of the cost of a problem.
    double initial_cost;
    double final_cost;

    // The part of the total cost that comes from residual blocks that
    // were held fixed by the preprocessor because all the parameter
    // blocks that they depend on were fixed.
    double fixed_cost;

    // Residuals before and after the optimization. Each vector
    // contains problem.NumResiduals() elements. Residuals are in the
    // same order in which they were added to the problem object when
    // constructing this problem.
    vector<double> initial_residuals;
    vector<double> final_residuals;

    vector<IterationSummary> iterations;

    int num_successful_steps;
    int num_unsuccessful_steps;

    double preprocessor_time_in_seconds;
    double minimizer_time_in_seconds;
    double total_time_in_seconds;

    // Preprocessor summary.
    int num_parameter_blocks;
    int num_parameters;
    int num_residual_blocks;
    int num_residuals;

    int num_parameter_blocks_reduced;
    int num_parameters_reduced;
    int num_residual_blocks_reduced;
    int num_residuals_reduced;

    int num_eliminate_blocks_given;
    int num_eliminate_blocks_used;

    int num_threads_given;
    int num_threads_used;

    int num_linear_solver_threads_given;
    int num_linear_solver_threads_used;

    LinearSolverType linear_solver_type_given;
    LinearSolverType linear_solver_type_used;

    PreconditionerType preconditioner_type;
    OrderingType ordering_type;
  };

  // Once a least squares problem has been built, this function takes
  // the problem and optimizes it based on the values of the options
  // parameters. Upon return, a detailed summary of the work performed
  // by the preprocessor, the non-linear minmizer and the linear
  // solver are reported in the summary object.
  virtual void Solve(const Options& options,
                     Problem* problem,
                     Solver::Summary* summary);
};

// Helper function which avoids going through the interface.
void Solve(const Solver::Options& options,
           Problem* problem,
           Solver::Summary* summary);

}  // namespace ceres

#endif  // CERES_PUBLIC_SOLVER_H_
