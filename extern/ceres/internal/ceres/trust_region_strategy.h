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

#ifndef CERES_INTERNAL_TRUST_REGION_STRATEGY_H_
#define CERES_INTERNAL_TRUST_REGION_STRATEGY_H_

#include <memory>
#include <string>

#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/export.h"
#include "ceres/linear_solver.h"

namespace ceres::internal {

class LinearSolver;
class SparseMatrix;

// Interface for classes implementing various trust region strategies
// for nonlinear least squares problems.
//
// The object is expected to maintain and update a trust region
// radius, which it then uses to solve for the trust region step using
// the jacobian matrix and residual vector.
//
// Here the term trust region radius is used loosely, as the strategy
// is free to treat it as guidance and violate it as need be. e.g.,
// the LevenbergMarquardtStrategy uses the inverse of the trust region
// radius to scale the damping term, which controls the step size, but
// does not set a hard limit on its size.
class CERES_NO_EXPORT TrustRegionStrategy {
 public:
  struct Options {
    TrustRegionStrategyType trust_region_strategy_type = LEVENBERG_MARQUARDT;
    // Linear solver used for actually solving the trust region step.
    LinearSolver* linear_solver = nullptr;
    double initial_radius = 1e4;
    double max_radius = 1e32;

    // Minimum and maximum values of the diagonal damping matrix used
    // by LevenbergMarquardtStrategy. The DoglegStrategy also uses
    // these bounds to construct a regularizing diagonal to ensure
    // that the Gauss-Newton step computation is of full rank.
    double min_lm_diagonal = 1e-6;
    double max_lm_diagonal = 1e32;

    // Further specify which dogleg method to use
    DoglegType dogleg_type = TRADITIONAL_DOGLEG;

    ContextImpl* context = nullptr;
    int num_threads = 1;
  };

  // Factory.
  static std::unique_ptr<TrustRegionStrategy> Create(const Options& options);

  virtual ~TrustRegionStrategy();

  // Per solve options.
  struct PerSolveOptions {
    // Forcing sequence for inexact solves.
    double eta = 1e-1;

    DumpFormatType dump_format_type = TEXTFILE;

    // If non-empty and dump_format_type is not CONSOLE, the trust
    // regions strategy will write the linear system to file(s) with
    // name starting with dump_filename_base.  If dump_format_type is
    // CONSOLE then dump_filename_base will be ignored and the linear
    // system will be written to the standard error.
    std::string dump_filename_base;
  };

  struct Summary {
    // If the trust region problem is,
    //
    //   1/2 x'Ax + b'x + c,
    //
    // then
    //
    //   residual_norm = |Ax -b|
    double residual_norm = -1;

    // Number of iterations used by the linear solver. If a linear
    // solver was not called (e.g., DogLegStrategy after an
    // unsuccessful step), then this would be zero.
    int num_iterations = -1;

    // Status of the linear solver used to solve the Newton system.
    LinearSolverTerminationType termination_type =
        LinearSolverTerminationType::FAILURE;
  };

  // Use the current radius to solve for the trust region step.
  virtual Summary ComputeStep(const PerSolveOptions& per_solve_options,
                              SparseMatrix* jacobian,
                              const double* residuals,
                              double* step) = 0;

  // Inform the strategy that the current step has been accepted, and
  // that the ratio of the decrease in the non-linear objective to the
  // decrease in the trust region model is step_quality.
  virtual void StepAccepted(double step_quality) = 0;

  // Inform the strategy that the current step has been rejected, and
  // that the ratio of the decrease in the non-linear objective to the
  // decrease in the trust region model is step_quality.
  virtual void StepRejected(double step_quality) = 0;

  // Inform the strategy that the current step has been rejected
  // because it was found to be numerically invalid.
  // StepRejected/StepAccepted will not be called for this step, and
  // the strategy is free to do what it wants with this information.
  virtual void StepIsInvalid() = 0;

  // Current trust region radius.
  virtual double Radius() const = 0;
};

}  // namespace ceres::internal

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_INTERNAL_TRUST_REGION_STRATEGY_H_
