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

#ifndef CERES_INTERNAL_MINIMIZER_H_
#define CERES_INTERNAL_MINIMIZER_H_

#include <memory>
#include <string>
#include <vector>
#include "ceres/internal/port.h"
#include "ceres/iteration_callback.h"
#include "ceres/solver.h"

namespace ceres {
namespace internal {

class Evaluator;
class SparseMatrix;
class TrustRegionStrategy;
class CoordinateDescentMinimizer;
class LinearSolver;

// Interface for non-linear least squares solvers.
class Minimizer {
 public:
  // Options struct to control the behaviour of the Minimizer. Please
  // see solver.h for detailed information about the meaning and
  // default values of each of these parameters.
  struct Options {
    Options() {
      Init(Solver::Options());
    }

    explicit Options(const Solver::Options& options) {
      Init(options);
    }

    void Init(const Solver::Options& options) {
      num_threads = options.num_threads;
      max_num_iterations = options.max_num_iterations;
      max_solver_time_in_seconds = options.max_solver_time_in_seconds;
      max_step_solver_retries = 5;
      gradient_tolerance = options.gradient_tolerance;
      parameter_tolerance = options.parameter_tolerance;
      function_tolerance = options.function_tolerance;
      min_relative_decrease = options.min_relative_decrease;
      eta = options.eta;
      jacobi_scaling = options.jacobi_scaling;
      use_nonmonotonic_steps = options.use_nonmonotonic_steps;
      max_consecutive_nonmonotonic_steps =
          options.max_consecutive_nonmonotonic_steps;
      trust_region_problem_dump_directory =
          options.trust_region_problem_dump_directory;
      trust_region_minimizer_iterations_to_dump =
          options.trust_region_minimizer_iterations_to_dump;
      trust_region_problem_dump_format_type =
          options.trust_region_problem_dump_format_type;
      max_num_consecutive_invalid_steps =
          options.max_num_consecutive_invalid_steps;
      min_trust_region_radius = options.min_trust_region_radius;
      line_search_direction_type = options.line_search_direction_type;
      line_search_type = options.line_search_type;
      nonlinear_conjugate_gradient_type =
          options.nonlinear_conjugate_gradient_type;
      max_lbfgs_rank = options.max_lbfgs_rank;
      use_approximate_eigenvalue_bfgs_scaling =
          options.use_approximate_eigenvalue_bfgs_scaling;
      line_search_interpolation_type =
          options.line_search_interpolation_type;
      min_line_search_step_size = options.min_line_search_step_size;
      line_search_sufficient_function_decrease =
          options.line_search_sufficient_function_decrease;
      max_line_search_step_contraction =
          options.max_line_search_step_contraction;
      min_line_search_step_contraction =
          options.min_line_search_step_contraction;
      max_num_line_search_step_size_iterations =
          options.max_num_line_search_step_size_iterations;
      max_num_line_search_direction_restarts =
          options.max_num_line_search_direction_restarts;
      line_search_sufficient_curvature_decrease =
          options.line_search_sufficient_curvature_decrease;
      max_line_search_step_expansion =
          options.max_line_search_step_expansion;
      inner_iteration_tolerance = options.inner_iteration_tolerance;
      is_silent = (options.logging_type == SILENT);
      is_constrained = false;
      callbacks = options.callbacks;
    }

    int max_num_iterations;
    double max_solver_time_in_seconds;
    int num_threads;

    // Number of times the linear solver should be retried in case of
    // numerical failure. The retries are done by exponentially scaling up
    // mu at each retry. This leads to stronger and stronger
    // regularization making the linear least squares problem better
    // conditioned at each retry.
    int max_step_solver_retries;
    double gradient_tolerance;
    double parameter_tolerance;
    double function_tolerance;
    double min_relative_decrease;
    double eta;
    bool jacobi_scaling;
    bool use_nonmonotonic_steps;
    int max_consecutive_nonmonotonic_steps;
    std::vector<int> trust_region_minimizer_iterations_to_dump;
    DumpFormatType trust_region_problem_dump_format_type;
    std::string trust_region_problem_dump_directory;
    int max_num_consecutive_invalid_steps;
    double min_trust_region_radius;
    LineSearchDirectionType line_search_direction_type;
    LineSearchType line_search_type;
    NonlinearConjugateGradientType nonlinear_conjugate_gradient_type;
    int max_lbfgs_rank;
    bool use_approximate_eigenvalue_bfgs_scaling;
    LineSearchInterpolationType line_search_interpolation_type;
    double min_line_search_step_size;
    double line_search_sufficient_function_decrease;
    double max_line_search_step_contraction;
    double min_line_search_step_contraction;
    int max_num_line_search_step_size_iterations;
    int max_num_line_search_direction_restarts;
    double line_search_sufficient_curvature_decrease;
    double max_line_search_step_expansion;
    double inner_iteration_tolerance;

    // If true, then all logging is disabled.
    bool is_silent;

    // Use a bounds constrained optimization algorithm.
    bool is_constrained;

    // List of callbacks that are executed by the Minimizer at the end
    // of each iteration.
    //
    // The Options struct does not own these pointers.
    std::vector<IterationCallback*> callbacks;

    // Object responsible for evaluating the cost, residuals and
    // Jacobian matrix.
    std::shared_ptr<Evaluator> evaluator;

    // Object responsible for actually computing the trust region
    // step, and sizing the trust region radius.
    std::shared_ptr<TrustRegionStrategy> trust_region_strategy;

    // Object holding the Jacobian matrix. It is assumed that the
    // sparsity structure of the matrix has already been initialized
    // and will remain constant for the life time of the
    // optimization.
    std::shared_ptr<SparseMatrix> jacobian;

    std::shared_ptr<CoordinateDescentMinimizer> inner_iteration_minimizer;
  };

  static Minimizer* Create(MinimizerType minimizer_type);
  static bool RunCallbacks(const Options& options,
                           const IterationSummary& iteration_summary,
                           Solver::Summary* summary);

  virtual ~Minimizer();
  // Note: The minimizer is expected to update the state of the
  // parameters array every iteration. This is required for the
  // StateUpdatingCallback to work.
  virtual void Minimize(const Options& options,
                        double* parameters,
                        Solver::Summary* summary) = 0;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_MINIMIZER_H_
