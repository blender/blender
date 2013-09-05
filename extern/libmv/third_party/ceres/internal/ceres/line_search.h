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
//
// Interface for and implementation of various Line search algorithms.

#ifndef CERES_INTERNAL_LINE_SEARCH_H_
#define CERES_INTERNAL_LINE_SEARCH_H_

#ifndef CERES_NO_LINE_SEARCH_MINIMIZER

#include <string>
#include <vector>
#include "ceres/internal/eigen.h"
#include "ceres/internal/port.h"
#include "ceres/types.h"

namespace ceres {
namespace internal {

class Evaluator;
struct FunctionSample;

// Line search is another name for a one dimensional optimization
// algorithm. The name "line search" comes from the fact one
// dimensional optimization problems that arise as subproblems of
// general multidimensional optimization problems.
//
// While finding the exact minimum of a one dimensionl function is
// hard, instances of LineSearch find a point that satisfies a
// sufficient decrease condition. Depending on the particular
// condition used, we get a variety of different line search
// algorithms, e.g., Armijo, Wolfe etc.
class LineSearch {
 public:
  class Function;

  struct Options {
    Options()
        : interpolation_type(CUBIC),
          sufficient_decrease(1e-4),
          max_step_contraction(1e-3),
          min_step_contraction(0.9),
          min_step_size(1e-9),
          max_num_iterations(20),
          sufficient_curvature_decrease(0.9),
          max_step_expansion(10.0),
          function(NULL) {}

    // Degree of the polynomial used to approximate the objective
    // function.
    LineSearchInterpolationType interpolation_type;

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
    double sufficient_decrease;

    // In each iteration of the Armijo / Wolfe line search,
    //
    // new_step_size >= max_step_contraction * step_size
    //
    // Note that by definition, for contraction:
    //
    //  0 < max_step_contraction < min_step_contraction < 1
    //
    double max_step_contraction;

    // In each iteration of the Armijo / Wolfe line search,
    //
    // new_step_size <= min_step_contraction * step_size
    // Note that by definition, for contraction:
    //
    //  0 < max_step_contraction < min_step_contraction < 1
    //
    double min_step_contraction;

    // If during the line search, the step_size falls below this
    // value, it is truncated to zero.
    double min_step_size;

    // Maximum number of trial step size iterations during each line search,
    // if a step size satisfying the search conditions cannot be found within
    // this number of trials, the line search will terminate.
    int max_num_iterations;

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
    double sufficient_curvature_decrease;

    // During the bracketing phase of the Wolfe search, the step size is
    // increased until either a point satisfying the Wolfe conditions is
    // found, or an upper bound for a bracket containing a point satisfying
    // the conditions is found.  Precisely, at each iteration of the
    // expansion:
    //
    //   new_step_size <= max_step_expansion * step_size.
    //
    // By definition for expansion, max_step_expansion > 1.0.
    double max_step_expansion;

    // The one dimensional function that the line search algorithm
    // minimizes.
    Function* function;
  };

  // An object used by the line search to access the function values
  // and gradient of the one dimensional function being optimized.
  //
  // In practice, this object will provide access to the objective
  // function value and the directional derivative of the underlying
  // optimization problem along a specific search direction.
  //
  // See LineSearchFunction for an example implementation.
  class Function {
   public:
    virtual ~Function() {}
    // Evaluate the line search objective
    //
    //   f(x) = p(position + x * direction)
    //
    // Where, p is the objective function of the general optimization
    // problem.
    //
    // g is the gradient f'(x) at x.
    //
    // f must not be null. The gradient is computed only if g is not null.
    virtual bool Evaluate(double x, double* f, double* g) = 0;
  };

  // Result of the line search.
  struct Summary {
    Summary()
        : success(false),
          optimal_step_size(0.0),
          num_function_evaluations(0),
          num_gradient_evaluations(0),
          num_iterations(0) {}

    bool success;
    double optimal_step_size;
    int num_function_evaluations;
    int num_gradient_evaluations;
    int num_iterations;
    string error;
  };

  explicit LineSearch(const LineSearch::Options& options);
  virtual ~LineSearch() {}

  static LineSearch* Create(const LineSearchType line_search_type,
                            const LineSearch::Options& options,
                            string* error);

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
  virtual void Search(double step_size_estimate,
                      double initial_cost,
                      double initial_gradient,
                      Summary* summary) = 0;
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
  LineSearch::Options options_;
};

class LineSearchFunction : public LineSearch::Function {
 public:
  explicit LineSearchFunction(Evaluator* evaluator);
  virtual ~LineSearchFunction() {}
  void Init(const Vector& position, const Vector& direction);
  virtual bool Evaluate(double x, double* f, double* g);
  double DirectionInfinityNorm() const;

 private:
  Evaluator* evaluator_;
  Vector position_;
  Vector direction_;

  // evaluation_point = Evaluator::Plus(position_,  x * direction_);
  Vector evaluation_point_;

  // scaled_direction = x * direction_;
  Vector scaled_direction_;
  Vector gradient_;
};

// Backtracking and interpolation based Armijo line search. This
// implementation is based on the Armijo line search that ships in the
// minFunc package by Mark Schmidt.
//
// For more details: http://www.di.ens.fr/~mschmidt/Software/minFunc.html
class ArmijoLineSearch : public LineSearch {
 public:
  explicit ArmijoLineSearch(const LineSearch::Options& options);
  virtual ~ArmijoLineSearch() {}
  virtual void Search(double step_size_estimate,
                      double initial_cost,
                      double initial_gradient,
                      Summary* summary);
};

// Bracketing / Zoom Strong Wolfe condition line search.  This implementation
// is based on the pseudo-code algorithm presented in Nocedal & Wright [1]
// (p60-61) with inspiration from the WolfeLineSearch which ships with the
// minFunc package by Mark Schmidt [2].
//
// [1] Nocedal J., Wright S., Numerical Optimization, 2nd Ed., Springer, 1999.
// [2] http://www.di.ens.fr/~mschmidt/Software/minFunc.html.
class WolfeLineSearch : public LineSearch {
 public:
  explicit WolfeLineSearch(const LineSearch::Options& options);
  virtual ~WolfeLineSearch() {}
  virtual void Search(double step_size_estimate,
                      double initial_cost,
                      double initial_gradient,
                      Summary* summary);
  // Returns true iff either a valid point, or valid bracket are found.
  bool BracketingPhase(const FunctionSample& initial_position,
                       const double step_size_estimate,
                       FunctionSample* bracket_low,
                       FunctionSample* bracket_high,
                       bool* perform_zoom_search,
                       Summary* summary);
  // Returns true iff final_line_sample satisfies strong Wolfe conditions.
  bool ZoomPhase(const FunctionSample& initial_position,
                 FunctionSample bracket_low,
                 FunctionSample bracket_high,
                 FunctionSample* solution,
                 Summary* summary);
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_NO_LINE_SEARCH_MINIMIZER
#endif  // CERES_INTERNAL_LINE_SEARCH_H_
