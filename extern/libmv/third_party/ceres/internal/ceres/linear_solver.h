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
// Abstract interface for objects solving linear systems of various
// kinds.

#ifndef CERES_INTERNAL_LINEAR_SOLVER_H_
#define CERES_INTERNAL_LINEAR_SOLVER_H_

#include <cstddef>

#include <glog/logging.h>
#include "ceres/block_sparse_matrix.h"
#include "ceres/casts.h"
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/dense_sparse_matrix.h"
#include "ceres/triplet_sparse_matrix.h"
#include "ceres/types.h"

namespace ceres {
namespace internal {

class LinearOperator;

// Abstract base class for objects that implement algorithms for
// solving linear systems
//
//   Ax = b
//
// It is expected that a single instance of a LinearSolver object
// maybe used multiple times for solving different linear
// systems. This allows them to cache and reuse information across
// solves if for example the sparsity of the linear system remains
// constant.
//
// Subclasses of LinearSolver use two structs to configure themselves.
// The Options struct configures the LinearSolver object for its
// lifetime. The PerSolveOptions struct is used to specify options for
// a particular Solve call.
class LinearSolver {
 public:
  struct Options {
    Options()
        : type(SPARSE_NORMAL_CHOLESKY),
          preconditioner_type(JACOBI),
          min_num_iterations(1),
          max_num_iterations(1),
          num_threads(1),
          constant_sparsity(false),
          num_eliminate_blocks(0),
          residual_reset_period(10),
          row_block_size(Dynamic),
          e_block_size(Dynamic),
          f_block_size(Dynamic) {
    }

    LinearSolverType type;

    PreconditionerType preconditioner_type;

    // Number of internal iterations that the solver uses. This
    // parameter only makes sense for iterative solvers like CG.
    int min_num_iterations;
    int max_num_iterations;

    // If possible, how many threads can the solver use.
    int num_threads;

    // If possible cache and reuse the symbolic factorization across
    // multiple calls.
    bool constant_sparsity;

    // Eliminate 0 to num_eliminate_blocks - 1 from the Normal
    // equations to form a schur complement. Only used by the Schur
    // complement based solver. The most common use for this parameter
    // is in the case of structure from motion problems where we have
    // camera blocks and point blocks. Then setting the
    // num_eliminate_blocks to the number of points allows the solver
    // to use the Schur complement trick. For more details see the
    // description of this parameter in solver.h.
    int num_eliminate_blocks;

    // Iterative solvers, e.g. Preconditioned Conjugate Gradients
    // maintain a cheap estimate of the residual which may become
    // inaccurate over time. Thus for non-zero values of this
    // parameter, the solver can be told to recalculate the value of
    // the residual using a |b - Ax| evaluation.
    int residual_reset_period;

    // If the block sizes in a BlockSparseMatrix are fixed, then in
    // some cases the Schur complement based solvers can detect and
    // specialize on them.
    //
    // It is expected that these parameters are set programmatically
    // rather than manually.
    //
    // Please see explicit_schur_complement_solver_impl.h for more
    // details.
    int row_block_size;
    int e_block_size;
    int f_block_size;
  };

  // Options for the Solve method.
  struct PerSolveOptions {
    PerSolveOptions()
        : D(NULL),
          preconditioner(NULL),
          r_tolerance(0.0),
          q_tolerance(0.0) {
    }

    // This option only makes sense for unsymmetric linear solvers
    // that can solve rectangular linear systems.
    //
    // Given a matrix A, an optional diagonal matrix D as a vector,
    // and a vector b, the linear solver will solve for
    //
    //   | A | x = | b |
    //   | D |     | 0 |
    //
    // If D is null, then it is treated as zero, and the solver returns
    // the solution to
    //
    //   A x = b
    //
    // In either case, x is the vector that solves the following
    // optimization problem.
    //
    //   arg min_x ||Ax - b||^2 + ||Dx||^2
    //
    // Here A is a matrix of size m x n, with full column rank. If A
    // does not have full column rank, the results returned by the
    // solver cannot be relied on. D, if it is not null is an array of
    // size n.  b is an array of size m and x is an array of size n.
    double * D;

    // This option only makes sense for iterative solvers.
    //
    // In general the performance of an iterative linear solver
    // depends on the condition number of the matrix A. For example
    // the convergence rate of the conjugate gradients algorithm
    // is proportional to the square root of the condition number.
    //
    // One particularly useful technique for improving the
    // conditioning of a linear system is to precondition it. In its
    // simplest form a preconditioner is a matrix M such that instead
    // of solving Ax = b, we solve the linear system AM^{-1} y = b
    // instead, where M is such that the condition number k(AM^{-1})
    // is smaller than the conditioner k(A). Given the solution to
    // this system, x = M^{-1} y. The iterative solver takes care of
    // the mechanics of solving the preconditioned system and
    // returning the corrected solution x. The user only needs to
    // supply a linear operator.
    //
    // A null preconditioner is equivalent to an identity matrix being
    // used a preconditioner.
    LinearOperator* preconditioner;


    // The following tolerance related options only makes sense for
    // iterative solvers. Direct solvers ignore them.

    // Solver terminates when
    //
    //   |Ax - b| <= r_tolerance * |b|.
    //
    // This is the most commonly used termination criterion for
    // iterative solvers.
    double r_tolerance;

    // For PSD matrices A, let
    //
    //   Q(x) = x'Ax - 2b'x
    //
    // be the cost of the quadratic function defined by A and b. Then,
    // the solver terminates at iteration i if
    //
    //   i * (Q(x_i) - Q(x_i-1)) / Q(x_i) < q_tolerance.
    //
    // This termination criterion is more useful when using CG to
    // solve the Newton step. This particular convergence test comes
    // from Stephen Nash's work on truncated Newton
    // methods. References:
    //
    //   1. Stephen G. Nash & Ariela Sofer, Assessing A Search
    //      Direction Within A Truncated Newton Method, Operation
    //      Research Letters 9(1990) 219-221.
    //
    //   2. Stephen G. Nash, A Survey of Truncated Newton Methods,
    //      Journal of Computational and Applied Mathematics,
    //      124(1-2), 45-59, 2000.
    //
    double q_tolerance;
  };

  // Summary of a call to the Solve method. We should move away from
  // the true/false method for determining solver success. We should
  // let the summary object do the talking.
  struct Summary {
    Summary()
        : residual_norm(0.0),
          num_iterations(-1),
          termination_type(FAILURE) {
    }

    double residual_norm;
    int num_iterations;
    LinearSolverTerminationType termination_type;
  };

  virtual ~LinearSolver();

  // Solve Ax = b.
  virtual Summary Solve(LinearOperator* A,
                        const double* b,
                        const PerSolveOptions& per_solve_options,
                        double* x) = 0;

  static LinearSolver* Create(const Options& options);
};

// This templated subclass of LinearSolver serves as a base class for
// other linear solvers that depend on the particular matrix layout of
// the underlying linear operator. For example some linear solvers
// need low level access to the TripletSparseMatrix implementing the
// LinearOperator interface. This class hides those implementation
// details behind a private virtual method, and has the Solve method
// perform the necessary upcasting.
template <typename MatrixType>
class TypedLinearSolver : public LinearSolver {
 public:
  virtual ~TypedLinearSolver() {}
  virtual LinearSolver::Summary Solve(
      LinearOperator* A,
      const double* b,
      const LinearSolver::PerSolveOptions& per_solve_options,
      double* x) {
    CHECK_NOTNULL(A);
    CHECK_NOTNULL(b);
    CHECK_NOTNULL(x);
    return SolveImpl(down_cast<MatrixType*>(A), b, per_solve_options, x);
  }

 private:
  virtual LinearSolver::Summary SolveImpl(
      MatrixType* A,
      const double* b,
      const LinearSolver::PerSolveOptions& per_solve_options,
      double* x) = 0;
};

// Linear solvers that depend on acccess to the low level structure of
// a SparseMatrix.
typedef TypedLinearSolver<BlockSparseMatrix>         BlockSparseMatrixSolver;          // NOLINT
typedef TypedLinearSolver<BlockSparseMatrixBase>     BlockSparseMatrixBaseSolver;      // NOLINT
typedef TypedLinearSolver<CompressedRowSparseMatrix> CompressedRowSparseMatrixSolver;  // NOLINT
typedef TypedLinearSolver<DenseSparseMatrix>         DenseSparseMatrixSolver;          // NOLINT
typedef TypedLinearSolver<TripletSparseMatrix>       TripletSparseMatrixSolver;        // NOLINT

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_LINEAR_SOLVER_H_
