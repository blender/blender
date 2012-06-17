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

// Interface for non-linear least squares solvers.
class Minimizer {
 public:
  // Options struct to control the behaviour of the Minimizer. Please
  // see solver.h for detailed information about the meaning and
  // default values of each of these parameters.
  struct Options {
    explicit Options(const Solver::Options& options) {
      max_num_iterations = options.max_num_iterations;
      max_solver_time_sec = options.max_solver_time_sec;
      gradient_tolerance = options.gradient_tolerance;
      parameter_tolerance = options.parameter_tolerance;
      function_tolerance = options.function_tolerance;
      min_relative_decrease = options.min_relative_decrease;
      eta = options.eta;
      tau = options.tau;
      jacobi_scaling = options.jacobi_scaling;
      crash_and_dump_lsqp_on_failure = options.crash_and_dump_lsqp_on_failure;
      lsqp_dump_directory = options.lsqp_dump_directory;
      lsqp_iterations_to_dump = options.lsqp_iterations_to_dump;
      lsqp_dump_format_type = options.lsqp_dump_format_type;
      num_eliminate_blocks = options.num_eliminate_blocks;
      logging_type = options.logging_type;
    }

    int max_num_iterations;
    int max_solver_time_sec;
    double gradient_tolerance;
    double parameter_tolerance;
    double function_tolerance;
    double min_relative_decrease;
    double eta;
    double tau;
    bool jacobi_scaling;
    bool crash_and_dump_lsqp_on_failure;
    vector<int> lsqp_iterations_to_dump;
    DumpFormatType lsqp_dump_format_type;
    string lsqp_dump_directory;
    int num_eliminate_blocks;
    LoggingType logging_type;

    // List of callbacks that are executed by the Minimizer at the end
    // of each iteration.
    //
    // Client owns these pointers.
    vector<IterationCallback*> callbacks;
  };

  virtual ~Minimizer() {}
  virtual void Minimize(const Options& options,
                        Evaluator* evaluator,
                        LinearSolver* linear_solver,
                        const double* initial_parameters,
                        double* final_parameters,
                        Solver::Summary* summary) = 0;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_MINIMIZER_H_
