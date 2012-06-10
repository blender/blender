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
// Enums and other top level class definitions.
//
// Note: internal/types.cc defines stringification routines for some
// of these enums. Please update those routines if you extend or
// remove enums from here.

#ifndef CERES_PUBLIC_TYPES_H_
#define CERES_PUBLIC_TYPES_H_

namespace ceres {

// Basic integer types. These typedefs are in the Ceres namespace to avoid
// conflicts with other packages having similar typedefs.
typedef short int16;
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

  // Solve the normal equations using a sparse cholesky solver; based
  // on CHOLMOD.
  SPARSE_NORMAL_CHOLESKY,

  // Solve the normal equations using a dense QR solver; based on
  // Eigen.
  DENSE_QR,

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

  // Block diagonal of the Schur complement. This preconditioner may
  // only be used with the ITERATIVE_SCHUR solver. Requires
  // SuiteSparse/CHOLMOD.
  SCHUR_JACOBI,

  // Visibility clustering based preconditioners.
  //
  // These preconditioners are well suited for Structure from Motion
  // problems, particularly problems arising from community photo
  // collections. These preconditioners use the visibility structure
  // of the scene to determine the sparsity structure of the
  // preconditioner. Requires SuiteSparse/CHOLMOD.
  CLUSTER_JACOBI,
  CLUSTER_TRIDIAGONAL
};

enum LinearSolverTerminationType {
  // Termination criterion was met. For factorization based solvers
  // the tolerance is assumed to be zero. Any user provided values are
  // ignored.
  TOLERANCE,

  // Solver ran for max_num_iterations and terminated before the
  // termination tolerance could be satified.
  MAX_ITERATIONS,

  // Solver is stuck and further iterations will not result in any
  // measurable progress.
  STAGNATION,

  // Solver failed. Solver was terminated due to numerical errors. The
  // exact cause of failure depends on the particular solver being
  // used.
  FAILURE
};

enum OrderingType {
  // The order in which the parameter blocks were defined.
  NATURAL,

  // Use the ordering specificed in the vector ordering.
  USER,

  // Automatically figure out the best ordering to use the schur
  // complement based solver.
  SCHUR
};

// Logging options
// The options get progressively noisier.
enum LoggingType {
  SILENT,
  PER_MINIMIZER_ITERATION
};

enum MinimizerType {
  LEVENBERG_MARQUARDT
};

enum SolverTerminationType {
  // The minimizer did not run at all; usually due to errors in the user's
  // Problem or the solver options.
  DID_NOT_RUN,

  // The solver ran for maximum number of iterations specified by the
  // user, but none of the convergence criterion specified by the user
  // were met.
  NO_CONVERGENCE,

  // Minimizer terminated because
  //  (new_cost - old_cost) < function_tolerance * old_cost;
  FUNCTION_TOLERANCE,

  // Minimizer terminated because
  // max_i |gradient_i| < gradient_tolerance * max_i|initial_gradient_i|
  GRADIENT_TOLERANCE,

  // Minimized terminated because
  //  |step|_2 <= parameter_tolerance * ( |x|_2 +  parameter_tolerance)
  PARAMETER_TOLERANCE,

  // The minimizer terminated because it encountered a numerical error
  // that it could not recover from.
  NUMERICAL_FAILURE,

  // Using an IterationCallback object, user code can control the
  // minimizer. The following enums indicate that the user code was
  // responsible for termination.

  // User's IterationCallback returned SOLVER_ABORT.
  USER_ABORT,

  // User's IterationCallback returned SOLVER_TERMINATE_SUCCESSFULLY
  USER_SUCCESS
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
  // pointed to by Solver::Options::lsqp_dump_directory as a protocol
  // buffer. linear_least_squares_problems.h/cc contains routines for
  // loading these problems. For details on the on disk format used,
  // see matrix.proto. The files are named lm_iteration_???.lsqp.
  PROTOBUF,

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

// For SizedCostFunction and AutoDiffCostFunction, DYNAMIC can be specified for
// the number of residuals. If specified, then the number of residuas for that
// cost function can vary at runtime.
enum DimensionType {
  DYNAMIC = -1
};

const char* LinearSolverTypeToString(LinearSolverType type);
const char* PreconditionerTypeToString(PreconditionerType type);
const char* LinearSolverTerminationTypeToString(
    LinearSolverTerminationType type);
const char* OrderingTypeToString(OrderingType type);
const char* SolverTerminationTypeToString(SolverTerminationType type);

bool IsSchurType(LinearSolverType type);

}  // namespace ceres

#endif  // CERES_PUBLIC_TYPES_H_
