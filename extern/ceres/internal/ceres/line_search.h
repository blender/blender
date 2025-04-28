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
//
// Interface for and implementation of various Line search algorithms.

#ifndef CERES_INTERNAL_LINE_SEARCH_H_
#define CERES_INTERNAL_LINE_SEARCH_H_

#include <memory>
#include <string>
#include <vector>

#include "ceres/function_sample.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/export.h"
#include "ceres/types.h"

namespace ceres::internal {

class Evaluator;
class LineSearchFunction;

// Line search is another name for a one dimensional optimization
// algorithm. The name "line search" comes from the fact one
// dimensional optimization problems that arise as subproblems of
// general multidimensional optimization problems.
//
// While finding the exact minimum of a one dimensional function is
// hard, instances of LineSearch find a point that satisfies a
// sufficient decrease condition. Depending on the particular
// condition used, we get a variety of different line search
// algorithms, e.g., Armijo, Wolfe etc.
class CERES_NO_EXPORT LineSearch {
 public:
  struct Summary;

  struct CERES_NO_EXPORT Options {
    // Degree of the polynomial used to approximate the objective
    // function.
    LineSearchInterpolationType interpolation_type = CUBIC;

    // Armijo and Wolfe line search parameters.

    // Solving the line search problem exactly is computationally
    // prohibitive. Fortunately, line search based optimization
    // algorithms can still guarantee convergence if instead of an
    // exact solution, the line search algorithm returns a solution
    // which decreases the value of the objective function
    // sufficiently. More precisely, we are looking for a step_size
    // s.t.
    //
    //  f(step_size) <= f(0) + sufficient_decrease * f'(0) * step_size
    double sufficient_decrease = 1e-4;

    // In each iteration of the Armijo / Wolfe line search,
    //
    // new_step_size >= max_step_contraction * step_size
    //
    // Note that by definition, for contraction:
    //
    //  0 < max_step_contraction < min_step_contraction < 1
    //
    double max_step_contraction = 1e-3;

    // In each iteration of the Armijo / Wolfe line search,
    //
    // new_step_size <= min_step_contraction * step_size
    // Note that by definition, for contraction:
    //
    //  0 < max_step_contraction < min_step_contraction < 1
    //
    double min_step_contraction = 0.9;

    // If during the line search, the step_size falls below this
    // value, it is truncated to zero.
    double min_step_size = 1e-9;

    // Maximum number of trial step size iterations during each line search,
    // if a step size satisfying the search conditions cannot be found within
    // this number of trials, the line search will terminate.
    int max_num_iterations = 20;

    // Wolfe-specific line search parameters.

    // The strong Wolfe conditions consist of the Armijo sufficient
    // decrease condition, and an additional requirement that the
    // step-size be chosen s.t. the _magnitude_ ('strong' Wolfe
    // conditions) of the gradient along the search direction
    // decreases sufficiently. Precisely, this second condition
    // is that we seek a step_size s.t.
    //
    //   |f'(step_size)| <= sufficient_curvature_decrease * |f'(0)|
    //
    // Where f() is the line search objective and f'() is the derivative
    // of f w.r.t step_size (d f / d step_size).
    double sufficient_curvature_decrease = 0.9;

    // During the bracketing phase of the Wolfe search, the step size is
    // increased until either a point satisfying the Wolfe conditions is
    // found, or an upper bound for a bracket containing a point satisfying
    // the conditions is found.  Precisely, at each iteration of the
    // expansion:
    //
    //   new_step_size <= max_step_expansion * step_size.
    //
    // By definition for expansion, max_step_expansion > 1.0.
    double max_step_expansion = 10;

    bool is_silent = false;

    // The one dimensional function that the line search algorithm
    // minimizes.
    LineSearchFunction* function = nullptr;
  };

  // Result of the line search.
  struct Summary {
    bool success = false;
    FunctionSample optimal_point;
    int num_function_evaluations = 0;
    int num_gradient_evaluations = 0;
    int num_iterations = 0;
    // Cumulative time spent evaluating the value of the cost function across
    // all iterations.
    double cost_evaluation_time_in_seconds = 0.0;
    // Cumulative time spent evaluating the gradient of the cost function across
    // all iterations.
    double gradient_evaluation_time_in_seconds = 0.0;
    // Cumulative time spent minimizing the interpolating polynomial to compute
    // the next candidate step size across all iterations.
    double polynomial_minimization_time_in_seconds = 0.0;
    double total_time_in_seconds = 0.0;
    std::string error;
  };

  explicit LineSearch(const LineSearch::Options& options);
  virtual ~LineSearch();

  static std::unique_ptr<LineSearch> Create(
      const LineSearchType line_search_type,
      const LineSearch::Options& options,
      std::string* error);

  // Perform the line search.
  //
  // step_size_estimate must be a positive number.
  //
  // initial_cost and initial_gradient are the values and gradient of
  // the function at zero.
  // summary must not be null and will contain the result of the line
  // search.
  //
  // Summary::success is true if a non-zero step size is found.
  void Search(double step_size_estimate,
              double initial_cost,
              double initial_gradient,
              Summary* summary) const;
  double InterpolatingPolynomialMinimizingStepSize(
      const LineSearchInterpolationType& interpolation_type,
      const FunctionSample& lowerbound_sample,
      const FunctionSample& previous_sample,
      const FunctionSample& current_sample,
      const double min_step_size,
      const double max_step_size) const;

 protected:
  const LineSearch::Options& options() const { return options_; }

 private:
  virtual void DoSearch(double step_size_estimate,
                        double initial_cost,
                        double initial_gradient,
                        Summary* summary) const = 0;

 private:
  LineSearch::Options options_;
};

// An object used by the line search to access the function values
// and gradient of the one dimensional function being optimized.
//
// In practice, this object provides access to the objective
// function value and the directional derivative of the underlying
// optimization problem along a specific search direction.
class CERES_NO_EXPORT LineSearchFunction {
 public:
  explicit LineSearchFunction(Evaluator* evaluator);
  void Init(const Vector& position, const Vector& direction);

  // Evaluate the line search objective
  //
  //   f(x) = p(position + x * direction)
  //
  // Where, p is the objective function of the general optimization
  // problem.
  //
  // evaluate_gradient controls whether the gradient will be evaluated
  // or not.
  //
  // On return output->*_is_valid indicate indicate whether the
  // corresponding fields have numerically valid values or not.
  void Evaluate(double x, bool evaluate_gradient, FunctionSample* output);

  double DirectionInfinityNorm() const;

  // Resets to now, the start point for the results from TimeStatistics().
  void ResetTimeStatistics();
  void TimeStatistics(double* cost_evaluation_time_in_seconds,
                      double* gradient_evaluation_time_in_seconds) const;
  const Vector& position() const { return position_; }
  const Vector& direction() const { return direction_; }

 private:
  Evaluator* evaluator_;
  Vector position_;
  Vector direction_;

  // scaled_direction = x * direction_;
  Vector scaled_direction_;

  // We may not exclusively own the evaluator (e.g. in the Trust Region
  // minimizer), hence we need to save the initial evaluation durations for the
  // value & gradient to accurately determine the duration of the evaluations
  // we invoked.  These are reset by a call to ResetTimeStatistics().
  double initial_evaluator_residual_time_in_seconds;
  double initial_evaluator_jacobian_time_in_seconds;
};

// Backtracking and interpolation based Armijo line search. This
// implementation is based on the Armijo line search that ships in the
// minFunc package by Mark Schmidt.
//
// For more details: http://www.di.ens.fr/~mschmidt/Software/minFunc.html
class CERES_NO_EXPORT ArmijoLineSearch final : public LineSearch {
 public:
  explicit ArmijoLineSearch(const LineSearch::Options& options);

 private:
  void DoSearch(double step_size_estimate,
                double initial_cost,
                double initial_gradient,
                Summary* summary) const final;
};

// Bracketing / Zoom Strong Wolfe condition line search.  This implementation
// is based on the pseudo-code algorithm presented in Nocedal & Wright [1]
// (p60-61) with inspiration from the WolfeLineSearch which ships with the
// minFunc package by Mark Schmidt [2].
//
// [1] Nocedal J., Wright S., Numerical Optimization, 2nd Ed., Springer, 1999.
// [2] http://www.di.ens.fr/~mschmidt/Software/minFunc.html.
class CERES_NO_EXPORT WolfeLineSearch final : public LineSearch {
 public:
  explicit WolfeLineSearch(const LineSearch::Options& options);

  // Returns true iff either a valid point, or valid bracket are found.
  bool BracketingPhase(const FunctionSample& initial_position,
                       const double step_size_estimate,
                       FunctionSample* bracket_low,
                       FunctionSample* bracket_high,
                       bool* perform_zoom_search,
                       Summary* summary) const;
  // Returns true iff final_line_sample satisfies strong Wolfe conditions.
  bool ZoomPhase(const FunctionSample& initial_position,
                 FunctionSample bracket_low,
                 FunctionSample bracket_high,
                 FunctionSample* solution,
                 Summary* summary) const;

 private:
  void DoSearch(double step_size_estimate,
                double initial_cost,
                double initial_gradient,
                Summary* summary) const final;
};

}  // namespace ceres::internal

#endif  // CERES_INTERNAL_LINE_SEARCH_H_
