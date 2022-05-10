// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2015 Google Inc. All rights reserved.
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
//
// Purpose: See .h file.

#include "ceres/loss_function.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace ceres {

LossFunction::~LossFunction() = default;

void TrivialLoss::Evaluate(double s, double rho[3]) const {
  rho[0] = s;
  rho[1] = 1.0;
  rho[2] = 0.0;
}

void HuberLoss::Evaluate(double s, double rho[3]) const {
  if (s > b_) {
    // Outlier region.
    // 'r' is always positive.
    const double r = sqrt(s);
    rho[0] = 2.0 * a_ * r - b_;
    rho[1] = std::max(std::numeric_limits<double>::min(), a_ / r);
    rho[2] = -rho[1] / (2.0 * s);
  } else {
    // Inlier region.
    rho[0] = s;
    rho[1] = 1.0;
    rho[2] = 0.0;
  }
}

void SoftLOneLoss::Evaluate(double s, double rho[3]) const {
  const double sum = 1.0 + s * c_;
  const double tmp = sqrt(sum);
  // 'sum' and 'tmp' are always positive, assuming that 's' is.
  rho[0] = 2.0 * b_ * (tmp - 1.0);
  rho[1] = std::max(std::numeric_limits<double>::min(), 1.0 / tmp);
  rho[2] = -(c_ * rho[1]) / (2.0 * sum);
}

void CauchyLoss::Evaluate(double s, double rho[3]) const {
  const double sum = 1.0 + s * c_;
  const double inv = 1.0 / sum;
  // 'sum' and 'inv' are always positive, assuming that 's' is.
  rho[0] = b_ * log(sum);
  rho[1] = std::max(std::numeric_limits<double>::min(), inv);
  rho[2] = -c_ * (inv * inv);
}

void ArctanLoss::Evaluate(double s, double rho[3]) const {
  const double sum = 1 + s * s * b_;
  const double inv = 1 / sum;
  // 'sum' and 'inv' are always positive.
  rho[0] = a_ * atan2(s, a_);
  rho[1] = std::max(std::numeric_limits<double>::min(), inv);
  rho[2] = -2.0 * s * b_ * (inv * inv);
}

TolerantLoss::TolerantLoss(double a, double b)
    : a_(a), b_(b), c_(b * log(1.0 + exp(-a / b))) {
  CHECK_GE(a, 0.0);
  CHECK_GT(b, 0.0);
}

void TolerantLoss::Evaluate(double s, double rho[3]) const {
  const double x = (s - a_) / b_;
  // The basic equation is rho[0] = b ln(1 + e^x).  However, if e^x is too
  // large, it will overflow.  Since numerically 1 + e^x == e^x when the
  // x is greater than about ln(2^53) for doubles, beyond this threshold
  // we substitute x for ln(1 + e^x) as a numerically equivalent approximation.

  // ln(MathLimits<double>::kEpsilon).
  static constexpr double kLog2Pow53 = 36.7;
  if (x > kLog2Pow53) {
    rho[0] = s - a_ - c_;
    rho[1] = 1.0;
    rho[2] = 0.0;
  } else {
    const double e_x = exp(x);
    rho[0] = b_ * log(1.0 + e_x) - c_;
    rho[1] = std::max(std::numeric_limits<double>::min(), e_x / (1.0 + e_x));
    rho[2] = 0.5 / (b_ * (1.0 + cosh(x)));
  }
}

void TukeyLoss::Evaluate(double s, double* rho) const {
  if (s <= a_squared_) {
    // Inlier region.
    const double value = 1.0 - s / a_squared_;
    const double value_sq = value * value;
    rho[0] = a_squared_ / 3.0 * (1.0 - value_sq * value);
    rho[1] = value_sq;
    rho[2] = -2.0 / a_squared_ * value;
  } else {
    // Outlier region.
    rho[0] = a_squared_ / 3.0;
    rho[1] = 0.0;
    rho[2] = 0.0;
  }
}

ComposedLoss::ComposedLoss(const LossFunction* f,
                           Ownership ownership_f,
                           const LossFunction* g,
                           Ownership ownership_g)
    : f_(f), g_(g), ownership_f_(ownership_f), ownership_g_(ownership_g) {
  CHECK(f_ != nullptr);
  CHECK(g_ != nullptr);
}

ComposedLoss::~ComposedLoss() {
  if (ownership_f_ == DO_NOT_TAKE_OWNERSHIP) {
    f_.release();
  }
  if (ownership_g_ == DO_NOT_TAKE_OWNERSHIP) {
    g_.release();
  }
}

void ComposedLoss::Evaluate(double s, double rho[3]) const {
  double rho_f[3], rho_g[3];
  g_->Evaluate(s, rho_g);
  f_->Evaluate(rho_g[0], rho_f);
  rho[0] = rho_f[0];
  // f'(g(s)) * g'(s).
  rho[1] = rho_f[1] * rho_g[1];
  // f''(g(s)) * g'(s) * g'(s) + f'(g(s)) * g''(s).
  rho[2] = rho_f[2] * rho_g[1] * rho_g[1] + rho_f[1] * rho_g[2];
}

void ScaledLoss::Evaluate(double s, double rho[3]) const {
  if (rho_.get() == nullptr) {
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
