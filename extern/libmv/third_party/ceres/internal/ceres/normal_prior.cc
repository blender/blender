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

#include "ceres/normal_prior.h"

#include <cstddef>
#include <vector>

#include <glog/logging.h>
#include "ceres/internal/eigen.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/types.h"

namespace ceres {

NormalPrior::NormalPrior(const Matrix& A, const Vector& b)
    : A_(A), b_(b) {
  CHECK_GT(b_.rows(), 0);
  CHECK_GT(A_.rows(), 0);
  CHECK_EQ(b_.rows(), A.cols());
  set_num_residuals(A_.rows());
  mutable_parameter_block_sizes()->push_back(b_.rows());
}

bool NormalPrior::Evaluate(double const* const* parameters,
                           double* residuals,
                           double** jacobians) const {
  ConstVectorRef p(parameters[0], parameter_block_sizes()[0]);
  VectorRef r(residuals, num_residuals());
  // The following line should read
  // r = A_ * (p - b_);
  // The extra eval is to get around a bug in the eigen library.
  r = A_ * (p - b_).eval();
  if ((jacobians != NULL) && (jacobians[0] != NULL)) {
    MatrixRef(jacobians[0], num_residuals(), parameter_block_sizes()[0]) = A_;
  }
  return true;
}

}  // namespace ceres
