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
// Authors: keir@google.com (Keir Mierle),
//          dgossow@google.com (David Gossow)

#ifndef CERES_INTERNAL_GRADIENT_CHECKING_COST_FUNCTION_H_
#define CERES_INTERNAL_GRADIENT_CHECKING_COST_FUNCTION_H_

#include <mutex>
#include <string>

#include "ceres/cost_function.h"
#include "ceres/iteration_callback.h"
#include "ceres/local_parameterization.h"

namespace ceres {
namespace internal {

class ProblemImpl;

// Callback that collects information about gradient checking errors, and
// will abort the solve as soon as an error occurs.
class GradientCheckingIterationCallback : public IterationCallback {
 public:
  GradientCheckingIterationCallback();

  // Will return SOLVER_CONTINUE until a gradient error has been detected,
  // then return SOLVER_ABORT.
  CallbackReturnType operator()(const IterationSummary& summary) final;

  // Notify this that a gradient error has occurred (thread safe).
  void SetGradientErrorDetected(std::string& error_log);

  // Retrieve error status (not thread safe).
  bool gradient_error_detected() const { return gradient_error_detected_; }
  const std::string& error_log() const { return error_log_; }
 private:
  bool gradient_error_detected_;
  std::string error_log_;
  std::mutex mutex_;
};

// Creates a CostFunction that checks the Jacobians that cost_function computes
// with finite differences. This API is only intended for unit tests that intend
// to  check the functionality of the GradientCheckingCostFunction
// implementation directly.
CostFunction* CreateGradientCheckingCostFunction(
    const CostFunction* cost_function,
    const std::vector<const LocalParameterization*>* local_parameterizations,
    double relative_step_size,
    double relative_precision,
    const std::string& extra_info,
    GradientCheckingIterationCallback* callback);

// Create a new ProblemImpl object from the input problem_impl, where all
// cost functions are wrapped so that each time their Evaluate method is called,
// an additional check is performed that compares the Jacobians computed by
// the original cost function with alternative Jacobians computed using
// numerical differentiation. If local parameterizations are given for any
// parameters, the Jacobians will be compared in the local space instead of the
// ambient space. For details on the gradient checking procedure, see the
// documentation of the GradientChecker class. If an error is detected in any
// iteration, the respective cost function will notify the
// GradientCheckingIterationCallback.
//
// The caller owns the returned ProblemImpl object.
//
// Note: This is quite inefficient and is intended only for debugging.
//
// relative_step_size and relative_precision are parameters to control
// the numeric differentiation and the relative tolerance between the
// jacobian computed by the CostFunctions in problem_impl and
// jacobians obtained by numerically differentiating them. See the
// documentation of 'numeric_derivative_relative_step_size' in solver.h for a
// better explanation.
ProblemImpl* CreateGradientCheckingProblemImpl(
    ProblemImpl* problem_impl,
    double relative_step_size,
    double relative_precision,
    GradientCheckingIterationCallback* callback);

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_GRADIENT_CHECKING_COST_FUNCTION_H_
