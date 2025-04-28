// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2023 Google Inc. All rights reserved.
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

#ifndef CERES_INTERNAL_ITERATIVE_REFINER_H_
#define CERES_INTERNAL_ITERATIVE_REFINER_H_

// This include must come before any #ifndef check on Ceres compile options.
// clang-format off
#include "ceres/internal/config.h"
// clang-format on

#include "ceres/internal/eigen.h"
#include "ceres/internal/export.h"

namespace ceres::internal {

class DenseCholesky;
class SparseCholesky;
class SparseMatrix;

// Iterative refinement
// (https://en.wikipedia.org/wiki/Iterative_refinement) is the process
// of improving the solution to a linear system, by using the
// following iteration.
//
// r_i = b - Ax_i
// Ad_i = r_i
// x_{i+1} = x_i + d_i
//
// IterativeRefiner implements this process for Symmetric Positive
// Definite linear systems.
//
// The above iterative loop is run until max_num_iterations is reached.
class CERES_NO_EXPORT SparseIterativeRefiner {
 public:
  // max_num_iterations is the number of refinement iterations to
  // perform.
  explicit SparseIterativeRefiner(int max_num_iterations);

  // Needed for mocking.
  virtual ~SparseIterativeRefiner();

  // Given an initial estimate of the solution of lhs * x = rhs, use
  // max_num_iterations rounds of iterative refinement to improve it.
  //
  // cholesky is assumed to contain an already computed factorization (or
  // an approximation thereof) of lhs.
  //
  // solution is expected to contain a approximation to the solution
  // to lhs * x = rhs. It can be zero.
  //
  // This method is virtual to facilitate mocking.
  virtual void Refine(const SparseMatrix& lhs,
                      const double* rhs,
                      SparseCholesky* cholesky,
                      double* solution);

 private:
  void Allocate(int num_cols);

  int max_num_iterations_;
  Vector residual_;
  Vector correction_;
  Vector lhs_x_solution_;
};

class CERES_NO_EXPORT DenseIterativeRefiner {
 public:
  // max_num_iterations is the number of refinement iterations to
  // perform.
  explicit DenseIterativeRefiner(int max_num_iterations);

  // Needed for mocking.
  virtual ~DenseIterativeRefiner();

  // Given an initial estimate of the solution of lhs * x = rhs, use
  // max_num_iterations rounds of iterative refinement to improve it.
  //
  // cholesky is assumed to contain an already computed factorization (or
  // an approximation thereof) of lhs.
  //
  // solution is expected to contain a approximation to the solution
  // to lhs * x = rhs. It can be zero.
  //
  // This method is virtual to facilitate mocking.
  virtual void Refine(int num_cols,
                      const double* lhs,
                      const double* rhs,
                      DenseCholesky* cholesky,
                      double* solution);

 private:
  void Allocate(int num_cols);

  int max_num_iterations_;
  Vector residual_;
  Vector correction_;
};

}  // namespace ceres::internal

#endif  // CERES_INTERNAL_ITERATIVE_REFINER_H_
