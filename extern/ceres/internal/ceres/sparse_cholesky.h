// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2017 Google Inc. All rights reserved.
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

#ifndef CERES_INTERNAL_SPARSE_CHOLESKY_H_
#define CERES_INTERNAL_SPARSE_CHOLESKY_H_

// This include must come before any #ifndef check on Ceres compile options.
#include "ceres/internal/port.h"

#include <memory>
#include "ceres/linear_solver.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

// An interface that abstracts away the internal details of various
// sparse linear algebra libraries and offers a simple API for solving
// symmetric positive definite linear systems using a sparse Cholesky
// factorization.
//
// Instances of SparseCholesky are expected to cache the symbolic
// factorization of the linear system. They do this on the first call
// to Factorize or FactorAndSolve. Subsequent calls to Factorize and
// FactorAndSolve are expected to have the same sparsity structure.
//
// Example usage:
//
//  std::unique_ptr<SparseCholesky>
//  sparse_cholesky(SparseCholesky::Create(SUITE_SPARSE, AMD));
//
//  CompressedRowSparseMatrix lhs = ...;
//  std::string message;
//  CHECK_EQ(sparse_cholesky->Factorize(&lhs, &message), LINEAR_SOLVER_SUCCESS);
//  Vector rhs = ...;
//  Vector solution = ...;
//  CHECK_EQ(sparse_cholesky->Solve(rhs.data(), solution.data(), &message),
//           LINEAR_SOLVER_SUCCESS);

class SparseCholesky {
 public:
  static std::unique_ptr<SparseCholesky> Create(
      const LinearSolver::Options& options);

  virtual ~SparseCholesky();

  // Due to the symmetry of the linear system, sparse linear algebra
  // libraries only use one half of the input matrix. Whether it is
  // the upper or the lower triangular part of the matrix depends on
  // the library and the re-ordering strategy being used. This
  // function tells the user the storage type expected of the input
  // matrix for the sparse linear algebra library and reordering
  // strategy used.
  virtual CompressedRowSparseMatrix::StorageType StorageType() const = 0;

  // Computes the numeric factorization of the given matrix.  If this
  // is the first call to Factorize, first the symbolic factorization
  // will be computed and cached and the numeric factorization will be
  // computed based on that.
  //
  // Subsequent calls to Factorize will use that symbolic
  // factorization assuming that the sparsity of the matrix has
  // remained constant.
  virtual LinearSolverTerminationType Factorize(
      CompressedRowSparseMatrix* lhs, std::string* message) = 0;

  // Computes the solution to the equation
  //
  // lhs * solution = rhs
  virtual LinearSolverTerminationType Solve(const double* rhs,
                                            double* solution,
                                            std::string* message) = 0;

  // Convenience method which combines a call to Factorize and
  // Solve. Solve is only called if Factorize returns
  // LINEAR_SOLVER_SUCCESS.
  virtual LinearSolverTerminationType FactorAndSolve(
      CompressedRowSparseMatrix* lhs,
      const double* rhs,
      double* solution,
      std::string* message);

};

class IterativeRefiner;

// Computes an initial solution using the given instance of
// SparseCholesky, and then refines it using the IterativeRefiner.
class RefinedSparseCholesky : public SparseCholesky {
 public:
  RefinedSparseCholesky(std::unique_ptr<SparseCholesky> sparse_cholesky,
                        std::unique_ptr<IterativeRefiner> iterative_refiner);
  virtual ~RefinedSparseCholesky();

  virtual CompressedRowSparseMatrix::StorageType StorageType() const;
  virtual LinearSolverTerminationType Factorize(
      CompressedRowSparseMatrix* lhs, std::string* message);
  virtual LinearSolverTerminationType Solve(const double* rhs,
                                            double* solution,
                                            std::string* message);

 private:
  std::unique_ptr<SparseCholesky> sparse_cholesky_;
  std::unique_ptr<IterativeRefiner> iterative_refiner_;
  CompressedRowSparseMatrix* lhs_ = nullptr;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_SPARSE_CHOLESKY_H_
