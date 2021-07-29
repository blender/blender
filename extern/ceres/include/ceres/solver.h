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
#include "ceres/internal/disable_warnings.h"

namespace ceres {

class Problem;

// Interface for non-linear least squares solvers.
class CERES_EXPORT Solver {
 public:
  virtual ~Solver();

  // The options structure contains, not surprisingly, options that control how
  // the solver operates. The defaults should be suitable for a wide range of
  // problems; however, better performance is often obtainable with tweaking.
  //
  // The constants are defined inside types.h
  struct CERES_EXPORT Options {
    // Default constructor that sets up a generic sparse problem.
    Options() {
      minimizer_type = TRUST_REGION;
      line_search_direction_type = LBFGS;
      line_search_type = WOLFE;
      nonlinear_conjugate_gradient_type = FLETCHER_REEVES;
      max_lbfgs_rank = 20;
      use_approximate_eigenvalue_bfgs_scaling = false;
      line_search_interpolation_type = CUBIC;
      min_line_search_step_size = 1e-9;
      line_search_sufficient_function_decrease = 1e-4;
      max_line_search_step_contraction = 1e-3;
      min_line_search_step_contraction = 0.6;
      max_num_line_search_step_size_iterations = 20;
      max_num_line_search_direction_restarts = 5;
      line_search_sufficient_curvature_decrease = 0.9;
      max_line_search_step_expansion = 10.0;
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
      min_lm_diagonal = 1e-6;
      max_lm_diagonal = 1e32;
      max_num_consecutive_invalid_steps = 5;
      function_tolerance = 1e-6;
      gradient_tolerance = 1e-10;
      parameter_tolerance = 1e-8;

#if defined(CERES_NO_SUITESPARSE) && defined(CERES_NO_CXSPARSE) && !defined(CERES_ENABLE_LGPL_CODE)  // NOLINT
      linear_solver_type = DENSE_QR;
#else
      linear_solver_type = SPARSE_NORMAL_CHOLESKY;
#endif

      preconditioner_type = JACOBI;
      visibility_clustering_type = CANONICAL_VIEWS;
      dense_linear_algebra_library_type = EIGEN;

      // Choose a default sparse linear algebra library in the order:
      //
      //   SUITE_SPARSE > CX_SPARSE > EIGEN_SPARSE > NO_SPARSE
      sparse_linear_algebra_library_type = NO_SPARSE;
#if !defined(CERES_NO_SUITESPARSE)
      sparse_linear_algebra_library_type = SUITE_SPARSE;
#else
  #if !defined(CERES_NO_CXSPARSE)
      sparse_linear_algebra_library_type = CX_SPARSE;
  #else
    #if defined(CERES_USE_EIGEN_SPARSE)
      sparse_linear_algebra_library_type = EIGEN_SPARSE;
    #endif
  #endif
#endif

      num_linear_solver_threads = 1;
      use_explicit_schur_complement = false;
      use_postordering = false;
      dynamic_sparsity = false;
      min_linear_solver_iterations = 0;
      max_linear_solver_iterations = 500;
      eta = 1e-1;
      jacobi_scaling = true;
      use_inner_iterations = false;
      inner_iteration_tolerance = 1e-3;
      logging_type = PER_MINIMIZER_ITERATION;
      minimizer_progress_to_stdout = false;
      trust_region_problem_dump_directory = "/tmp";
      trust_region_problem_dump_format_type = TEXTFILE;
      check_gradients = false;
      gradient_check_relative_precision = 1e-8;
      gradient_check_numeric_derivative_relative_step_size = 1e-6;
      update_state_every_iteration = false;
    }

    // Returns true if the options struct has a valid
    // configuration. Returns false otherwise, and fills in *error
    // with a message describing the problem.
    bool IsValid(std::string* error) const;

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
    bool use_approximate_eigenvalue_bfgs_scaling;

    // Degree of the polynomial used to approximate the objective
    // function. Valid values are BISECTION, QUADRATIC and CUBIC.
    //
    // BISECTION corresponds to pure backtracking search with no
    // interpolation.
    LineSearchInterpolationType line_search_interpolation_type;

    // If during the line search, the step_size falls below this
    // value, it is truncated to zero.
    double min_line_search_step_size;

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
    double line_search_sufficient_function_decrease;

    // In each iteration of the line search,
    //
    //  new_step_size >= max_line_search_step_contraction * step_size
    //
    // Note that by definition, for contraction:
    //
    //  0 < max_step_contraction < min_step_contraction < 1
    //
    double max_line_search_step_contraction;

    // In each iteration of the line search,
    //
    //  new_step_size <= min_line_search_step_contraction * step_size
    //
    // Note that by definition, for contraction:
    //
    //  0 < max_step_contraction < min_step_contraction < 1
    //
    double min_line_search_step_contraction;

    // Maximum number of trial step size iterations during each line search,
    // if a step size satisfying the search conditions cannot be found within
    // this number of trials, the line search will terminate.
    int max_num_line_search_step_size_iterations;

    // Maximum number of restarts of the line search direction algorithm before
    // terminating the optimization. Restarts of the line search direction
    // algorithm occur when the current algorithm fails to produce a new descent
    // direction. This typically indicates a numerical failure, or a breakdown
    // in the validity of the approximations used.
    int max_num_line_search_direction_restarts;

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
    double line_search_sufficient_curvature_decrease;

    // During the bracketing phase of the Wolfe search, the step size is
    // increased until either a point satisfying the Wolfe conditions is
    // found, or an upper bound for a bracket containing a point satisfying
    // the conditions is found.  Precisely, at each iteration of the
    // expansion:
    //
    //   new_step_size <= max_step_expansion * step_size.
    //
    // By definition for expansion, max_step_expansion > 1.0.
    double max_line_search_step_expansion;

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
    // fail. max_lm_diagonal and min_lm_diagonal, clamp the values of
    // diag(J'J) from above and below. In the normal course of
    // operation, the user should not have to modify these parameters.
    double min_lm_diagonal;
    double max_lm_diagonal;

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
    //   max_i |x - Project(Plus(x, -g(x))| < gradient_tolerance
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

    // Type of clustering algorithm to use for visibility based
    // preconditioning. This option is used only when the
    // preconditioner_type is CLUSTER_JACOBI or CLUSTER_TRIDIAGONAL.
    VisibilityClusteringType visibility_clustering_type;

    // Ceres supports using multiple dense linear algebra libraries
    // for dense matrix factorizations. Currently EIGEN and LAPACK are
    // the valid choices. EIGEN is always available, LAPACK refers to
    // the system BLAS + LAPACK library which may or may not be
    // available.
    //
    // This setting affects the DENSE_QR, DENSE_NORMAL_CHOLESKY and
    // DENSE_SCHUR solvers. For small to moderate sized probem EIGEN
    // is a fine choice but for large problems, an optimized LAPACK +
    // BLAS implementation can make a substantial difference in
    // performance.
    DenseLinearAlgebraLibraryType dense_linear_algebra_library_type;

    // Ceres supports using multiple sparse linear algebra libraries
    // for sparse matrix ordering and factorizations. Currently,
    // SUITE_SPARSE and CX_SPARSE are the valid choices, depending on
    // whether they are linked into Ceres at build time.
    SparseLinearAlgebraLibraryType sparse_linear_algebra_library_type;

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
    shared_ptr<ParameterBlockOrdering> linear_solver_ordering;

    // Use an explicitly computed Schur complement matrix with
    // ITERATIVE_SCHUR.
    //
    // By default this option is disabled and ITERATIVE_SCHUR
    // evaluates evaluates matrix-vector products between the Schur
    // complement and a vector implicitly by exploiting the algebraic
    // expression for the Schur complement.
    //
    // The cost of this evaluation scales with the number of non-zeros
    // in the Jacobian.
    //
    // For small to medium sized problems there is a sweet spot where
    // computing the Schur complement is cheap enough that it is much
    // more efficient to explicitly compute it and use it for evaluating
    // the matrix-vector products.
    //
    // Enabling this option tells ITERATIVE_SCHUR to use an explicitly
    // computed Schur complement.
    //
    // NOTE: This option can only be used with the SCHUR_JACOBI
    // preconditioner.
    bool use_explicit_schur_complement;

    // Sparse Cholesky factorization algorithms use a fill-reducing
    // ordering to permute the columns of the Jacobian matrix. There
    // are two ways of doing this.

    // 1. Compute the Jacobian matrix in some order and then have the
    //    factorization algorithm permute the columns of the Jacobian.

    // 2. Compute the Jacobian with its columns already permuted.

    // The first option incurs a significant memory penalty. The
    // factorization algorithm has to make a copy of the permuted
    // Jacobian matrix, thus Ceres pre-permutes the columns of the
    // Jacobian matrix and generally speaking, there is no performance
    // penalty for doing so.

    // In some rare cases, it is worth using a more complicated
    // reordering algorithm which has slightly better runtime
    // performance at the expense of an extra copy of the Jacobian
    // matrix. Setting use_postordering to true enables this tradeoff.
    bool use_postordering;

    // Some non-linear least squares problems are symbolically dense but
    // numerically sparse. i.e. at any given state only a small number
    // of jacobian entries are non-zero, but the position and number of
    // non-zeros is different depending on the state. For these problems
    // it can be useful to factorize the sparse jacobian at each solver
    // iteration instead of including all of the zero entries in a single
    // general factorization.
    //
    // If your problem does not have this property (or you do not know),
    // then it is probably best to keep this false, otherwise it will
    // likely lead to worse performance.

    // This settings affects the SPARSE_NORMAL_CHOLESKY solver.
    bool dynamic_sparsity;

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
    //    number groups. Each group must be an independent set. Not
    //    all parameter blocks need to be present in the ordering.
    shared_ptr<ParameterBlockOrdering> inner_iteration_ordering;

    // Generally speaking, inner iterations make significant progress
    // in the early stages of the solve and then their contribution
    // drops down sharply, at which point the time spent doing inner
    // iterations is not worth it.
    //
    // Once the relative decrease in the objective function due to
    // inner iterations drops below inner_iteration_tolerance, the use
    // of inner iterations in subsequent trust region minimizer
    // iterations is disabled.
    double inner_iteration_tolerance;

    // Minimum number of iterations for which the linear solver should
    // run, even if the convergence criterion is satisfied.
    int min_linear_solver_iterations;

    // Maximum number of iterations for which the linear solver should
    // run. If the solver does not converge in less than
    // max_linear_solver_iterations, then it returns MAX_ITERATIONS,
    // as its termination type.
    int max_linear_solver_iterations;

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

    // List of iterations at which the minimizer should dump the trust
    // region problem. Useful for testing and benchmarking. If empty
    // (default), no problems are dumped.
    std::vector<int> trust_region_minimizer_iterations_to_dump;

    // Directory to which the problems should be written to. Should be
    // non-empty if trust_region_minimizer_iterations_to_dump is
    // non-empty and trust_region_problem_dump_format_type is not
    // CONSOLE.
    std::string trust_region_problem_dump_directory;
    DumpFormatType trust_region_problem_dump_format_type;

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

    // WARNING: This option only applies to the to the numeric
    // differentiation used for checking the user provided derivatives
    // when when Solver::Options::check_gradients is true. If you are
    // using NumericDiffCostFunction and are interested in changing
    // the step size for numeric differentiation in your cost
    // function, please have a look at
    // include/ceres/numeric_diff_options.h.
    //
    // Relative shift used for taking numeric derivatives when
    // Solver::Options::check_gradients is true.
    //
    // For finite differencing, each dimension is evaluated at
    // slightly shifted values; for the case of central difference,
    // this is what gets evaluated:
    //
    //   delta = gradient_check_numeric_derivative_relative_step_size;
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
    double gradient_check_numeric_derivative_relative_step_size;

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
    std::vector<IterationCallback*> callbacks;
  };

  struct CERES_EXPORT Summary {
    Summary();

    // A brief one line description of the state of the solver after
    // termination.
    std::string BriefReport() const;

    // A full multiline description of the state of the solver after
    // termination.
    std::string FullReport() const;

    bool IsSolutionUsable() const;

    // Minimizer summary -------------------------------------------------
    MinimizerType minimizer_type;

    TerminationType termination_type;

    // Reason why the solver terminated.
    std::string message;

    // Cost of the problem (value of the objective function) before
    // the optimization.
    double initial_cost;

    // Cost of the problem (value of the objective function) after the
    // optimization.
    double final_cost;

    // The part of the total cost that comes from residual blocks that
    // were held fixed by the preprocessor because all the parameter
    // blocks that they depend on were fixed.
    double fixed_cost;

    // IterationSummary for each minimizer iteration in order.
    std::vector<IterationSummary> iterations;

    // Number of minimizer iterations in which the step was
    // accepted. Unless use_non_monotonic_steps is true this is also
    // the number of steps in which the objective function value/cost
    // went down.
    int num_successful_steps;

    // Number of minimizer iterations in which the step was rejected
    // either because it did not reduce the cost enough or the step
    // was not numerically valid.
    int num_unsuccessful_steps;

    // Number of times inner iterations were performed.
    int num_inner_iteration_steps;

    // Total number of iterations inside the line search algorithm
    // across all invocations. We call these iterations "steps" to
    // distinguish them from the outer iterations of the line search
    // and trust region minimizer algorithms which call the line
    // search algorithm as a subroutine.
    int num_line_search_steps;

    // All times reported below are wall times.

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

    // Time (in seconds) spent in the linear solver computing the
    // trust region step.
    double linear_solver_time_in_seconds;

    // Time (in seconds) spent evaluating the residual vector.
    double residual_evaluation_time_in_seconds;

    // Time (in seconds) spent evaluating the jacobian matrix.
    double jacobian_evaluation_time_in_seconds;

    // Time (in seconds) spent doing inner iterations.
    double inner_iteration_time_in_seconds;

    // Cumulative timing information for line searches performed as part of the
    // solve.  Note that in addition to the case when the Line Search minimizer
    // is used, the Trust Region minimizer also uses a line search when
    // solving a constrained problem.

    // Time (in seconds) spent evaluating the univariate cost function as part
    // of a line search.
    double line_search_cost_evaluation_time_in_seconds;

    // Time (in seconds) spent evaluating the gradient of the univariate cost
    // function as part of a line search.
    double line_search_gradient_evaluation_time_in_seconds;

    // Time (in seconds) spent minimizing the interpolating polynomial
    // to compute the next candidate step size as part of a line search.
    double line_search_polynomial_minimization_time_in_seconds;

    // Total time (in seconds) spent performing line searches.
    double line_search_total_time_in_seconds;

    // Number of parameter blocks in the problem.
    int num_parameter_blocks;

    // Number of parameters in the probem.
    int num_parameters;

    // Dimension of the tangent space of the problem (or the number of
    // columns in the Jacobian for the problem). This is different
    // from num_parameters if a parameter block is associated with a
    // LocalParameterization
    int num_effective_parameters;

    // Number of residual blocks in the problem.
    int num_residual_blocks;

    // Number of residuals in the problem.
    int num_residuals;

    // Number of parameter blocks in the problem after the inactive
    // and constant parameter blocks have been removed. A parameter
    // block is inactive if no residual block refers to it.
    int num_parameter_blocks_reduced;

    // Number of parameters in the reduced problem.
    int num_parameters_reduced;

    // Dimension of the tangent space of the reduced problem (or the
    // number of columns in the Jacobian for the reduced
    // problem). This is different from num_parameters_reduced if a
    // parameter block in the reduced problem is associated with a
    // LocalParameterization.
    int num_effective_parameters_reduced;

    // Number of residual blocks in the reduced problem.
    int num_residual_blocks_reduced;

    //  Number of residuals in the reduced problem.
    int num_residuals_reduced;

    // Is the reduced problem bounds constrained.
    bool is_constrained;

    //  Number of threads specified by the user for Jacobian and
    //  residual evaluation.
    int num_threads_given;

    // Number of threads actually used by the solver for Jacobian and
    // residual evaluation. This number is not equal to
    // num_threads_given if OpenMP is not available.
    int num_threads_used;

    //  Number of threads specified by the user for solving the trust
    // region problem.
    int num_linear_solver_threads_given;

    // Number of threads actually used by the solver for solving the
    // trust region problem. This number is not equal to
    // num_threads_given if OpenMP is not available.
    int num_linear_solver_threads_used;

    // Type of the linear solver requested by the user.
    LinearSolverType linear_solver_type_given;

    // Type of the linear solver actually used. This may be different
    // from linear_solver_type_given if Ceres determines that the
    // problem structure is not compatible with the linear solver
    // requested or if the linear solver requested by the user is not
    // available, e.g. The user requested SPARSE_NORMAL_CHOLESKY but
    // no sparse linear algebra library was available.
    LinearSolverType linear_solver_type_used;

    // Size of the elimination groups given by the user as hints to
    // the linear solver.
    std::vector<int> linear_solver_ordering_given;

    // Size of the parameter groups used by the solver when ordering
    // the columns of the Jacobian.  This maybe different from
    // linear_solver_ordering_given if the user left
    // linear_solver_ordering_given blank and asked for an automatic
    // ordering, or if the problem contains some constant or inactive
    // parameter blocks.
    std::vector<int> linear_solver_ordering_used;

    // True if the user asked for inner iterations to be used as part
    // of the optimization.
    bool inner_iterations_given;

    // True if the user asked for inner iterations to be used as part
    // of the optimization and the problem structure was such that
    // they were actually performed. e.g., in a problem with just one
    // parameter block, inner iterations are not performed.
    bool inner_iterations_used;

    // Size of the parameter groups given by the user for performing
    // inner iterations.
    std::vector<int> inner_iteration_ordering_given;

    // Size of the parameter groups given used by the solver for
    // performing inner iterations. This maybe different from
    // inner_iteration_ordering_given if the user left
    // inner_iteration_ordering_given blank and asked for an automatic
    // ordering, or if the problem contains some constant or inactive
    // parameter blocks.
    std::vector<int> inner_iteration_ordering_used;

    // Type of the preconditioner requested by the user.
    PreconditionerType preconditioner_type_given;

    // Type of the preconditioner actually used. This may be different
    // from linear_solver_type_given if Ceres determines that the
    // problem structure is not compatible with the linear solver
    // requested or if the linear solver requested by the user is not
    // available.
    PreconditionerType preconditioner_type_used;

    // Type of clustering algorithm used for visibility based
    // preconditioning. Only meaningful when the preconditioner_type
    // is CLUSTER_JACOBI or CLUSTER_TRIDIAGONAL.
    VisibilityClusteringType visibility_clustering_type;

    //  Type of trust region strategy.
    TrustRegionStrategyType trust_region_strategy_type;

    //  Type of dogleg strategy used for solving the trust region
    //  problem.
    DoglegType dogleg_type;

    //  Type of the dense linear algebra library used.
    DenseLinearAlgebraLibraryType dense_linear_algebra_library_type;

    // Type of the sparse linear algebra library used.
    SparseLinearAlgebraLibraryType sparse_linear_algebra_library_type;

    // Type of line search direction used.
    LineSearchDirectionType line_search_direction_type;

    // Type of the line search algorithm used.
    LineSearchType line_search_type;

    //  When performing line search, the degree of the polynomial used
    //  to approximate the objective function.
    LineSearchInterpolationType line_search_interpolation_type;

    // If the line search direction is NONLINEAR_CONJUGATE_GRADIENT,
    // then this indicates the particular variant of non-linear
    // conjugate gradient used.
    NonlinearConjugateGradientType nonlinear_conjugate_gradient_type;

    // If the type of the line search direction is LBFGS, then this
    // indicates the rank of the Hessian approximation.
    int max_lbfgs_rank;
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
CERES_EXPORT void Solve(const Solver::Options& options,
           Problem* problem,
           Solver::Summary* summary);

}  // namespace ceres

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_PUBLIC_SOLVER_H_
