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
// Author: markshachkov@gmail.com (Mark Shachkov)

#include "ceres/power_series_expansion_preconditioner.h"

#include "ceres/eigen_vector_ops.h"
#include "ceres/parallel_vector_ops.h"
#include "ceres/preconditioner.h"

namespace ceres::internal {

PowerSeriesExpansionPreconditioner::PowerSeriesExpansionPreconditioner(
    const ImplicitSchurComplement* isc,
    const int max_num_spse_iterations,
    const double spse_tolerance,
    const Preconditioner::Options& options)
    : isc_(isc),
      max_num_spse_iterations_(max_num_spse_iterations),
      spse_tolerance_(spse_tolerance),
      options_(options) {}

PowerSeriesExpansionPreconditioner::~PowerSeriesExpansionPreconditioner() =
    default;

bool PowerSeriesExpansionPreconditioner::Update(const LinearOperator& /*A*/,
                                                const double* /*D*/) {
  return true;
}

void PowerSeriesExpansionPreconditioner::RightMultiplyAndAccumulate(
    const double* x, double* y) const {
  VectorRef yref(y, num_rows());
  Vector series_term(num_rows());
  Vector previous_series_term(num_rows());
  ParallelSetZero(options_.context, options_.num_threads, yref);
  isc_->block_diagonal_FtF_inverse()->RightMultiplyAndAccumulate(
      x, y, options_.context, options_.num_threads);
  ParallelAssign(
      options_.context, options_.num_threads, previous_series_term, yref);

  const double norm_threshold =
      spse_tolerance_ * Norm(yref, options_.context, options_.num_threads);

  for (int i = 1;; i++) {
    ParallelSetZero(options_.context, options_.num_threads, series_term);
    isc_->InversePowerSeriesOperatorRightMultiplyAccumulate(
        previous_series_term.data(), series_term.data());
    ParallelAssign(
        options_.context, options_.num_threads, yref, yref + series_term);
    if (i >= max_num_spse_iterations_ || series_term.norm() < norm_threshold) {
      break;
    }
    std::swap(previous_series_term, series_term);
  }
}

int PowerSeriesExpansionPreconditioner::num_rows() const {
  return isc_->num_rows();
}

}  // namespace ceres::internal
