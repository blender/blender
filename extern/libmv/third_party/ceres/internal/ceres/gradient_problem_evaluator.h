// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2013 Google Inc. All rights reserved.
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
  virtual SparseMatrix* CreateJacobian() const { return NULL; }
  virtual bool Evaluate(const EvaluateOptions& evaluate_options,
                        const double* state,
                        double* cost,
                        double* residuals,
                        double* gradient,
                        SparseMatrix* jacobian) {
    CHECK(jacobian == NULL);
    ScopedExecutionTimer total_timer("Evaluator::Total", &execution_summary_);
    ScopedExecutionTimer call_type_timer(
        gradient == NULL ? "Evaluator::Cost" : "Evaluator::Gradient",
        &execution_summary_);
    return problem_.Evaluate(state, cost, gradient);
  }

  virtual bool Plus(const double* state,
                    const double* delta,
                    double* state_plus_delta) const {
    return problem_.Plus(state, delta, state_plus_delta);
  }

  virtual int NumParameters() const {
    return problem_.NumParameters();
  }

  virtual int NumEffectiveParameters()  const {
    return problem_.NumLocalParameters();
  }

  virtual int NumResiduals() const { return 1; }

  virtual map<string, int> CallStatistics() const {
    return execution_summary_.calls();
  }

  virtual map<string, double> TimeStatistics() const {
    return execution_summary_.times();
  }

 private:
  const GradientProblem& problem_;
  ::ceres::internal::ExecutionSummary execution_summary_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_GRADIENT_PROBLEM_EVALUATOR_H_
