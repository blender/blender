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

#ifndef CERES_INTERNAL_LEVENBERG_MARQUARDT_STRATEGY_H_
#define CERES_INTERNAL_LEVENBERG_MARQUARDT_STRATEGY_H_

#include "ceres/internal/eigen.h"
#include "ceres/trust_region_strategy.h"

namespace ceres {
namespace internal {

// Levenberg-Marquardt step computation and trust region sizing
// strategy based on on "Methods for Nonlinear Least Squares" by
// K. Madsen, H.B. Nielsen and O. Tingleff. Available to download from
//
// http://www2.imm.dtu.dk/pubdb/views/edoc_download.php/3215/pdf/imm3215.pdf
class LevenbergMarquardtStrategy : public TrustRegionStrategy {
 public:
  explicit LevenbergMarquardtStrategy(
      const TrustRegionStrategy::Options& options);
  virtual ~LevenbergMarquardtStrategy();

  // TrustRegionStrategy interface
  virtual TrustRegionStrategy::Summary ComputeStep(
      const TrustRegionStrategy::PerSolveOptions& per_solve_options,
      SparseMatrix* jacobian,
      const double* residuals,
      double* step);
  virtual void StepAccepted(double step_quality);
  virtual void StepRejected(double step_quality);
  virtual void StepIsInvalid() {
    // Treat the current step as a rejected step with no increase in
    // solution quality. Since rejected steps lead to decrease in the
    // size of the trust region, the next time ComputeStep is called,
    // this will lead to a better conditioned system.
    StepRejected(0.0);
  }

  virtual double Radius() const;

 private:
  LinearSolver* linear_solver_;
  double radius_;
  double max_radius_;
  const double min_diagonal_;
  const double max_diagonal_;
  double decrease_factor_;
  bool reuse_diagonal_;
  Vector diagonal_;   // diagonal_ =  diag(J'J)
  // Scaled copy of diagonal_. Stored here as optimization to prevent
  // allocations in every iteration and reuse when a step fails and
  // ComputeStep is called again.
  Vector lm_diagonal_;  // lm_diagonal_ = diagonal_ / radius_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_LEVENBERG_MARQUARDT_STRATEGY_H_
