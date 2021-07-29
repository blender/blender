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

#include <algorithm>
#include "ceres/trust_region_step_evaluator.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

TrustRegionStepEvaluator::TrustRegionStepEvaluator(
    const double initial_cost,
    const int max_consecutive_nonmonotonic_steps)
    : max_consecutive_nonmonotonic_steps_(max_consecutive_nonmonotonic_steps),
      minimum_cost_(initial_cost),
      current_cost_(initial_cost),
      reference_cost_(initial_cost),
      candidate_cost_(initial_cost),
      accumulated_reference_model_cost_change_(0.0),
      accumulated_candidate_model_cost_change_(0.0),
      num_consecutive_nonmonotonic_steps_(0){
}

double TrustRegionStepEvaluator::StepQuality(
    const double cost,
    const double model_cost_change) const {
  const double relative_decrease = (current_cost_ - cost) / model_cost_change;
  const double historical_relative_decrease =
      (reference_cost_ - cost) /
      (accumulated_reference_model_cost_change_ + model_cost_change);
  return std::max(relative_decrease, historical_relative_decrease);
}

void TrustRegionStepEvaluator::StepAccepted(
    const double cost,
    const double model_cost_change) {
  // Algorithm 10.1.2 from Trust Region Methods by Conn, Gould &
  // Toint.
  //
  // Step 3a
  current_cost_ = cost;
  accumulated_candidate_model_cost_change_ += model_cost_change;
  accumulated_reference_model_cost_change_ += model_cost_change;

  // Step 3b.
  if (current_cost_ < minimum_cost_) {
    minimum_cost_ = current_cost_;
    num_consecutive_nonmonotonic_steps_ = 0;
    candidate_cost_ = current_cost_;
    accumulated_candidate_model_cost_change_ = 0.0;
  } else {
    // Step 3c.
    ++num_consecutive_nonmonotonic_steps_;
    if (current_cost_ > candidate_cost_) {
      candidate_cost_ = current_cost_;
      accumulated_candidate_model_cost_change_ = 0.0;
    }
  }

  // Step 3d.
  //
  // At this point we have made too many non-monotonic steps and
  // we are going to reset the value of the reference iterate so
  // as to force the algorithm to descend.
  //
  // Note: In the original algorithm by Toint, this step was only
  // executed if the step was non-monotonic, but that would not handle
  // the case of max_consecutive_nonmonotonic_steps = 0. The small
  // modification of doing this always handles that corner case
  // correctly.
  if (num_consecutive_nonmonotonic_steps_ ==
      max_consecutive_nonmonotonic_steps_) {
    reference_cost_ = candidate_cost_;
    accumulated_reference_model_cost_change_ =
        accumulated_candidate_model_cost_change_;
  }
}

}  // namespace internal
}  // namespace ceres
