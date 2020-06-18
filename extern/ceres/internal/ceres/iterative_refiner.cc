// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2018 Google Inc. All rights reserved.
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

#include <string>
#include "ceres/iterative_refiner.h"

#include "Eigen/Core"
#include "ceres/sparse_cholesky.h"
#include "ceres/sparse_matrix.h"

namespace ceres {
namespace internal {

IterativeRefiner::IterativeRefiner(const int max_num_iterations)
    : max_num_iterations_(max_num_iterations) {}

IterativeRefiner::~IterativeRefiner() {}

void IterativeRefiner::Allocate(int num_cols) {
  residual_.resize(num_cols);
  correction_.resize(num_cols);
  lhs_x_solution_.resize(num_cols);
}

void IterativeRefiner::Refine(const SparseMatrix& lhs,
                              const double* rhs_ptr,
                              SparseCholesky* sparse_cholesky,
                              double* solution_ptr) {
  const int num_cols = lhs.num_cols();
  Allocate(num_cols);
  ConstVectorRef rhs(rhs_ptr, num_cols);
  VectorRef solution(solution_ptr, num_cols);
  for (int i = 0; i < max_num_iterations_; ++i) {
    // residual = rhs - lhs * solution
    lhs_x_solution_.setZero();
    lhs.RightMultiply(solution_ptr, lhs_x_solution_.data());
    residual_ = rhs - lhs_x_solution_;
    // solution += lhs^-1 residual
    std::string ignored_message;
    sparse_cholesky->Solve(
        residual_.data(), correction_.data(), &ignored_message);
    solution += correction_;
  }
};

}  // namespace internal
}  // namespace ceres
