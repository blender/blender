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

#include <iomanip>
#include <iostream>  // NOLINT

#include "ceres/line_search.h"

#include "ceres/fpclassify.h"
#include "ceres/evaluator.h"
#include "ceres/internal/eigen.h"
#include "ceres/polynomial.h"
#include "ceres/stringprintf.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {
namespace {
// Precision used for floating point values in error message output.
const int kErrorMessageNumericPrecision = 8;

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


std::ostream& operator<<(std::ostream &os, const FunctionSample& sample);

// Convenience stream operator for pushing FunctionSamples into log messages.
std::ostream& operator<<(std::ostream &os, const FunctionSample& sample) {
  os << sample.ToDebugString();
  return os;
}

LineSearch::LineSearch(const LineSearch::Options& options)
    : options_(options) {}

LineSearch* LineSearch::Create(const LineSearchType line_search_type,
                               const LineSearch::Options& options,
                               string* error) {
  LineSearch* line_search = NULL;
  switch (line_search_type) {
  case ceres::ARMIJO:
    line_search = new ArmijoLineSearch(options);
    break;
  case ceres::WOLFE:
    line_search = new WolfeLineSearch(options);
    break;
  default:
    *error = string("Invalid line search algorithm type: ") +
        LineSearchTypeToString(line_search_type) +
        string(", unable to create line search.");
    return NULL;
  }
  return line_search;
}

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

bool LineSearchFunction::Evaluate(double x, double* f, double* g) {
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

double LineSearchFunction::DirectionInfinityNorm() const {
  return direction_.lpNorm<Eigen::Infinity>();
}

// Returns step_size \in [min_step_size, max_step_size] which minimizes the
// polynomial of degree defined by interpolation_type which interpolates all
// of the provided samples with valid values.
double LineSearch::InterpolatingPolynomialMinimizingStepSize(
    const LineSearchInterpolationType& interpolation_type,
    const FunctionSample& lowerbound,
    const FunctionSample& previous,
    const FunctionSample& current,
    const double min_step_size,
    const double max_step_size) const {
  if (!current.value_is_valid ||
      (interpolation_type == BISECTION &&
       max_step_size <= current.x)) {
    // Either: sample is invalid; or we are using BISECTION and contracting
    // the step size.
    return min(max(current.x * 0.5, min_step_size), max_step_size);
  } else if (interpolation_type == BISECTION) {
    CHECK_GT(max_step_size, current.x);
    // We are expanding the search (during a Wolfe bracketing phase) using
    // BISECTION interpolation.  Using BISECTION when trying to expand is
    // strictly speaking an oxymoron, but we define this to mean always taking
    // the maximum step size so that the Armijo & Wolfe implementations are
    // agnostic to the interpolation type.
    return max_step_size;
  }
  // Only check if lower-bound is valid here, where it is required
  // to avoid replicating current.value_is_valid == false
  // behaviour in WolfeLineSearch.
  CHECK(lowerbound.value_is_valid)
      << std::scientific << std::setprecision(kErrorMessageNumericPrecision)
      << "Ceres bug: lower-bound sample for interpolation is invalid, "
      << "please contact the developers!, interpolation_type: "
      << LineSearchInterpolationTypeToString(interpolation_type)
      << ", lowerbound: " << lowerbound << ", previous: " << previous
      << ", current: " << current;

  // Select step size by interpolating the function and gradient values
  // and minimizing the corresponding polynomial.
  vector<FunctionSample> samples;
  samples.push_back(lowerbound);

  if (interpolation_type == QUADRATIC) {
    // Two point interpolation using function values and the
    // gradient at the lower bound.
    samples.push_back(ValueSample(current.x, current.value));

    if (previous.value_is_valid) {
      // Three point interpolation, using function values and the
      // gradient at the lower bound.
      samples.push_back(ValueSample(previous.x, previous.value));
    }
  } else if (interpolation_type == CUBIC) {
    // Two point interpolation using the function values and the gradients.
    samples.push_back(current);

    if (previous.value_is_valid) {
      // Three point interpolation using the function values and
      // the gradients.
      samples.push_back(previous);
    }
  } else {
    LOG(FATAL) << "Ceres bug: No handler for interpolation_type: "
               << LineSearchInterpolationTypeToString(interpolation_type)
               << ", please contact the developers!";
  }

  double step_size = 0.0, unused_min_value = 0.0;
  MinimizeInterpolatingPolynomial(samples, min_step_size, max_step_size,
                                  &step_size, &unused_min_value);
  return step_size;
}

ArmijoLineSearch::ArmijoLineSearch(const LineSearch::Options& options)
    : LineSearch(options) {}

void ArmijoLineSearch::Search(const double step_size_estimate,
                              const double initial_cost,
                              const double initial_gradient,
                              Summary* summary) {
  *CHECK_NOTNULL(summary) = LineSearch::Summary();
  CHECK_GE(step_size_estimate, 0.0);
  CHECK_GT(options().sufficient_decrease, 0.0);
  CHECK_LT(options().sufficient_decrease, 1.0);
  CHECK_GT(options().max_num_iterations, 0);
  Function* function = options().function;

  // Note initial_cost & initial_gradient are evaluated at step_size = 0,
  // not step_size_estimate, which is our starting guess.
  const FunctionSample initial_position =
      ValueAndGradientSample(0.0, initial_cost, initial_gradient);

  FunctionSample previous = ValueAndGradientSample(0.0, 0.0, 0.0);
  previous.value_is_valid = false;

  FunctionSample current = ValueAndGradientSample(step_size_estimate, 0.0, 0.0);
  current.value_is_valid = false;

  // As the Armijo line search algorithm always uses the initial point, for
  // which both the function value and derivative are known, when fitting a
  // minimizing polynomial, we can fit up to a quadratic without requiring the
  // gradient at the current query point.
  const bool interpolation_uses_gradient_at_current_sample =
      options().interpolation_type == CUBIC;
  const double descent_direction_max_norm =
      static_cast<const LineSearchFunction*>(function)->DirectionInfinityNorm();

  ++summary->num_function_evaluations;
  if (interpolation_uses_gradient_at_current_sample) {
    ++summary->num_gradient_evaluations;
  }
  current.value_is_valid =
      function->Evaluate(current.x,
                         &current.value,
                         interpolation_uses_gradient_at_current_sample
                         ? &current.gradient : NULL);
  current.gradient_is_valid =
      interpolation_uses_gradient_at_current_sample && current.value_is_valid;
  while (!current.value_is_valid ||
         current.value > (initial_cost
                          + options().sufficient_decrease
                          * initial_gradient
                          * current.x)) {
    // If current.value_is_valid is false, we treat it as if the cost at that
    // point is not large enough to satisfy the sufficient decrease condition.
    ++summary->num_iterations;
    if (summary->num_iterations >= options().max_num_iterations) {
      summary->error =
          StringPrintf("Line search failed: Armijo failed to find a point "
                       "satisfying the sufficient decrease condition within "
                       "specified max_num_iterations: %d.",
                       options().max_num_iterations);
      LOG_IF(WARNING, !options().is_silent) << summary->error;
      return;
    }

    const double step_size =
        this->InterpolatingPolynomialMinimizingStepSize(
            options().interpolation_type,
            initial_position,
            previous,
            current,
            (options().max_step_contraction * current.x),
            (options().min_step_contraction * current.x));

    if (step_size * descent_direction_max_norm < options().min_step_size) {
      summary->error =
          StringPrintf("Line search failed: step_size too small: %.5e "
                       "with descent_direction_max_norm: %.5e.", step_size,
                       descent_direction_max_norm);
      LOG_IF(WARNING, !options().is_silent) << summary->error;
      return;
    }

    previous = current;
    current.x = step_size;

    ++summary->num_function_evaluations;
    if (interpolation_uses_gradient_at_current_sample) {
      ++summary->num_gradient_evaluations;
    }
    current.value_is_valid =
      function->Evaluate(current.x,
                         &current.value,
                         interpolation_uses_gradient_at_current_sample
                         ? &current.gradient : NULL);
    current.gradient_is_valid =
        interpolation_uses_gradient_at_current_sample && current.value_is_valid;
  }

  summary->optimal_step_size = current.x;
  summary->success = true;
}

WolfeLineSearch::WolfeLineSearch(const LineSearch::Options& options)
    : LineSearch(options) {}

void WolfeLineSearch::Search(const double step_size_estimate,
                             const double initial_cost,
                             const double initial_gradient,
                             Summary* summary) {
  *CHECK_NOTNULL(summary) = LineSearch::Summary();
  // All parameters should have been validated by the Solver, but as
  // invalid values would produce crazy nonsense, hard check them here.
  CHECK_GE(step_size_estimate, 0.0);
  CHECK_GT(options().sufficient_decrease, 0.0);
  CHECK_GT(options().sufficient_curvature_decrease,
           options().sufficient_decrease);
  CHECK_LT(options().sufficient_curvature_decrease, 1.0);
  CHECK_GT(options().max_step_expansion, 1.0);

  // Note initial_cost & initial_gradient are evaluated at step_size = 0,
  // not step_size_estimate, which is our starting guess.
  const FunctionSample initial_position =
      ValueAndGradientSample(0.0, initial_cost, initial_gradient);

  bool do_zoom_search = false;
  // Important: The high/low in bracket_high & bracket_low refer to their
  // _function_ values, not their step sizes i.e. it is _not_ required that
  // bracket_low.x < bracket_high.x.
  FunctionSample solution, bracket_low, bracket_high;

  // Wolfe bracketing phase: Increases step_size until either it finds a point
  // that satisfies the (strong) Wolfe conditions, or an interval that brackets
  // step sizes which satisfy the conditions.  From Nocedal & Wright [1] p61 the
  // interval: (step_size_{k-1}, step_size_{k}) contains step lengths satisfying
  // the strong Wolfe conditions if one of the following conditions are met:
  //
  //   1. step_size_{k} violates the sufficient decrease (Armijo) condition.
  //   2. f(step_size_{k}) >= f(step_size_{k-1}).
  //   3. f'(step_size_{k}) >= 0.
  //
  // Caveat: If f(step_size_{k}) is invalid, then step_size is reduced, ignoring
  // this special case, step_size monotonically increases during bracketing.
  if (!this->BracketingPhase(initial_position,
                             step_size_estimate,
                             &bracket_low,
                             &bracket_high,
                             &do_zoom_search,
                             summary)) {
    // Failed to find either a valid point, a valid bracket satisfying the Wolfe
    // conditions, or even a step size > minimum tolerance satisfying the Armijo
    // condition.
    return;
  }

  if (!do_zoom_search) {
    // Either: Bracketing phase already found a point satisfying the strong
    // Wolfe conditions, thus no Zoom required.
    //
    // Or: Bracketing failed to find a valid bracket or a point satisfying the
    // strong Wolfe conditions within max_num_iterations, or whilst searching
    // shrank the bracket width until it was below our minimum tolerance.
    // As these are 'artificial' constraints, and we would otherwise fail to
    // produce a valid point when ArmijoLineSearch would succeed, we return the
    // point with the lowest cost found thus far which satsifies the Armijo
    // condition (but not the Wolfe conditions).
    summary->optimal_step_size = bracket_low.x;
    summary->success = true;
    return;
  }

  VLOG(3) << std::scientific << std::setprecision(kErrorMessageNumericPrecision)
          << "Starting line search zoom phase with bracket_low: "
          << bracket_low << ", bracket_high: " << bracket_high
          << ", bracket width: " << fabs(bracket_low.x - bracket_high.x)
          << ", bracket abs delta cost: "
          << fabs(bracket_low.value - bracket_high.value);

  // Wolfe Zoom phase: Called when the Bracketing phase finds an interval of
  // non-zero, finite width that should bracket step sizes which satisfy the
  // (strong) Wolfe conditions (before finding a step size that satisfies the
  // conditions).  Zoom successively decreases the size of the interval until a
  // step size which satisfies the Wolfe conditions is found.  The interval is
  // defined by bracket_low & bracket_high, which satisfy:
  //
  //   1. The interval bounded by step sizes: bracket_low.x & bracket_high.x
  //      contains step sizes that satsify the strong Wolfe conditions.
  //   2. bracket_low.x is of all the step sizes evaluated *which satisifed the
  //      Armijo sufficient decrease condition*, the one which generated the
  //      smallest function value, i.e. bracket_low.value <
  //      f(all other steps satisfying Armijo).
  //        - Note that this does _not_ (necessarily) mean that initially
  //          bracket_low.value < bracket_high.value (although this is typical)
  //          e.g. when bracket_low = initial_position, and bracket_high is the
  //          first sample, and which does not satisfy the Armijo condition,
  //          but still has bracket_high.value < initial_position.value.
  //   3. bracket_high is chosen after bracket_low, s.t.
  //      bracket_low.gradient * (bracket_high.x - bracket_low.x) < 0.
  if (!this->ZoomPhase(initial_position,
                       bracket_low,
                       bracket_high,
                       &solution,
                       summary) && !solution.value_is_valid) {
    // Failed to find a valid point (given the specified decrease parameters)
    // within the specified bracket.
    return;
  }
  // Ensure that if we ran out of iterations whilst zooming the bracket, or
  // shrank the bracket width to < tolerance and failed to find a point which
  // satisfies the strong Wolfe curvature condition, that we return the point
  // amongst those found thus far, which minimizes f() and satisfies the Armijo
  // condition.
  solution =
      solution.value_is_valid && solution.value <= bracket_low.value
      ? solution : bracket_low;

  summary->optimal_step_size = solution.x;
  summary->success = true;
}

// Returns true if either:
//
// A termination condition satisfying the (strong) Wolfe bracketing conditions
// is found:
//
// - A valid point, defined as a bracket of zero width [zoom not required].
// - A valid bracket (of width > tolerance), [zoom required].
//
// Or, searching was stopped due to an 'artificial' constraint, i.e. not
// a condition imposed / required by the underlying algorithm, but instead an
// engineering / implementation consideration. But a step which exceeds the
// minimum step size, and satsifies the Armijo condition was still found,
// and should thus be used [zoom not required].
//
// Returns false if no step size > minimum step size was found which
// satisfies at least the Armijo condition.
bool WolfeLineSearch::BracketingPhase(
    const FunctionSample& initial_position,
    const double step_size_estimate,
    FunctionSample* bracket_low,
    FunctionSample* bracket_high,
    bool* do_zoom_search,
    Summary* summary) {
  Function* function = options().function;

  FunctionSample previous = initial_position;
  FunctionSample current = ValueAndGradientSample(step_size_estimate, 0.0, 0.0);
  current.value_is_valid = false;

  const double descent_direction_max_norm =
      static_cast<const LineSearchFunction*>(function)->DirectionInfinityNorm();

  *do_zoom_search = false;
  *bracket_low = initial_position;

  // As we require the gradient to evaluate the Wolfe condition, we always
  // calculate it together with the value, irrespective of the interpolation
  // type.  As opposed to only calculating the gradient after the Armijo
  // condition is satisifed, as the computational saving from this approach
  // would be slight (perhaps even negative due to the extra call).  Also,
  // always calculating the value & gradient together protects against us
  // reporting invalid solutions if the cost function returns slightly different
  // function values when evaluated with / without gradients (due to numerical
  // issues).
  ++summary->num_function_evaluations;
  ++summary->num_gradient_evaluations;
  current.value_is_valid =
      function->Evaluate(current.x,
                         &current.value,
                         &current.gradient);
  current.gradient_is_valid = current.value_is_valid;

  while (true) {
    ++summary->num_iterations;

    if (current.value_is_valid &&
        (current.value > (initial_position.value
                          + options().sufficient_decrease
                          * initial_position.gradient
                          * current.x) ||
         (previous.value_is_valid && current.value > previous.value))) {
      // Bracket found: current step size violates Armijo sufficient decrease
      // condition, or has stepped past an inflection point of f() relative to
      // previous step size.
      *do_zoom_search = true;
      *bracket_low = previous;
      *bracket_high = current;
      VLOG(3) << std::scientific
              << std::setprecision(kErrorMessageNumericPrecision)
              << "Bracket found: current step (" << current.x
              << ") violates Armijo sufficient condition, or has passed an "
              << "inflection point of f() based on value.";
      break;
    }

    if (current.value_is_valid &&
        fabs(current.gradient) <=
        -options().sufficient_curvature_decrease * initial_position.gradient) {
      // Current step size satisfies the strong Wolfe conditions, and is thus a
      // valid termination point, therefore a Zoom not required.
      *bracket_low = current;
      *bracket_high = current;
      VLOG(3) << std::scientific
              << std::setprecision(kErrorMessageNumericPrecision)
              << "Bracketing phase found step size: " << current.x
              << ", satisfying strong Wolfe conditions, initial_position: "
              << initial_position << ", current: " << current;
      break;

    } else if (current.value_is_valid && current.gradient >= 0) {
      // Bracket found: current step size has stepped past an inflection point
      // of f(), but Armijo sufficient decrease is still satisfied and
      // f(current) is our best minimum thus far.  Remember step size
      // monotonically increases, thus previous_step_size < current_step_size
      // even though f(previous) > f(current).
      *do_zoom_search = true;
      // Note inverse ordering from first bracket case.
      *bracket_low = current;
      *bracket_high = previous;
      VLOG(3) << "Bracket found: current step (" << current.x
              << ") satisfies Armijo, but has gradient >= 0, thus have passed "
              << "an inflection point of f().";
      break;

    } else if (current.value_is_valid &&
               fabs(current.x - previous.x) * descent_direction_max_norm
               < options().min_step_size) {
      // We have shrunk the search bracket to a width less than our tolerance,
      // and still not found either a point satisfying the strong Wolfe
      // conditions, or a valid bracket containing such a point. Stop searching
      // and set bracket_low to the size size amongst all those tested which
      // minimizes f() and satisfies the Armijo condition.
      LOG_IF(WARNING, !options().is_silent)
          << "Line search failed: Wolfe bracketing phase shrank "
          << "bracket width: " << fabs(current.x - previous.x)
          <<  ", to < tolerance: " << options().min_step_size
          << ", with descent_direction_max_norm: "
          << descent_direction_max_norm << ", and failed to find "
          << "a point satisfying the strong Wolfe conditions or a "
          << "bracketing containing such a point. Accepting "
          << "point found satisfying Armijo condition only, to "
          << "allow continuation.";
      *bracket_low = current;
      break;

    } else if (summary->num_iterations >= options().max_num_iterations) {
      // Check num iterations bound here so that we always evaluate the
      // max_num_iterations-th iteration against all conditions, and
      // then perform no additional (unused) evaluations.
      summary->error =
          StringPrintf("Line search failed: Wolfe bracketing phase failed to "
                       "find a point satisfying strong Wolfe conditions, or a "
                       "bracket containing such a point within specified "
                       "max_num_iterations: %d", options().max_num_iterations);
      LOG_IF(WARNING, !options().is_silent) << summary->error;
      // Ensure that bracket_low is always set to the step size amongst all
      // those tested which minimizes f() and satisfies the Armijo condition
      // when we terminate due to the 'artificial' max_num_iterations condition.
      *bracket_low =
          current.value_is_valid && current.value < bracket_low->value
          ? current : *bracket_low;
      break;
    }
    // Either: f(current) is invalid; or, f(current) is valid, but does not
    // satisfy the strong Wolfe conditions itself, or the conditions for
    // being a boundary of a bracket.

    // If f(current) is valid, (but meets no criteria) expand the search by
    // increasing the step size.
    const double max_step_size =
        current.value_is_valid
        ? (current.x * options().max_step_expansion) : current.x;

    // We are performing 2-point interpolation only here, but the API of
    // InterpolatingPolynomialMinimizingStepSize() allows for up to
    // 3-point interpolation, so pad call with a sample with an invalid
    // value that will therefore be ignored.
    const FunctionSample unused_previous;
    DCHECK(!unused_previous.value_is_valid);
    // Contracts step size if f(current) is not valid.
    const double step_size =
        this->InterpolatingPolynomialMinimizingStepSize(
            options().interpolation_type,
            previous,
            unused_previous,
            current,
            previous.x,
            max_step_size);
    if (step_size * descent_direction_max_norm < options().min_step_size) {
      summary->error =
          StringPrintf("Line search failed: step_size too small: %.5e "
                       "with descent_direction_max_norm: %.5e", step_size,
                       descent_direction_max_norm);
      LOG_IF(WARNING, !options().is_silent) << summary->error;
      return false;
    }

    previous = current.value_is_valid ? current : previous;
    current.x = step_size;

    ++summary->num_function_evaluations;
    ++summary->num_gradient_evaluations;
    current.value_is_valid =
        function->Evaluate(current.x,
                           &current.value,
                           &current.gradient);
    current.gradient_is_valid = current.value_is_valid;
  }

  // Ensure that even if a valid bracket was found, we will only mark a zoom
  // as required if the bracket's width is greater than our minimum tolerance.
  if (*do_zoom_search &&
      fabs(bracket_high->x - bracket_low->x) * descent_direction_max_norm
      < options().min_step_size) {
    *do_zoom_search = false;
  }

  return true;
}

// Returns true iff solution satisfies the strong Wolfe conditions. Otherwise,
// on return false, if we stopped searching due to the 'artificial' condition of
// reaching max_num_iterations, solution is the step size amongst all those
// tested, which satisfied the Armijo decrease condition and minimized f().
bool WolfeLineSearch::ZoomPhase(const FunctionSample& initial_position,
                                FunctionSample bracket_low,
                                FunctionSample bracket_high,
                                FunctionSample* solution,
                                Summary* summary) {
  Function* function = options().function;

  CHECK(bracket_low.value_is_valid && bracket_low.gradient_is_valid)
      << std::scientific << std::setprecision(kErrorMessageNumericPrecision)
      << "Ceres bug: f_low input to Wolfe Zoom invalid, please contact "
      << "the developers!, initial_position: " << initial_position
      << ", bracket_low: " << bracket_low
      << ", bracket_high: "<< bracket_high;
  // We do not require bracket_high.gradient_is_valid as the gradient condition
  // for a valid bracket is only dependent upon bracket_low.gradient, and
  // in order to minimize jacobian evaluations, bracket_high.gradient may
  // not have been calculated (if bracket_high.value does not satisfy the
  // Armijo sufficient decrease condition and interpolation method does not
  // require it).
  //
  // We also do not require that: bracket_low.value < bracket_high.value,
  // although this is typical. This is to deal with the case when
  // bracket_low = initial_position, bracket_high is the first sample,
  // and bracket_high does not satisfy the Armijo condition, but still has
  // bracket_high.value < initial_position.value.
  CHECK(bracket_high.value_is_valid)
      << std::scientific << std::setprecision(kErrorMessageNumericPrecision)
      << "Ceres bug: f_high input to Wolfe Zoom invalid, please "
      << "contact the developers!, initial_position: " << initial_position
      << ", bracket_low: " << bracket_low
      << ", bracket_high: "<< bracket_high;

  if (bracket_low.gradient * (bracket_high.x - bracket_low.x) >= 0) {
    // The third condition for a valid initial bracket:
    //
    //   3. bracket_high is chosen after bracket_low, s.t.
    //      bracket_low.gradient * (bracket_high.x - bracket_low.x) < 0.
    //
    // is not satisfied.  As this can happen when the users' cost function
    // returns inconsistent gradient values relative to the function values,
    // we do not CHECK_LT(), but we do stop processing and return an invalid
    // value.
    summary->error =
        StringPrintf("Line search failed: Wolfe zoom phase passed a bracket "
                     "which does not satisfy: bracket_low.gradient * "
                     "(bracket_high.x - bracket_low.x) < 0 [%.8e !< 0] "
                     "with initial_position: %s, bracket_low: %s, bracket_high:"
                     " %s, the most likely cause of which is the cost function "
                     "returning inconsistent gradient & function values.",
                     bracket_low.gradient * (bracket_high.x - bracket_low.x),
                     initial_position.ToDebugString().c_str(),
                     bracket_low.ToDebugString().c_str(),
                     bracket_high.ToDebugString().c_str());
    LOG_IF(WARNING, !options().is_silent) << summary->error;
    solution->value_is_valid = false;
    return false;
  }

  const int num_bracketing_iterations = summary->num_iterations;
  const double descent_direction_max_norm =
      static_cast<const LineSearchFunction*>(function)->DirectionInfinityNorm();

  while (true) {
    // Set solution to bracket_low, as it is our best step size (smallest f())
    // found thus far and satisfies the Armijo condition, even though it does
    // not satisfy the Wolfe condition.
    *solution = bracket_low;
    if (summary->num_iterations >= options().max_num_iterations) {
      summary->error =
          StringPrintf("Line search failed: Wolfe zoom phase failed to "
                       "find a point satisfying strong Wolfe conditions "
                       "within specified max_num_iterations: %d, "
                       "(num iterations taken for bracketing: %d).",
                       options().max_num_iterations, num_bracketing_iterations);
      LOG_IF(WARNING, !options().is_silent) << summary->error;
      return false;
    }
    if (fabs(bracket_high.x - bracket_low.x) * descent_direction_max_norm
        < options().min_step_size) {
      // Bracket width has been reduced below tolerance, and no point satisfying
      // the strong Wolfe conditions has been found.
      summary->error =
          StringPrintf("Line search failed: Wolfe zoom bracket width: %.5e "
                       "too small with descent_direction_max_norm: %.5e.",
                       fabs(bracket_high.x - bracket_low.x),
                       descent_direction_max_norm);
      LOG_IF(WARNING, !options().is_silent) << summary->error;
      return false;
    }

    ++summary->num_iterations;
    // Polynomial interpolation requires inputs ordered according to step size,
    // not f(step size).
    const FunctionSample& lower_bound_step =
        bracket_low.x < bracket_high.x ? bracket_low : bracket_high;
    const FunctionSample& upper_bound_step =
        bracket_low.x < bracket_high.x ? bracket_high : bracket_low;
    // We are performing 2-point interpolation only here, but the API of
    // InterpolatingPolynomialMinimizingStepSize() allows for up to
    // 3-point interpolation, so pad call with a sample with an invalid
    // value that will therefore be ignored.
    const FunctionSample unused_previous;
    DCHECK(!unused_previous.value_is_valid);
    solution->x =
        this->InterpolatingPolynomialMinimizingStepSize(
            options().interpolation_type,
            lower_bound_step,
            unused_previous,
            upper_bound_step,
            lower_bound_step.x,
            upper_bound_step.x);
    // No check on magnitude of step size being too small here as it is
    // lower-bounded by the initial bracket start point, which was valid.
    //
    // As we require the gradient to evaluate the Wolfe condition, we always
    // calculate it together with the value, irrespective of the interpolation
    // type.  As opposed to only calculating the gradient after the Armijo
    // condition is satisifed, as the computational saving from this approach
    // would be slight (perhaps even negative due to the extra call).  Also,
    // always calculating the value & gradient together protects against us
    // reporting invalid solutions if the cost function returns slightly
    // different function values when evaluated with / without gradients (due
    // to numerical issues).
    ++summary->num_function_evaluations;
    ++summary->num_gradient_evaluations;
    solution->value_is_valid =
        function->Evaluate(solution->x,
                           &solution->value,
                           &solution->gradient);
    solution->gradient_is_valid = solution->value_is_valid;
    if (!solution->value_is_valid) {
      summary->error =
          StringPrintf("Line search failed: Wolfe Zoom phase found "
                       "step_size: %.5e, for which function is invalid, "
                       "between low_step: %.5e and high_step: %.5e "
                       "at which function is valid.",
                       solution->x, bracket_low.x, bracket_high.x);
      LOG_IF(WARNING, !options().is_silent) << summary->error;
      return false;
    }

    VLOG(3) << "Zoom iteration: "
            << summary->num_iterations - num_bracketing_iterations
            << ", bracket_low: " << bracket_low
            << ", bracket_high: " << bracket_high
            << ", minimizing solution: " << *solution;

    if ((solution->value > (initial_position.value
                            + options().sufficient_decrease
                            * initial_position.gradient
                            * solution->x)) ||
        (solution->value >= bracket_low.value)) {
      // Armijo sufficient decrease not satisfied, or not better
      // than current lowest sample, use as new upper bound.
      bracket_high = *solution;
      continue;
    }

    // Armijo sufficient decrease satisfied, check strong Wolfe condition.
    if (fabs(solution->gradient) <=
        -options().sufficient_curvature_decrease * initial_position.gradient) {
      // Found a valid termination point satisfying strong Wolfe conditions.
      VLOG(3) << std::scientific
              << std::setprecision(kErrorMessageNumericPrecision)
              << "Zoom phase found step size: " << solution->x
              << ", satisfying strong Wolfe conditions.";
      break;

    } else if (solution->gradient * (bracket_high.x - bracket_low.x) >= 0) {
      bracket_high = bracket_low;
    }

    bracket_low = *solution;
  }
  // Solution contains a valid point which satisfies the strong Wolfe
  // conditions.
  return true;
}

}  // namespace internal
}  // namespace ceres
