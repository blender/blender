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

#include <glog/logging.h>
#include <vector>
#include "ceres/internal/eigen.h"
#include "ceres/internal/port.h"

namespace ceres {
namespace internal {

class Evaluator;

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
        : interpolation_degree(1),
          use_higher_degree_interpolation_when_possible(false),
          sufficient_decrease(1e-4),
          min_relative_step_size_change(1e-3),
          max_relative_step_size_change(0.6),
          step_size_threshold(1e-9),
          function(NULL) {}

    // TODO(sameeragarwal): Replace this with enums which are common
    // across various line searches.
    //
    // Degree of the polynomial used to approximate the objective
    // function. Valid values are {0, 1, 2}.
    //
    // For Armijo line search
    //
    // 0: Bisection based backtracking search.
    // 1: Quadratic interpolation.
    // 2: Cubic interpolation.
    int interpolation_degree;

    // Usually its possible to increase the degree of the
    // interpolation polynomial by storing and using an extra point.
    bool use_higher_degree_interpolation_when_possible;

    // Armijo line search parameters.

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

    // In each iteration of the Armijo line search,
    //
    // new_step_size >= min_relative_step_size_change * step_size
    double min_relative_step_size_change;

    // In each iteration of the Armijo line search,
    //
    // new_step_size <= max_relative_step_size_change * step_size
    double max_relative_step_size_change;

    // If during the line search, the step_size falls below this
    // value, it is truncated to zero.
    double step_size_threshold;

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
          num_evaluations(0) {}

    bool success;
    double optimal_step_size;
    int num_evaluations;
  };

  virtual ~LineSearch() {}

  // Perform the line search.
  //
  // initial_step_size must be a positive number.
  //
  // initial_cost and initial_gradient are the values and gradient of
  // the function at zero.
  // summary must not be null and will contain the result of the line
  // search.
  //
  // Summary::success is true if a non-zero step size is found.
  virtual void Search(const LineSearch::Options& options,
                      double initial_step_size,
                      double initial_cost,
                      double initial_gradient,
                      Summary* summary) = 0;
};

class LineSearchFunction : public LineSearch::Function {
 public:
  explicit LineSearchFunction(Evaluator* evaluator);
  virtual ~LineSearchFunction() {}
  void Init(const Vector& position, const Vector& direction);
  virtual bool Evaluate(const double x, double* f, double* g);

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
  virtual ~ArmijoLineSearch() {}
  virtual void Search(const LineSearch::Options& options,
                      double initial_step_size,
                      double initial_cost,
                      double initial_gradient,
                      Summary* summary);
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_LINE_SEARCH_H_
