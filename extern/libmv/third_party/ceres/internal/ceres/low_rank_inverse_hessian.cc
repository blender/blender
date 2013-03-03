// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2012 Google Inc. All rights reserved.
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

#include "ceres/internal/eigen.h"
#include "ceres/low_rank_inverse_hessian.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

LowRankInverseHessian::LowRankInverseHessian(int num_parameters,
                                             int max_num_corrections)
    : num_parameters_(num_parameters),
      max_num_corrections_(max_num_corrections),
      num_corrections_(0),
      diagonal_(1.0),
      delta_x_history_(num_parameters, max_num_corrections),
      delta_gradient_history_(num_parameters, max_num_corrections),
      delta_x_dot_delta_gradient_(max_num_corrections) {
}

bool LowRankInverseHessian::Update(const Vector& delta_x,
                                   const Vector& delta_gradient) {
  const double delta_x_dot_delta_gradient = delta_x.dot(delta_gradient);
  if (delta_x_dot_delta_gradient <= 1e-10) {
    VLOG(2) << "Skipping LBFGS Update. " << delta_x_dot_delta_gradient;
    return false;
  }

  if (num_corrections_ == max_num_corrections_) {
    // TODO(sameeragarwal): This can be done more efficiently using
    // a circular buffer/indexing scheme, but for simplicity we will
    // do the expensive copy for now.
    delta_x_history_.block(0, 0, num_parameters_, max_num_corrections_ - 2) =
        delta_x_history_
        .block(0, 1, num_parameters_, max_num_corrections_ - 1);

    delta_gradient_history_
        .block(0, 0, num_parameters_, max_num_corrections_ - 2) =
        delta_gradient_history_
        .block(0, 1, num_parameters_, max_num_corrections_ - 1);

    delta_x_dot_delta_gradient_.head(num_corrections_ - 2) =
        delta_x_dot_delta_gradient_.tail(num_corrections_ - 1);
  } else {
    ++num_corrections_;
  }

  delta_x_history_.col(num_corrections_ - 1) = delta_x;
  delta_gradient_history_.col(num_corrections_ - 1) = delta_gradient;
  delta_x_dot_delta_gradient_(num_corrections_ - 1) =
      delta_x_dot_delta_gradient;
  diagonal_ = delta_x_dot_delta_gradient / delta_gradient.squaredNorm();
  return true;
}

void LowRankInverseHessian::RightMultiply(const double* x_ptr,
                                          double* y_ptr) const {
  ConstVectorRef gradient(x_ptr, num_parameters_);
  VectorRef search_direction(y_ptr, num_parameters_);

  search_direction = gradient;

  Vector alpha(num_corrections_);

  for (int i = num_corrections_ - 1; i >= 0; --i) {
    alpha(i) = delta_x_history_.col(i).dot(search_direction) /
        delta_x_dot_delta_gradient_(i);
    search_direction -= alpha(i) * delta_gradient_history_.col(i);
  }

  search_direction *= diagonal_;

  for (int i = 0; i < num_corrections_; ++i) {
    const double beta = delta_gradient_history_.col(i).dot(search_direction) /
        delta_x_dot_delta_gradient_(i);
    search_direction += delta_x_history_.col(i) * (alpha(i) - beta);
  }
}

}  // namespace internal
}  // namespace ceres
