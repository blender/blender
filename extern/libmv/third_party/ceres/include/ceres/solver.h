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
#include "ceres/ordered_groups.h"
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
      minimizer_type = TRUST_REGION;
      line_search_direction_type = LBFGS;
      line_search_type = ARMIJO;
      nonlinear_conjugate_gradient_type = FLETCHER_REEVES;
      max_lbfgs_rank = 20;
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

#if defined(CERES_NO_SUITESPARSE)
      use_block_amd = false;
#else
      use_block_amd = true;
#endif
      linear_solver_ordering = NULL;
      use_inner_iterations = false;
      inner_iteration_ordering = NULL;
      linear_solver_min_num_iterations = 1;
      linear_solver_max_num_iterations = 500;
      eta = 1e-1;
      jacobi_scaling = true;
      logging_type = PER_MINIMIZER_ITERATION;
      minimizer_progress_to_stdout = false;
      lsqp_dump_directory = "/tmp";
      lsqp_dump_format_type = TEXTFILE;
      check_gradients = false;
      gradient_check_relative_precision = 1e-8;
      numeric_derivative_relative_step_size = 1e-6;
      update_state_every_iteration = false;
    }

    ~Options();
    // Minimizer options ----------------------------------------

    // Ceres supports the two major families of optimization strategies -
    // Trust Region and Line Search.
    //
    // 1. The line search approach first finds a descent direction
    // along which the objective function will be reduced and then
    // computes a step size that decides how far should move along
    // that direction. The descent direction can be computed by
    // various methods, such as gradient descent, Newton's method and
    // Quasi-Newton method. The step size can be determined either
    // exactly or inexactly.
    //
    // 2. The trust region approach approximates the objective
    // function using using a model function (often a quadratic) over
    // a subset of the search space known as the trust region. If the
    // model function succeeds in minimizing the true objective
    // function the trust region is expanded; conversely, otherwise it
    // is contracted and the model optimization problem is solved
    // again.
    //
    // Trust region methods are in some sense dual to line search methods:
    // trust region methods first choose a step size (the size of the
    // trust region) and then a step direction while line search methods
    // first choose a step direction and then a step size.
    MinimizerType minimizer_type;

    LineSearchDirectionType line_search_direction_type;
    LineSearchType line_search_type;
    NonlinearConjugateGradientType nonlinear_conjugate_gradient_type;

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
    // Limited Storage". Mathematics of Computation 35 (151): 773–782.
    int max_lbfgs_rank;

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

    // The order in which variables are eliminated in a linear solver
    // can have a significant of impact on the efficiency and accuracy
    // of the method. e.g., when doing sparse Cholesky factorization,
    // there are matrices for which a good ordering will give a
    // Cholesky factor with O(n) storage, where as a bad ordering will
    // result in an completely dense factor.
    //
    // Ceres allows the user to provide varying amounts of hints to
    // the solver about the variable elimination ordering to use. This
    // can range from no hints, where the solver is free to decide the
    // best possible ordering based on the user's choices like the
    // linear solver being used, to an exact order in which the
    // variables should be eliminated, and a variety of possibilities
    // in between.
    //
    // Instances of the ParameterBlockOrdering class are used to
    // communicate this information to Ceres.
    //
    // Formally an ordering is an ordered partitioning of the
    // parameter blocks, i.e, each parameter block belongs to exactly
    // one group, and each group has a unique non-negative integer
    // associated with it, that determines its order in the set of
    // groups.
    //
    // Given such an ordering, Ceres ensures that the parameter blocks in
    // the lowest numbered group are eliminated first, and then the
    // parmeter blocks in the next lowest numbered group and so on. Within
    // each group, Ceres is free to order the parameter blocks as it
    // chooses.
    //
    // If NULL, then all parameter blocks are assumed to be in the
    // same group and the solver is free to decide the best
    // ordering.
    //
    // e.g. Consider the linear system
    //
    //   x + y = 3
    //   2x + 3y = 7
    //
    // There are two ways in which it can be solved. First eliminating x
    // from the two equations, solving for y and then back substituting
    // for x, or first eliminating y, solving for x and back substituting
    // for y. The user can construct three orderings here.
    //
    //   {0: x}, {1: y} - eliminate x first.
    //   {0: y}, {1: x} - eliminate y first.
    //   {0: x, y}      - Solver gets to decide the elimination order.
    //
    // Thus, to have Ceres determine the ordering automatically using
    // heuristics, put all the variables in group 0 and to control the
    // ordering for every variable, create groups 0..N-1, one per
    // variable, in the desired order.
    //
    // Bundle Adjustment
    // -----------------
    //
    // A particular case of interest is bundle adjustment, where the user
    // has two options. The default is to not specify an ordering at all,
    // the solver will see that the user wants to use a Schur type solver
    // and figure out the right elimination ordering.
    //
    // But if the user already knows what parameter blocks are points and
    // what are cameras, they can save preprocessing time by partitioning
    // the parameter blocks into two groups, one for the points and one
    // for the cameras, where the group containing the points has an id
    // smaller than the group containing cameras.
    //
    // Once assigned, Solver::Options owns this pointer and will
    // deallocate the memory when destroyed.
    ParameterBlockOrdering* linear_solver_ordering;

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

    // Some non-linear least squares problems have additional
    // structure in the way the parameter blocks interact that it is
    // beneficial to modify the way the trust region step is computed.
    //
    // e.g., consider the following regression problem
    //
    //   y = a_1 exp(b_1 x) + a_2 exp(b_3 x^2 + c_1)
    //
    // Given a set of pairs{(x_i, y_i)}, the user wishes to estimate
    // a_1, a_2, b_1, b_2, and c_1.
    //
    // Notice here that the expression on the left is linear in a_1
    // and a_2, and given any value for b_1, b_2 and c_1, it is
    // possible to use linear regression to estimate the optimal
    // values of a_1 and a_2. Indeed, its possible to analytically
    // eliminate the variables a_1 and a_2 from the problem all
    // together. Problems like these are known as separable least
    // squares problem and the most famous algorithm for solving them
    // is the Variable Projection algorithm invented by Golub &
    // Pereyra.
    //
    // Similar structure can be found in the matrix factorization with
    // missing data problem. There the corresponding algorithm is
    // known as Wiberg's algorithm.
    //
    // Ruhe & Wedin (Algorithms for Separable Nonlinear Least Squares
    // Problems, SIAM Reviews, 22(3), 1980) present an analyis of
    // various algorithms for solving separable non-linear least
    // squares problems and refer to "Variable Projection" as
    // Algorithm I in their paper.
    //
    // Implementing Variable Projection is tedious and expensive, and
    // they present a simpler algorithm, which they refer to as
    // Algorithm II, where once the Newton/Trust Region step has been
    // computed for the whole problem (a_1, a_2, b_1, b_2, c_1) and
    // additional optimization step is performed to estimate a_1 and
    // a_2 exactly.
    //
    // This idea can be generalized to cases where the residual is not
    // linear in a_1 and a_2, i.e., Solve for the trust region step
    // for the full problem, and then use it as the starting point to
    // further optimize just a_1 and a_2. For the linear case, this
    // amounts to doing a single linear least squares solve. For
    // non-linear problems, any method for solving the a_1 and a_2
    // optimization problems will do. The only constraint on a_1 and
    // a_2 is that they do not co-occur in any residual block.
    //
    // This idea can be further generalized, by not just optimizing
    // (a_1, a_2), but decomposing the graph corresponding to the
    // Hessian matrix's sparsity structure in a collection of
    // non-overlapping independent sets and optimizing each of them.
    //
    // Setting "use_inner_iterations" to true enables the use of this
    // non-linear generalization of Ruhe & Wedin's Algorithm II.  This
    // version of Ceres has a higher iteration complexity, but also
    // displays better convergence behaviour per iteration. Setting
    // Solver::Options::num_threads to the maximum number possible is
    // highly recommended.
    bool use_inner_iterations;

    // If inner_iterations is true, then the user has two choices.
    //
    // 1. Let the solver heuristically decide which parameter blocks
    //    to optimize in each inner iteration. To do this leave
    //    Solver::Options::inner_iteration_ordering untouched.
    //
    // 2. Specify a collection of of ordered independent sets. Where
    //    the lower numbered groups are optimized before the higher
    //    number groups. Each group must be an independent set.
    ParameterBlockOrdering* inner_iteration_ordering;

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
    MinimizerType minimizer_type;

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

    double linear_solver_time_in_seconds;
    double residual_evaluation_time_in_seconds;
    double jacobian_evaluation_time_in_seconds;

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

    vector<int> linear_solver_ordering_given;
    vector<int> linear_solver_ordering_used;

    PreconditionerType preconditioner_type;

    TrustRegionStrategyType trust_region_strategy_type;
    DoglegType dogleg_type;
    bool inner_iterations;

    SparseLinearAlgebraLibraryType sparse_linear_algebra_library;

    LineSearchDirectionType line_search_direction_type;
    LineSearchType line_search_type;
    int max_lbfgs_rank;

    vector<int> inner_iteration_ordering_given;
    vector<int> inner_iteration_ordering_used;
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
