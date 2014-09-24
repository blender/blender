// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2014 Google Inc. All rights reserved.
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
// Author: sameragarwal@google.com (Sameer Agarwal)

#ifndef CERES_INTERNAL_PREPROCESSOR_H_
#define CERES_INTERNAL_PREPROCESSOR_H_

#include "ceres/coordinate_descent_minimizer.h"
#include "ceres/evaluator.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/port.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/iteration_callback.h"
#include "ceres/linear_solver.h"
#include "ceres/minimizer.h"
#include "ceres/problem_impl.h"
#include "ceres/program.h"
#include "ceres/solver.h"

namespace ceres {
namespace internal {

struct PreprocessedProblem;

// Given a Problem object and a Solver::Options object indicating the
// configuration of the solver, the job of the Preprocessor is to
// analyze the Problem and perform the setup needed to solve it using
// the desired Minimization algorithm. The setup involves removing
// redundancies in the input problem (inactive parameter and residual
// blocks), finding fill reducing orderings as needed, configuring and
// creating various objects needed by the Minimizer to solve the
// problem such as an evaluator, a linear solver etc.
//
// Each Minimizer (LineSearchMinimizer and TrustRegionMinimizer) comes
// with a corresponding Preprocessor (LineSearchPreprocessor and
// TrustRegionPreprocessor) that knows about its needs and performs
// the preprocessing needed.
//
// The output of the Preprocessor is stored in a PreprocessedProblem
// object.
class Preprocessor {
public:
  // Factory.
  static Preprocessor* Create(MinimizerType minimizer_type);
  virtual ~Preprocessor();
  virtual bool Preprocess(const Solver::Options& options,
                          ProblemImpl* problem,
                          PreprocessedProblem* pp) = 0;
};

// A PreprocessedProblem is the result of running the Preprocessor on
// a Problem and Solver::Options object.
struct PreprocessedProblem {
  PreprocessedProblem()
      : fixed_cost(0.0) {
  }

  string error;
  Solver::Options options;
  LinearSolver::Options linear_solver_options;
  Evaluator::Options evaluator_options;
  Minimizer::Options minimizer_options;

  ProblemImpl* problem;
  scoped_ptr<ProblemImpl> gradient_checking_problem;
  scoped_ptr<Program> reduced_program;
  scoped_ptr<LinearSolver> linear_solver;
  scoped_ptr<IterationCallback> logging_callback;
  scoped_ptr<IterationCallback> state_updating_callback;

  shared_ptr<Evaluator> evaluator;
  shared_ptr<CoordinateDescentMinimizer> inner_iteration_minimizer;

  vector<double*> removed_parameter_blocks;
  Vector reduced_parameters;
  double fixed_cost;
};

// Common functions used by various preprocessors.

// If OpenMP support is not available and user has requested more than
// one thread, then set the *_num_threads options as needed to 1.
void ChangeNumThreadsIfNeeded(Solver::Options* options);

// Extract the effective parameter vector from the preprocessed
// problem and setup bits of the Minimizer::Options object that are
// common to all Preprocessors.
void SetupCommonMinimizerOptions(PreprocessedProblem* pp);

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_PREPROCESSOR_H_
