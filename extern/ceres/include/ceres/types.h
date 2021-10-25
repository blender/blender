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
//
// Enums and other top level class definitions.
//
// Note: internal/types.cc defines stringification routines for some
// of these enums. Please update those routines if you extend or
// remove enums from here.

#ifndef CERES_PUBLIC_TYPES_H_
#define CERES_PUBLIC_TYPES_H_

#include <string>

#include "ceres/internal/port.h"
#include "ceres/internal/disable_warnings.h"

namespace ceres {

// Basic integer types. These typedefs are in the Ceres namespace to avoid
// conflicts with other packages having similar typedefs.
typedef int   int32;

// Argument type used in interfaces that can optionally take ownership
// of a passed in argument. If TAKE_OWNERSHIP is passed, the called
// object takes ownership of the pointer argument, and will call
// delete on it upon completion.
enum Ownership {
  DO_NOT_TAKE_OWNERSHIP,
  TAKE_OWNERSHIP
};

// TODO(keir): Considerably expand the explanations of each solver type.
enum LinearSolverType {
  // These solvers are for general rectangular systems formed from the
  // normal equations A'A x = A'b. They are direct solvers and do not
  // assume any special problem structure.

  // Solve the normal equations using a dense Cholesky solver; based
  // on Eigen.
  DENSE_NORMAL_CHOLESKY,

  // Solve the normal equations using a dense QR solver; based on
  // Eigen.
  DENSE_QR,

  // Solve the normal equations using a sparse cholesky solver; requires
  // SuiteSparse or CXSparse.
  SPARSE_NORMAL_CHOLESKY,

  // Specialized solvers, specific to problems with a generalized
  // bi-partitite structure.

  // Solves the reduced linear system using a dense Cholesky solver;
  // based on Eigen.
  DENSE_SCHUR,

  // Solves the reduced linear system using a sparse Cholesky solver;
  // based on CHOLMOD.
  SPARSE_SCHUR,

  // Solves the reduced linear system using Conjugate Gradients, based
  // on a new Ceres implementation.  Suitable for large scale
  // problems.
  ITERATIVE_SCHUR,

  // Conjugate gradients on the normal equations.
  CGNR
};

enum PreconditionerType {
  // Trivial preconditioner - the identity matrix.
  IDENTITY,

  // Block diagonal of the Gauss-Newton Hessian.
  JACOBI,

  // Note: The following three preconditioners can only be used with
  // the ITERATIVE_SCHUR solver. They are well suited for Structure
  // from Motion problems.

  // Block diagonal of the Schur complement. This preconditioner may
  // only be used with the ITERATIVE_SCHUR solver.
  SCHUR_JACOBI,

  // Visibility clustering based preconditioners.
  //
  // The following two preconditioners use the visibility structure of
  // the scene to determine the sparsity structure of the
  // preconditioner. This is done using a clustering algorithm. The
  // available visibility clustering algorithms are described below.
  //
  // Note: Requires SuiteSparse.
  CLUSTER_JACOBI,
  CLUSTER_TRIDIAGONAL
};

enum VisibilityClusteringType {
  // Canonical views algorithm as described in
  //
  // "Scene Summarization for Online Image Collections", Ian Simon, Noah
  // Snavely, Steven M. Seitz, ICCV 2007.
  //
  // This clustering algorithm can be quite slow, but gives high
  // quality clusters. The original visibility based clustering paper
  // used this algorithm.
  CANONICAL_VIEWS,

  // The classic single linkage algorithm. It is extremely fast as
  // compared to CANONICAL_VIEWS, but can give slightly poorer
  // results. For problems with large number of cameras though, this
  // is generally a pretty good option.
  //
  // If you are using SCHUR_JACOBI preconditioner and have SuiteSparse
  // available, CLUSTER_JACOBI and CLUSTER_TRIDIAGONAL in combination
  // with the SINGLE_LINKAGE algorithm will generally give better
  // results.
  SINGLE_LINKAGE
};

enum SparseLinearAlgebraLibraryType {
  // High performance sparse Cholesky factorization and approximate
  // minimum degree ordering.
  SUITE_SPARSE,

  // A lightweight replacment for SuiteSparse, which does not require
  // a LAPACK/BLAS implementation. Consequently, its performance is
  // also a bit lower than SuiteSparse.
  CX_SPARSE,

  // Eigen's sparse linear algebra routines. In particular Ceres uses
  // the Simplicial LDLT routines.
  EIGEN_SPARSE,

  // No sparse linear solver should be used.  This does not necessarily
  // imply that Ceres was built without any sparse library, although that
  // is the likely use case, merely that one should not be used.
  NO_SPARSE
};

enum DenseLinearAlgebraLibraryType {
  EIGEN,
  LAPACK
};

// Logging options
// The options get progressively noisier.
enum LoggingType {
  SILENT,
  PER_MINIMIZER_ITERATION
};

enum MinimizerType {
  LINE_SEARCH,
  TRUST_REGION
};

enum LineSearchDirectionType {
  // Negative of the gradient.
  STEEPEST_DESCENT,

  // A generalization of the Conjugate Gradient method to non-linear
  // functions. The generalization can be performed in a number of
  // different ways, resulting in a variety of search directions. The
  // precise choice of the non-linear conjugate gradient algorithm
  // used is determined by NonlinerConjuateGradientType.
  NONLINEAR_CONJUGATE_GRADIENT,

  // BFGS, and it's limited memory approximation L-BFGS, are quasi-Newton
  // algorithms that approximate the Hessian matrix by iteratively refining
  // an initial estimate with rank-one updates using the gradient at each
  // iteration. They are a generalisation of the Secant method and satisfy
  // the Secant equation.  The Secant equation has an infinium of solutions
  // in multiple dimensions, as there are N*(N+1)/2 degrees of freedom in a
  // symmetric matrix but only N conditions are specified by the Secant
  // equation. The requirement that the Hessian approximation be positive
  // definite imposes another N additional constraints, but that still leaves
  // remaining degrees-of-freedom.  (L)BFGS methods uniquely deteremine the
  // approximate Hessian by imposing the additional constraints that the
  // approximation at the next iteration must be the 'closest' to the current
  // approximation (the nature of how this proximity is measured is actually
  // the defining difference between a family of quasi-Newton methods including
  // (L)BFGS & DFP). (L)BFGS is currently regarded as being the best known
  // general quasi-Newton method.
  //
  // The principal difference between BFGS and L-BFGS is that whilst BFGS
  // maintains a full, dense approximation to the (inverse) Hessian, L-BFGS
  // maintains only a window of the last M observations of the parameters and
  // gradients. Using this observation history, the calculation of the next
  // search direction can be computed without requiring the construction of the
  // full dense inverse Hessian approximation. This is particularly important
  // for problems with a large number of parameters, where storage of an N-by-N
  // matrix in memory would be prohibitive.
  //
  // For more details on BFGS see:
  //
  // Broyden, C.G., "The Convergence of a Class of Double-rank Minimization
  // Algorithms,"; J. Inst. Maths. Applics., Vol. 6, pp 76–90, 1970.
  //
  // Fletcher, R., "A New Approach to Variable Metric Algorithms,"
  // Computer Journal, Vol. 13, pp 317–322, 1970.
  //
  // Goldfarb, D., "A Family of Variable Metric Updates Derived by Variational
  // Means," Mathematics of Computing, Vol. 24, pp 23–26, 1970.
  //
  // Shanno, D.F., "Conditioning of Quasi-Newton Methods for Function
  // Minimization," Mathematics of Computing, Vol. 24, pp 647–656, 1970.
  //
  // For more details on L-BFGS see:
  //
  // Nocedal, J. (1980). "Updating Quasi-Newton Matrices with Limited
  // Storage". Mathematics of Computation 35 (151): 773–782.
  //
  // Byrd, R. H.; Nocedal, J.; Schnabel, R. B. (1994).
  // "Representations of Quasi-Newton Matrices and their use in
  // Limited Memory Methods". Mathematical Programming 63 (4):
  // 129–156.
  //
  // A general reference for both methods:
  //
  // Nocedal J., Wright S., Numerical Optimization, 2nd Ed. Springer, 1999.
  LBFGS,
  BFGS,
};

// Nonliner conjugate gradient methods are a generalization of the
// method of Conjugate Gradients for linear systems. The
// generalization can be carried out in a number of different ways
// leading to number of different rules for computing the search
// direction. Ceres provides a number of different variants. For more
// details see Numerical Optimization by Nocedal & Wright.
enum NonlinearConjugateGradientType {
  FLETCHER_REEVES,
  POLAK_RIBIERE,
  HESTENES_STIEFEL,
};

enum LineSearchType {
  // Backtracking line search with polynomial interpolation or
  // bisection.
  ARMIJO,
  WOLFE,
};

// Ceres supports different strategies for computing the trust region
// step.
enum TrustRegionStrategyType {
  // The default trust region strategy is to use the step computation
  // used in the Levenberg-Marquardt algorithm. For more details see
  // levenberg_marquardt_strategy.h
  LEVENBERG_MARQUARDT,

  // Powell's dogleg algorithm interpolates between the Cauchy point
  // and the Gauss-Newton step. It is particularly useful if the
  // LEVENBERG_MARQUARDT algorithm is making a large number of
  // unsuccessful steps. For more details see dogleg_strategy.h.
  //
  // NOTES:
  //
  // 1. This strategy has not been experimented with or tested as
  // extensively as LEVENBERG_MARQUARDT, and therefore it should be
  // considered EXPERIMENTAL for now.
  //
  // 2. For now this strategy should only be used with exact
  // factorization based linear solvers, i.e., SPARSE_SCHUR,
  // DENSE_SCHUR, DENSE_QR and SPARSE_NORMAL_CHOLESKY.
  DOGLEG
};

// Ceres supports two different dogleg strategies.
// The "traditional" dogleg method by Powell and the
// "subspace" method described in
// R. H. Byrd, R. B. Schnabel, and G. A. Shultz,
// "Approximate solution of the trust region problem by minimization
//  over two-dimensional subspaces", Mathematical Programming,
// 40 (1988), pp. 247--263
enum DoglegType {
  // The traditional approach constructs a dogleg path
  // consisting of two line segments and finds the furthest
  // point on that path that is still inside the trust region.
  TRADITIONAL_DOGLEG,

  // The subspace approach finds the exact minimum of the model
  // constrained to the subspace spanned by the dogleg path.
  SUBSPACE_DOGLEG
};

enum TerminationType {
  // Minimizer terminated because one of the convergence criterion set
  // by the user was satisfied.
  //
  // 1.  (new_cost - old_cost) < function_tolerance * old_cost;
  // 2.  max_i |gradient_i| < gradient_tolerance
  // 3.  |step|_2 <= parameter_tolerance * ( |x|_2 +  parameter_tolerance)
  //
  // The user's parameter blocks will be updated with the solution.
  CONVERGENCE,

  // The solver ran for maximum number of iterations or maximum amount
  // of time specified by the user, but none of the convergence
  // criterion specified by the user were met. The user's parameter
  // blocks will be updated with the solution found so far.
  NO_CONVERGENCE,

  // The minimizer terminated because of an error.  The user's
  // parameter blocks will not be updated.
  FAILURE,

  // Using an IterationCallback object, user code can control the
  // minimizer. The following enums indicate that the user code was
  // responsible for termination.
  //
  // Minimizer terminated successfully because a user
  // IterationCallback returned SOLVER_TERMINATE_SUCCESSFULLY.
  //
  // The user's parameter blocks will be updated with the solution.
  USER_SUCCESS,

  // Minimizer terminated because because a user IterationCallback
  // returned SOLVER_ABORT.
  //
  // The user's parameter blocks will not be updated.
  USER_FAILURE
};

// Enums used by the IterationCallback instances to indicate to the
// solver whether it should continue solving, the user detected an
// error or the solution is good enough and the solver should
// terminate.
enum CallbackReturnType {
  // Continue solving to next iteration.
  SOLVER_CONTINUE,

  // Terminate solver, and do not update the parameter blocks upon
  // return. Unless the user has set
  // Solver:Options:::update_state_every_iteration, in which case the
  // state would have been updated every iteration
  // anyways. Solver::Summary::termination_type is set to USER_ABORT.
  SOLVER_ABORT,

  // Terminate solver, update state and
  // return. Solver::Summary::termination_type is set to USER_SUCCESS.
  SOLVER_TERMINATE_SUCCESSFULLY
};

// The format in which linear least squares problems should be logged
// when Solver::Options::lsqp_iterations_to_dump is non-empty.
enum DumpFormatType {
  // Print the linear least squares problem in a human readable format
  // to stderr. The Jacobian is printed as a dense matrix. The vectors
  // D, x and f are printed as dense vectors. This should only be used
  // for small problems.
  CONSOLE,

  // Write out the linear least squares problem to the directory
  // pointed to by Solver::Options::lsqp_dump_directory as text files
  // which can be read into MATLAB/Octave. The Jacobian is dumped as a
  // text file containing (i,j,s) triplets, the vectors D, x and f are
  // dumped as text files containing a list of their values.
  //
  // A MATLAB/octave script called lm_iteration_???.m is also output,
  // which can be used to parse and load the problem into memory.
  TEXTFILE
};

// For SizedCostFunction and AutoDiffCostFunction, DYNAMIC can be
// specified for the number of residuals. If specified, then the
// number of residuas for that cost function can vary at runtime.
enum DimensionType {
  DYNAMIC = -1
};

// The differentiation method used to compute numerical derivatives in
// NumericDiffCostFunction and DynamicNumericDiffCostFunction.
enum NumericDiffMethodType {
  // Compute central finite difference: f'(x) ~ (f(x+h) - f(x-h)) / 2h.
  CENTRAL,

  // Compute forward finite difference: f'(x) ~ (f(x+h) - f(x)) / h.
  FORWARD,

  // Adaptive numerical differentiation using Ridders' method. Provides more
  // accurate and robust derivatives at the expense of additional cost
  // function evaluations.
  RIDDERS
};

enum LineSearchInterpolationType {
  BISECTION,
  QUADRATIC,
  CUBIC
};

enum CovarianceAlgorithmType {
  DENSE_SVD,
  SUITE_SPARSE_QR,
  EIGEN_SPARSE_QR
};

CERES_EXPORT const char* LinearSolverTypeToString(
    LinearSolverType type);
CERES_EXPORT bool StringToLinearSolverType(std::string value,
                                           LinearSolverType* type);

CERES_EXPORT const char* PreconditionerTypeToString(PreconditionerType type);
CERES_EXPORT bool StringToPreconditionerType(std::string value,
                                             PreconditionerType* type);

CERES_EXPORT const char* VisibilityClusteringTypeToString(
    VisibilityClusteringType type);
CERES_EXPORT bool StringToVisibilityClusteringType(std::string value,
                                      VisibilityClusteringType* type);

CERES_EXPORT const char* SparseLinearAlgebraLibraryTypeToString(
    SparseLinearAlgebraLibraryType type);
CERES_EXPORT bool StringToSparseLinearAlgebraLibraryType(
    std::string value,
    SparseLinearAlgebraLibraryType* type);

CERES_EXPORT const char* DenseLinearAlgebraLibraryTypeToString(
    DenseLinearAlgebraLibraryType type);
CERES_EXPORT bool StringToDenseLinearAlgebraLibraryType(
    std::string value,
    DenseLinearAlgebraLibraryType* type);

CERES_EXPORT const char* TrustRegionStrategyTypeToString(
    TrustRegionStrategyType type);
CERES_EXPORT bool StringToTrustRegionStrategyType(std::string value,
                                     TrustRegionStrategyType* type);

CERES_EXPORT const char* DoglegTypeToString(DoglegType type);
CERES_EXPORT bool StringToDoglegType(std::string value, DoglegType* type);

CERES_EXPORT const char* MinimizerTypeToString(MinimizerType type);
CERES_EXPORT bool StringToMinimizerType(std::string value, MinimizerType* type);

CERES_EXPORT const char* LineSearchDirectionTypeToString(
    LineSearchDirectionType type);
CERES_EXPORT bool StringToLineSearchDirectionType(std::string value,
                                     LineSearchDirectionType* type);

CERES_EXPORT const char* LineSearchTypeToString(LineSearchType type);
CERES_EXPORT bool StringToLineSearchType(std::string value, LineSearchType* type);

CERES_EXPORT const char* NonlinearConjugateGradientTypeToString(
    NonlinearConjugateGradientType type);
CERES_EXPORT bool StringToNonlinearConjugateGradientType(
    std::string value,
    NonlinearConjugateGradientType* type);

CERES_EXPORT const char* LineSearchInterpolationTypeToString(
    LineSearchInterpolationType type);
CERES_EXPORT bool StringToLineSearchInterpolationType(
    std::string value,
    LineSearchInterpolationType* type);

CERES_EXPORT const char* CovarianceAlgorithmTypeToString(
    CovarianceAlgorithmType type);
CERES_EXPORT bool StringToCovarianceAlgorithmType(
    std::string value,
    CovarianceAlgorithmType* type);

CERES_EXPORT const char* NumericDiffMethodTypeToString(
    NumericDiffMethodType type);
CERES_EXPORT bool StringToNumericDiffMethodType(
    std::string value,
    NumericDiffMethodType* type);

CERES_EXPORT const char* TerminationTypeToString(TerminationType type);

CERES_EXPORT bool IsSchurType(LinearSolverType type);
CERES_EXPORT bool IsSparseLinearAlgebraLibraryTypeAvailable(
    SparseLinearAlgebraLibraryType type);
CERES_EXPORT bool IsDenseLinearAlgebraLibraryTypeAvailable(
    DenseLinearAlgebraLibraryType type);

}  // namespace ceres

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_PUBLIC_TYPES_H_
