// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2010, 2011, 2012 Google Inc. All rights reserved.
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

#ifndef CERES_INTERNAL_MINIMIZER_H_
#define CERES_INTERNAL_MINIMIZER_H_

#include <vector>
#include "ceres/solver.h"
#include "ceres/iteration_callback.h"

namespace ceres {
namespace internal {

class Evaluator;
class LinearSolver;
class SparseMatrix;
class TrustRegionStrategy;

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
      lsqp_dump_directory = options.lsqp_dump_directory;
      lsqp_iterations_to_dump = options.lsqp_iterations_to_dump;
      lsqp_dump_format_type = options.lsqp_dump_format_type;
      num_eliminate_blocks = options.num_eliminate_blocks;
      max_num_consecutive_invalid_steps =
          options.max_num_consecutive_invalid_steps;
      min_trust_region_radius = options.min_trust_region_radius;
      evaluator = NULL;
      trust_region_strategy = NULL;
      jacobian = NULL;
      callbacks = options.callbacks;
    }

    int max_num_iterations;
    double max_solver_time_in_seconds;

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
    vector<int> lsqp_iterations_to_dump;
    DumpFormatType lsqp_dump_format_type;
    string lsqp_dump_directory;
    int num_eliminate_blocks;
    int max_num_consecutive_invalid_steps;
    int min_trust_region_radius;

    // List of callbacks that are executed by the Minimizer at the end
    // of each iteration.
    //
    // The Options struct does not own these pointers.
    vector<IterationCallback*> callbacks;

    // Object responsible for evaluating the cost, residuals and
    // Jacobian matrix. The Options struct does not own this pointer.
    Evaluator* evaluator;

    // Object responsible for actually computing the trust region
    // step, and sizing the trust region radius. The Options struct
    // does not own this pointer.
    TrustRegionStrategy* trust_region_strategy;

    // Object holding the Jacobian matrix. It is assumed that the
    // sparsity structure of the matrix has already been initialized
    // and will remain constant for the life time of the
    // optimization. The Options struct does not own this pointer.
    SparseMatrix* jacobian;
  };

  virtual ~Minimizer() {}

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
