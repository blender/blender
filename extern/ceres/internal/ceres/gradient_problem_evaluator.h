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

#ifndef CERES_INTERNAL_GRADIENT_PROBLEM_EVALUATOR_H_
#define CERES_INTERNAL_GRADIENT_PROBLEM_EVALUATOR_H_

#include <map>
#include <string>

#include "ceres/evaluator.h"
#include "ceres/execution_summary.h"
#include "ceres/gradient_problem.h"
#include "ceres/internal/port.h"
#include "ceres/wall_time.h"

namespace ceres {
namespace internal {

class GradientProblemEvaluator : public Evaluator {
 public:
  explicit GradientProblemEvaluator(const GradientProblem& problem)
      : problem_(problem) {}
  virtual ~GradientProblemEvaluator() {}
  SparseMatrix* CreateJacobian() const final { return nullptr; }
  bool Evaluate(const EvaluateOptions& evaluate_options,
                const double* state,
                double* cost,
                double* residuals,
                double* gradient,
                SparseMatrix* jacobian) final {
    CHECK(jacobian == NULL);
    ScopedExecutionTimer total_timer("Evaluator::Total", &execution_summary_);
    // The reason we use Residual and Jacobian here even when we are
    // only computing the cost and gradient has to do with the fact
    // that the line search minimizer code is used by both the
    // GradientProblemSolver and the main CeresSolver coder where the
    // Evaluator evaluates the Jacobian, and these magic strings need
    // to be consistent across the code base for the time accounting
    // to work.
    ScopedExecutionTimer call_type_timer(
        gradient == NULL ? "Evaluator::Residual" : "Evaluator::Jacobian",
        &execution_summary_);
    return problem_.Evaluate(state, cost, gradient);
  }

  bool Plus(const double* state,
            const double* delta,
            double* state_plus_delta) const final {
    return problem_.Plus(state, delta, state_plus_delta);
  }

  int NumParameters() const final {
    return problem_.NumParameters();
  }

  int NumEffectiveParameters() const final {
    return problem_.NumLocalParameters();
  }

  int NumResiduals() const final { return 1; }

  std::map<std::string, internal::CallStatistics> Statistics() const final {
    return execution_summary_.statistics();
  }

 private:
  const GradientProblem& problem_;
  ::ceres::internal::ExecutionSummary execution_summary_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_GRADIENT_PROBLEM_EVALUATOR_H_
