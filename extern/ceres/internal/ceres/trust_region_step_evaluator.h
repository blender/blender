// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2016 Google Inc. All rights reserved.
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

#ifndef CERES_INTERNAL_TRUST_REGION_STEP_EVALUATOR_H_
#define CERES_INTERNAL_TRUST_REGION_STEP_EVALUATOR_H_

namespace ceres {
namespace internal {

// The job of the TrustRegionStepEvaluator is to evaluate the quality
// of a step, i.e., how the cost of a step compares with the reduction
// in the objective of the trust region problem.
//
// Classic trust region methods are descent methods, in that they only
// accept a point if it strictly reduces the value of the objective
// function. They do this by measuring the quality of a step as
//
//   cost_change / model_cost_change.
//
// Relaxing the monotonic descent requirement allows the algorithm to
// be more efficient in the long term at the cost of some local
// increase in the value of the objective function.
//
// This is because allowing for non-decreasing objective function
// values in a principled manner allows the algorithm to "jump over
// boulders" as the method is not restricted to move into narrow
// valleys while preserving its convergence properties.
//
// The parameter max_consecutive_nonmonotonic_steps controls the
// window size used by the step selection algorithm to accept
// non-monotonic steps. Setting this parameter to zero, recovers the
// classic monotonic descent algorithm.
//
// Based on algorithm 10.1.2 (page 357) of "Trust Region
// Methods" by Conn Gould & Toint, or equations 33-40 of
// "Non-monotone trust-region algorithms for nonlinear
// optimization subject to convex constraints" by Phil Toint,
// Mathematical Programming, 77, 1997.
//
// Example usage:
//
// TrustRegionStepEvaluator* step_evaluator = ...
//
// cost = ... // Compute the non-linear objective function value.
// model_cost_change = ... // Change in the value of the trust region objective.
// if (step_evaluator->StepQuality(cost, model_cost_change) > threshold) {
//   x = x + delta;
//   step_evaluator->StepAccepted(cost, model_cost_change);
// }
class TrustRegionStepEvaluator {
 public:
  // initial_cost is as the name implies the cost of the starting
  // state of the trust region minimizer.
  //
  // max_consecutive_nonmonotonic_steps controls the window size used
  // by the step selection algorithm to accept non-monotonic
  // steps. Setting this parameter to zero, recovers the classic
  // monotonic descent algorithm.
  TrustRegionStepEvaluator(double initial_cost,
                           int max_consecutive_nonmonotonic_steps);

  // Return the quality of the step given its cost and the decrease in
  // the cost of the model. model_cost_change has to be positive.
  double StepQuality(double cost, double model_cost_change) const;

  // Inform the step evaluator that a step with the given cost and
  // model_cost_change has been accepted by the trust region
  // minimizer.
  void StepAccepted(double cost, double model_cost_change);

 private:
  const int max_consecutive_nonmonotonic_steps_;
  // The minimum cost encountered up till now.
  double minimum_cost_;
  // The current cost of the trust region minimizer as informed by the
  // last call to StepAccepted.
  double current_cost_;
  double reference_cost_;
  double candidate_cost_;
  // Accumulated model cost since the last time the reference model
  // cost was updated, i.e., when a step with cost less than the
  // current known minimum cost is accepted.
  double accumulated_reference_model_cost_change_;
  // Accumulated model cost since the last time the candidate model
  // cost was updated, i.e., a non-monotonic step was taken with a
  // cost that was greater than the current candidate cost.
  double accumulated_candidate_model_cost_change_;
  // Number of steps taken since the last time minimum_cost was updated.
  int num_consecutive_nonmonotonic_steps_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_TRUST_REGION_STEP_EVALUATOR_H_
