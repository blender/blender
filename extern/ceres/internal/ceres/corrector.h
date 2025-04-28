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
//
// Class definition for the object that is responsible for applying a
// second order correction to the Gauss-Newton based on the ideas in
// BAMS by Triggs et al.

#ifndef CERES_INTERNAL_CORRECTOR_H_
#define CERES_INTERNAL_CORRECTOR_H_

#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/export.h"

namespace ceres::internal {

// Corrector is responsible for applying the second order correction
// to the residual and jacobian of a least squares problem based on a
// radial robust loss.
//
// The key idea here is to look at the expressions for the robustified
// gauss newton approximation and then take its square root to get the
// corresponding corrections to the residual and jacobian.  For the
// full expressions see Eq. 10 and 11 in BAMS by Triggs et al.
class CERES_NO_EXPORT Corrector {
 public:
  // The constructor takes the squared norm, the value, the first and
  // second derivatives of the LossFunction. It precalculates some of
  // the constants that are needed to apply the correction. The
  // correction constant alpha is constrained to be smaller than 1, if
  // it becomes larger than 1, then it will reverse the sign of the
  // residual and the correction. If alpha is equal to 1 will result
  // in a divide by zero error. Thus we constrain alpha to be upper
  // bounded by 1 - epsilon_.
  //
  // rho[1] needs to be positive. The constructor will crash if this
  // condition is not met.
  //
  // In practical use CorrectJacobian should always be called before
  // CorrectResidual, because the jacobian correction depends on the
  // value of the uncorrected residual values.
  explicit Corrector(double sq_norm, const double rho[3]);

  // residuals *= sqrt(rho[1]) / (1 - alpha)
  void CorrectResiduals(int num_rows, double* residuals);

  // jacobian = sqrt(rho[1]) * jacobian -
  // sqrt(rho[1]) * alpha / sq_norm * residuals residuals' * jacobian.
  //
  // The method assumes that the jacobian has row-major storage. It is
  // the caller's responsibility to ensure that the pointer to
  // jacobian is not null.
  void CorrectJacobian(int num_rows,
                       int num_cols,
                       double* residuals,
                       double* jacobian);

 private:
  double sqrt_rho1_;
  double residual_scaling_;
  double alpha_sq_norm_;
};
}  // namespace ceres::internal

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_INTERNAL_CORRECTOR_H_
