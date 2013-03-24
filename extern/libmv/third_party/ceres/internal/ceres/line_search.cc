// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2012 Google Inc. All rights reserved.
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

#ifndef CERES_NO_LINE_SEARCH_MINIMIZER
#include "ceres/line_search.h"

#include <glog/logging.h>
#include "ceres/fpclassify.h"
#include "ceres/evaluator.h"
#include "ceres/internal/eigen.h"
#include "ceres/polynomial.h"


namespace ceres {
namespace internal {
namespace {

FunctionSample ValueSample(const double x, const double value) {
  FunctionSample sample;
  sample.x = x;
  sample.value = value;
  sample.value_is_valid = true;
  return sample;
};

FunctionSample ValueAndGradientSample(const double x,
                                      const double value,
                                      const double gradient) {
  FunctionSample sample;
  sample.x = x;
  sample.value = value;
  sample.gradient = gradient;
  sample.value_is_valid = true;
  sample.gradient_is_valid = true;
  return sample;
};

}  // namespace

LineSearchFunction::LineSearchFunction(Evaluator* evaluator)
    : evaluator_(evaluator),
      position_(evaluator->NumParameters()),
      direction_(evaluator->NumEffectiveParameters()),
      evaluation_point_(evaluator->NumParameters()),
      scaled_direction_(evaluator->NumEffectiveParameters()),
      gradient_(evaluator->NumEffectiveParameters()) {
}

void LineSearchFunction::Init(const Vector& position,
                              const Vector& direction) {
  position_ = position;
  direction_ = direction;
}

bool LineSearchFunction::Evaluate(const double x, double* f, double* g) {
  scaled_direction_ = x * direction_;
  if (!evaluator_->Plus(position_.data(),
                        scaled_direction_.data(),
                        evaluation_point_.data())) {
    return false;
  }

  if (g == NULL) {
    return (evaluator_->Evaluate(evaluation_point_.data(),
                                  f, NULL, NULL, NULL) &&
            IsFinite(*f));
  }

  if (!evaluator_->Evaluate(evaluation_point_.data(),
                            f,
                            NULL,
                            gradient_.data(), NULL)) {
    return false;
  }

  *g = direction_.dot(gradient_);
  return IsFinite(*f) && IsFinite(*g);
}

void ArmijoLineSearch::Search(const LineSearch::Options& options,
                              const double initial_step_size,
                              const double initial_cost,
                              const double initial_gradient,
                              Summary* summary) {
  *CHECK_NOTNULL(summary) = LineSearch::Summary();
  Function* function = options.function;

  double previous_step_size = 0.0;
  double previous_cost = 0.0;
  double previous_gradient = 0.0;
  bool previous_step_size_is_valid = false;

  double step_size = initial_step_size;
  double cost = 0.0;
  double gradient = 0.0;
  bool step_size_is_valid = false;

  ++summary->num_evaluations;
  step_size_is_valid =
      function->Evaluate(step_size,
                         &cost,
                         options.interpolation_degree < 2 ? NULL : &gradient);
  while (!step_size_is_valid || cost > (initial_cost
                                        + options.sufficient_decrease
                                        * initial_gradient
                                        * step_size)) {
    // If step_size_is_valid is not true we treat it as if the cost at
    // that point is not large enough to satisfy the sufficient
    // decrease condition.

    const double current_step_size = step_size;
    // Backtracking search. Each iteration of this loop finds a new point

    if ((options.interpolation_degree == 0) || !step_size_is_valid) {
      // Backtrack by halving the step_size;
      step_size *= 0.5;
    } else {
      // Backtrack by interpolating the function and gradient values
      // and minimizing the corresponding polynomial.

      vector<FunctionSample> samples;
      samples.push_back(ValueAndGradientSample(0.0,
                                               initial_cost,
                                               initial_gradient));

      if (options.interpolation_degree == 1) {
        // Two point interpolation using function values and the
        // initial gradient.
        samples.push_back(ValueSample(step_size, cost));

        if (options.use_higher_degree_interpolation_when_possible &&
            summary->num_evaluations > 1 &&
            previous_step_size_is_valid) {
          // Three point interpolation, using function values and the
          // initial gradient.
          samples.push_back(ValueSample(previous_step_size, previous_cost));
        }
      } else {
        // Two point interpolation using the function values and the gradients.
        samples.push_back(ValueAndGradientSample(step_size,
                                                 cost,
                                                 gradient));

        if (options.use_higher_degree_interpolation_when_possible &&
            summary->num_evaluations > 1 &&
            previous_step_size_is_valid) {
          // Three point interpolation using the function values and
          // the gradients.
          samples.push_back(ValueAndGradientSample(previous_step_size,
                                                   previous_cost,
                                                   previous_gradient));
        }
      }

      double min_value;
      MinimizeInterpolatingPolynomial(samples, 0.0, current_step_size,
                                      &step_size, &min_value);
      step_size =
          min(max(step_size,
                  options.min_relative_step_size_change * current_step_size),
              options.max_relative_step_size_change * current_step_size);
    }

    previous_step_size = current_step_size;
    previous_cost = cost;
    previous_gradient = gradient;

    if (fabs(initial_gradient) * step_size < options.step_size_threshold) {
      LOG(WARNING) << "Line search failed: step_size too small: " << step_size;
      return;
    }

    ++summary->num_evaluations;
    step_size_is_valid =
        function->Evaluate(step_size,
                           &cost,
                           options.interpolation_degree < 2 ? NULL : &gradient);
  }

  summary->optimal_step_size = step_size;
  summary->success = true;
}

}  // namespace internal
}  // namespace ceres

#endif  // CERES_NO_LINE_SEARCH_MINIMIZER
