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

#include "ceres/dense_qr_solver.h"

#include <cstddef>

#include "Eigen/Dense"
#include "ceres/dense_qr.h"
#include "ceres/dense_sparse_matrix.h"
#include "ceres/internal/eigen.h"
#include "ceres/linear_solver.h"
#include "ceres/types.h"
#include "ceres/wall_time.h"

namespace ceres::internal {

DenseQRSolver::DenseQRSolver(const LinearSolver::Options& options)
    : options_(options), dense_qr_(DenseQR::Create(options)) {}

LinearSolver::Summary DenseQRSolver::SolveImpl(
    DenseSparseMatrix* A,
    const double* b,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double* x) {
  EventLogger event_logger("DenseQRSolver::Solve");

  const int num_rows = A->num_rows();
  const int num_cols = A->num_cols();
  const int num_augmented_rows =
      num_rows + ((per_solve_options.D != nullptr) ? num_cols : 0);

  if (lhs_.rows() != num_augmented_rows || lhs_.cols() != num_cols) {
    lhs_.resize(num_augmented_rows, num_cols);
    rhs_.resize(num_augmented_rows);
  }

  lhs_.topRows(num_rows) = A->matrix();
  rhs_.head(num_rows) = ConstVectorRef(b, num_rows);

  if (num_rows != num_augmented_rows) {
    lhs_.bottomRows(num_cols) =
        ConstVectorRef(per_solve_options.D, num_cols).asDiagonal();
    rhs_.tail(num_cols).setZero();
  }

  LinearSolver::Summary summary;
  summary.termination_type = dense_qr_->FactorAndSolve(
      lhs_.rows(), lhs_.cols(), lhs_.data(), rhs_.data(), x, &summary.message);
  summary.num_iterations = 1;
  event_logger.AddEvent("Solve");

  return summary;
}

}  // namespace ceres::internal
