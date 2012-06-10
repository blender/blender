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
// Purpose: See .h file.

#include "ceres/loss_function.h"

#include <cmath>
#include <cstddef>

namespace ceres {

void TrivialLoss::Evaluate(double s, double rho[3]) const {
  rho[0] = s;
  rho[1] = 1;
  rho[2] = 0;
}

void HuberLoss::Evaluate(double s, double rho[3]) const {
  if (s > b_) {
    // Outlier region.
    // 'r' is always positive.
    const double r = sqrt(s);
    rho[0] = 2 * a_ * r - b_;
    rho[1] = a_ / r;
    rho[2] = - rho[1] / (2 * s);
  } else {
    // Inlier region.
    rho[0] = s;
    rho[1] = 1;
    rho[2] = 0;
  }
}

void SoftLOneLoss::Evaluate(double s, double rho[3]) const {
  const double sum = 1 + s * c_;
  const double tmp = sqrt(sum);
  // 'sum' and 'tmp' are always positive, assuming that 's' is.
  rho[0] = 2 * b_ * (tmp - 1);
  rho[1] = 1 / tmp;
  rho[2] = - (c_ * rho[1]) / (2 * sum);
}

void CauchyLoss::Evaluate(double s, double rho[3]) const {
  const double sum = 1 + s * c_;
  const double inv = 1 / sum;
  // 'sum' and 'inv' are always positive, assuming that 's' is.
  rho[0] = b_ * log(sum);
  rho[1] = inv;
  rho[2] = - c_ * (inv * inv);
}

void ScaledLoss::Evaluate(double s, double rho[3]) const {
  if (rho_.get() == NULL) {
    rho[0] = a_ * s;
    rho[1] = a_;
    rho[2] = 0.0;
  } else {
    rho_->Evaluate(s, rho);
    rho[0] *= a_;
    rho[1] *= a_;
    rho[2] *= a_;
  }
}

}  // namespace ceres
