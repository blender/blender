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

#include "ceres/corrector.h"

#include <cstddef>
#include <cmath>
#include "glog/logging.h"

namespace ceres {
namespace internal {

Corrector::Corrector(double sq_norm, const double rho[3]) {
  CHECK_GE(sq_norm, 0.0);
  CHECK_GT(rho[1], 0.0);
  sqrt_rho1_ = sqrt(rho[1]);

  // If sq_norm = 0.0, the correction becomes trivial, the residual
  // and the jacobian are scaled by the squareroot of the derivative
  // of rho. Handling this case explicitly avoids the divide by zero
  // error that would occur below.
  //
  // The case where rho'' < 0 also gets special handling. Technically
  // it shouldn't, and the computation of the scaling should proceed
  // as below, however we found in experiments that applying the
  // curvature correction when rho'' < 0, which is the case when we
  // are in the outlier region slows down the convergence of the
  // algorithm significantly.
  //
  // Thus, we have divided the action of the robustifier into two
  // parts. In the inliner region, we do the full second order
  // correction which re-wights the gradient of the function by the
  // square root of the derivative of rho, and the Gauss-Newton
  // Hessian gets both the scaling and the rank-1 curvature
  // correction. Normaly, alpha is upper bounded by one, but with this
  // change, alpha is bounded above by zero.
  //
  // Empirically we have observed that the full Triggs correction and
  // the clamped correction both start out as very good approximations
  // to the loss function when we are in the convex part of the
  // function, but as the function starts transitioning from convex to
  // concave, the Triggs approximation diverges more and more and
  // ultimately becomes linear. The clamped Triggs model however
  // remains quadratic.
  //
  // The reason why the Triggs approximation becomes so poor is
  // because the curvature correction that it applies to the gauss
  // newton hessian goes from being a full rank correction to a rank
  // deficient correction making the inversion of the Hessian fraught
  // with all sorts of misery and suffering.
  //
  // The clamped correction retains its quadratic nature and inverting it
  // is always well formed.
  if ((sq_norm == 0.0) || (rho[2] <= 0.0)) {
    residual_scaling_ = sqrt_rho1_;
    alpha_sq_norm_ = 0.0;
    return;
  }

  // Calculate the smaller of the two solutions to the equation
  //
  // 0.5 *  alpha^2 - alpha - rho'' / rho' *  z'z = 0.
  //
  // Start by calculating the discriminant D.
  const double D = 1.0 + 2.0 * sq_norm * rho[2] / rho[1];

  // Since both rho[1] and rho[2] are guaranteed to be positive at
  // this point, we know that D > 1.0.

  const double alpha = 1.0 - sqrt(D);

  // Calculate the constants needed by the correction routines.
  residual_scaling_ = sqrt_rho1_ / (1 - alpha);
  alpha_sq_norm_ = alpha / sq_norm;
}

void Corrector::CorrectResiduals(int num_rows, double* residuals) {
  DCHECK(residuals != NULL);
  // Equation 11 in BANS.
  for (int r = 0; r < num_rows; ++r) {
    residuals[r] *= residual_scaling_;
  }
}

void Corrector::CorrectJacobian(int num_rows,
                                int num_cols,
                                double* residuals,
                                double* jacobian) {
  DCHECK(residuals != NULL);
  DCHECK(jacobian != NULL);
  // Equation 11 in BANS.
  //
  //  J = sqrt(rho) * (J - alpha^2 r * r' J)
  //
  // In days gone by this loop used to be a single Eigen expression of
  // the form
  //
  //  J = sqrt_rho1_ * (J - alpha_sq_norm_ * r* (r.transpose() * J));
  //
  // Which turns out to about 17x slower on bal problems. The reason
  // is that Eigen is unable to figure out that this expression can be
  // evaluated columnwise and ends up creating a temporary.
  for (int c = 0; c < num_cols; ++c) {
    double r_transpose_j = 0.0;
    for (int r = 0; r < num_rows; ++r) {
      r_transpose_j += jacobian[r * num_cols + c] * residuals[r];
    }

    for (int r = 0; r < num_rows; ++r) {
      jacobian[r * num_cols + c] = sqrt_rho1_ *
          (jacobian[r * num_cols + c] -
           alpha_sq_norm_ * residuals[r] * r_transpose_j);
    }
  }
}

}  // namespace internal
}  // namespace ceres
