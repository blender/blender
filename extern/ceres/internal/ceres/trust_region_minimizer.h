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

#ifndef CERES_INTERNAL_TRUST_REGION_MINIMIZER_H_
#define CERES_INTERNAL_TRUST_REGION_MINIMIZER_H_

#include <memory>

#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/export.h"
#include "ceres/minimizer.h"
#include "ceres/solver.h"
#include "ceres/sparse_matrix.h"
#include "ceres/trust_region_step_evaluator.h"
#include "ceres/trust_region_strategy.h"
#include "ceres/types.h"

namespace ceres::internal {

// Generic trust region minimization algorithm.
//
// For example usage, see SolverImpl::Minimize.
class CERES_NO_EXPORT TrustRegionMinimizer final : public Minimizer {
 public:
  // This method is not thread safe.
  void Minimize(const Minimizer::Options& options,
                double* parameters,
                Solver::Summary* solver_summary) override;

 private:
  void Init(const Minimizer::Options& options,
            double* parameters,
            Solver::Summary* solver_summary);
  bool IterationZero();
  bool FinalizeIterationAndCheckIfMinimizerCanContinue();
  bool ComputeTrustRegionStep();

  bool EvaluateGradientAndJacobian(bool new_evaluation_point);
  void ComputeCandidatePointAndEvaluateCost();

  void DoLineSearch(const Vector& x,
                    const Vector& gradient,
                    const double cost,
                    Vector* delta);
  void DoInnerIterationsIfNeeded();

  bool ParameterToleranceReached();
  bool FunctionToleranceReached();
  bool GradientToleranceReached();
  bool MaxSolverTimeReached();
  bool MaxSolverIterationsReached();
  bool MinTrustRegionRadiusReached();

  bool IsStepSuccessful();
  bool HandleSuccessfulStep();
  bool HandleInvalidStep();

  Minimizer::Options options_;

  // These pointers are shortcuts to objects passed to the
  // TrustRegionMinimizer. The TrustRegionMinimizer does not own them.
  double* parameters_;
  Solver::Summary* solver_summary_;
  Evaluator* evaluator_;
  SparseMatrix* jacobian_;
  TrustRegionStrategy* strategy_;

  std::unique_ptr<TrustRegionStepEvaluator> step_evaluator_;

  bool is_not_silent_;
  bool inner_iterations_are_enabled_;
  bool inner_iterations_were_useful_;

  // Summary of the current iteration.
  IterationSummary iteration_summary_;

  // Dimensionality of the problem in the ambient space.
  int num_parameters_;
  // Dimensionality of the problem in the tangent space. This is the
  // number of columns in the Jacobian.
  int num_effective_parameters_;
  // Length of the residual vector, also the number of rows in the Jacobian.
  int num_residuals_;

  // Current point.
  Vector x_;
  // Residuals at x_;
  Vector residuals_;
  // Gradient at x_.
  Vector gradient_;
  // Solution computed by the inner iterations.
  Vector inner_iteration_x_;
  // model_residuals = J * trust_region_step
  Vector model_residuals_;
  Vector negative_gradient_;
  // projected_gradient_step = Plus(x, -gradient), an intermediate
  // quantity used to compute the projected gradient norm.
  Vector projected_gradient_step_;
  // The step computed by the trust region strategy. If Jacobi scaling
  // is enabled, this is a vector in the scaled space.
  Vector trust_region_step_;
  // The current proposal for how far the trust region algorithm
  // thinks we should move. In the most basic case, it is just the
  // trust_region_step_ with the Jacobi scaling undone. If bounds
  // constraints are present, then it is the result of the projected
  // line search.
  Vector delta_;
  // candidate_x  = Plus(x, delta)
  Vector candidate_x_;
  // Scaling vector to scale the columns of the Jacobian.
  Vector jacobian_scaling_;

  // Cost at x_.
  double x_cost_;
  // Minimum cost encountered up till now.
  double minimum_cost_;
  // How much did the trust region strategy reduce the cost of the
  // linearized Gauss-Newton model.
  double model_cost_change_;
  // Cost at candidate_x_.
  double candidate_cost_;

  // Time at which the minimizer was started.
  double start_time_in_secs_;
  // Time at which the current iteration was started.
  double iteration_start_time_in_secs_;
  // Number of consecutive steps where the minimizer loop computed a
  // numerically invalid step.
  int num_consecutive_invalid_steps_;
};

}  // namespace ceres::internal

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_INTERNAL_TRUST_REGION_MINIMIZER_H_
