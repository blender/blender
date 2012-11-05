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
#include "ceres/crs_matrix.h"
#include "ceres/internal/macros.h"
#include "ceres/internal/port.h"
#include "ceres/iteration_callback.h"
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
      trust_region_strategy_type = LEVENBERG_MARQUARDT;
      dogleg_type = TRADITIONAL_DOGLEG;
      use_nonmonotonic_steps = false;
      max_consecutive_nonmonotonic_steps = 5;
      max_num_iterations = 50;
      max_solver_time_in_seconds = 1e9;
      num_threads = 1;
      initial_trust_region_radius = 1e4;
      max_trust_region_radius = 1e16;
      min_trust_region_radius = 1e-32;
      min_relative_decrease = 1e-3;
      lm_min_diagonal = 1e-6;
      lm_max_diagonal = 1e32;
      max_num_consecutive_invalid_steps = 5;
      function_tolerance = 1e-6;
      gradient_tolerance = 1e-10;
      parameter_tolerance = 1e-8;

#if defined(CERES_NO_SUITESPARSE) && defined(CERES_NO_CXSPARSE)
      linear_solver_type = DENSE_QR;
#else
      linear_solver_type = SPARSE_NORMAL_CHOLESKY;
#endif

      preconditioner_type = JACOBI;

      sparse_linear_algebra_library = SUITE_SPARSE;
#if defined(CERES_NO_SUITESPARSE) && !defined(CERES_NO_CXSPARSE)
      sparse_linear_algebra_library = CX_SPARSE;
#endif

      num_linear_solver_threads = 1;
      num_eliminate_blocks = 0;
      ordering_type = NATURAL;

#if defined(CERES_NO_SUITESPARSE)
      use_block_amd = false;
#else
      use_block_amd = true;
#endif

      linear_solver_min_num_iterations = 1;
      linear_solver_max_num_iterations = 500;
      eta = 1e-1;
      jacobi_scaling = true;
      logging_type = PER_MINIMIZER_ITERATION;
      minimizer_progress_to_stdout = false;
      return_initial_residuals = false;
      return_initial_gradient = false;
      return_initial_jacobian = false;
      return_final_residuals = false;
      return_final_gradient = false;
      return_final_jacobian = false;
      lsqp_dump_directory = "/tmp";
      lsqp_dump_format_type = TEXTFILE;
      check_gradients = false;
      gradient_check_relative_precision = 1e-8;
      numeric_derivative_relative_step_size = 1e-6;
      update_state_every_iteration = false;
    }

    // Minimizer options ----------------------------------------

    TrustRegionStrategyType trust_region_strategy_type;

    // Type of dogleg strategy to use.
    DoglegType dogleg_type;

    // The classical trust region methods are descent methods, in that
    // they only accept a point if it strictly reduces the value of
    // the objective function.
    //
    // Relaxing this requirement allows the algorithm to be more
    // efficient in the long term at the cost of some local increase
    // in the value of the objective function.
    //
    // This is because allowing for non-decreasing objective function
    // values in a princpled manner allows the algorithm to "jump over
    // boulders" as the method is not restricted to move into narrow
    // valleys while preserving its convergence properties.
    //
    // Setting use_nonmonotonic_steps to true enables the
    // non-monotonic trust region algorithm as described by Conn,
    // Gould & Toint in "Trust Region Methods", Section 10.1.
    //
    // The parameter max_consecutive_nonmonotonic_steps controls the
    // window size used by the step selection algorithm to accept
    // non-monotonic steps.
    //
    // Even though the value of the objective function may be larger
    // than the minimum value encountered over the course of the
    // optimization, the final parameters returned to the user are the
    // ones corresponding to the minimum cost over all iterations.
    bool use_nonmonotonic_steps;
    int max_consecutive_nonmonotonic_steps;

    // Maximum number of iterations for the minimizer to run for.
    int max_num_iterations;

    // Maximum time for which the minimizer should run for.
    double max_solver_time_in_seconds;

    // Number of threads used by Ceres for evaluating the cost and
    // jacobians.
    int num_threads;

    // Trust region minimizer settings.
    double initial_trust_region_radius;
    double max_trust_region_radius;

    // Minimizer terminates when the trust region radius becomes
    // smaller than this value.
    double min_trust_region_radius;

    // Lower bound for the relative decrease before a step is
    // accepted.
    double min_relative_decrease;

    // For the Levenberg-Marquadt algorithm, the scaled diagonal of
    // the normal equations J'J is used to control the size of the
    // trust region. Extremely small and large values along the
    // diagonal can make this regularization scheme
    // fail. lm_max_diagonal and lm_min_diagonal, clamp the values of
    // diag(J'J) from above and below. In the normal course of
    // operation, the user should not have to modify these parameters.
    double lm_min_diagonal;
    double lm_max_diagonal;

    // Sometimes due to numerical conditioning problems or linear
    // solver flakiness, the trust region strategy may return a
    // numerically invalid step that can be fixed by reducing the
    // trust region size. So the TrustRegionMinimizer allows for a few
    // successive invalid steps before it declares NUMERICAL_FAILURE.
    int max_num_consecutive_invalid_steps;

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

    // Ceres supports using multiple sparse linear algebra libraries
    // for sparse matrix ordering and factorizations. Currently,
    // SUITE_SPARSE and CX_SPARSE are the valid choices, depending on
    // whether they are linked into Ceres at build time.
    SparseLinearAlgebraLibraryType sparse_linear_algebra_library;

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

    // By virtue of the modeling layer in Ceres being block oriented,
    // all the matrices used by Ceres are also block oriented. When
    // doing sparse direct factorization of these matrices (for
    // SPARSE_NORMAL_CHOLESKY, SPARSE_SCHUR and ITERATIVE in
    // conjunction with CLUSTER_TRIDIAGONAL AND CLUSTER_JACOBI
    // preconditioners), the fill-reducing ordering algorithms can
    // either be run on the block or the scalar form of these matrices.
    // Running it on the block form exposes more of the super-nodal
    // structure of the matrix to the factorization routines. Setting
    // this parameter to true runs the ordering algorithms in block
    // form. Currently this option only makes sense with
    // sparse_linear_algebra_library = SUITE_SPARSE.
    bool use_block_amd;

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
    bool return_initial_gradient;
    bool return_initial_jacobian;

    bool return_final_residuals;
    bool return_final_gradient;
    bool return_final_jacobian;

    // List of iterations at which the optimizer should dump the
    // linear least squares problem to disk. Useful for testing and
    // benchmarking. If empty (default), no problems are dumped.
    //
    // This is ignored if protocol buffers are disabled.
    vector<int> lsqp_iterations_to_dump;
    string lsqp_dump_directory;
    DumpFormatType lsqp_dump_format_type;

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
    vector<IterationCallback*> callbacks;

    // If non-empty, a summary of the execution of the solver is
    // recorded to this file.
    string solver_log;
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

    // Vectors of residuals before and after the optimization. The
    // entries of these vectors are in the order in which
    // ResidualBlocks were added to the Problem object.
    //
    // Whether the residual vectors are populated with values is
    // controlled by Solver::Options::return_initial_residuals and
    // Solver::Options::return_final_residuals respectively.
    vector<double> initial_residuals;
    vector<double> final_residuals;

    // Gradient vectors, before and after the optimization.  The rows
    // are in the same order in which the ParameterBlocks were added
    // to the Problem object.
    //
    // NOTE: Since AddResidualBlock adds ParameterBlocks to the
    // Problem automatically if they do not already exist, if you wish
    // to have explicit control over the ordering of the vectors, then
    // use Problem::AddParameterBlock to explicitly add the
    // ParameterBlocks in the order desired.
    //
    // Whether the vectors are populated with values is controlled by
    // Solver::Options::return_initial_gradient and
    // Solver::Options::return_final_gradient respectively.
    vector<double> initial_gradient;
    vector<double> final_gradient;

    // Jacobian matrices before and after the optimization. The rows
    // of these matrices are in the same order in which the
    // ResidualBlocks were added to the Problem object. The columns
    // are in the same order in which the ParameterBlocks were added
    // to the Problem object.
    //
    // NOTE: Since AddResidualBlock adds ParameterBlocks to the
    // Problem automatically if they do not already exist, if you wish
    // to have explicit control over the column ordering of the
    // matrix, then use Problem::AddParameterBlock to explicitly add
    // the ParameterBlocks in the order desired.
    //
    // The Jacobian matrices are stored as compressed row sparse
    // matrices. Please see ceres/crs_matrix.h for more details of the
    // format.
    //
    // Whether the Jacboan matrices are populated with values is
    // controlled by Solver::Options::return_initial_jacobian and
    // Solver::Options::return_final_jacobian respectively.
    CRSMatrix initial_jacobian;
    CRSMatrix final_jacobian;

    vector<IterationSummary> iterations;

    int num_successful_steps;
    int num_unsuccessful_steps;

    // When the user calls Solve, before the actual optimization
    // occurs, Ceres performs a number of preprocessing steps. These
    // include error checks, memory allocations, and reorderings. This
    // time is accounted for as preprocessing time.
    double preprocessor_time_in_seconds;

    // Time spent in the TrustRegionMinimizer.
    double minimizer_time_in_seconds;

    // After the Minimizer is finished, some time is spent in
    // re-evaluating residuals etc. This time is accounted for in the
    // postprocessor time.
    double postprocessor_time_in_seconds;

    // Some total of all time spent inside Ceres when Solve is called.
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

    TrustRegionStrategyType trust_region_strategy_type;
    DoglegType dogleg_type;
    SparseLinearAlgebraLibraryType sparse_linear_algebra_library;
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
