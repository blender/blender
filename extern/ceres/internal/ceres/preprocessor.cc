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
// Author: sameragarwal@google.com (Sameer Agarwal)

#include "ceres/callbacks.h"
#include "ceres/gradient_checking_cost_function.h"
#include "ceres/line_search_preprocessor.h"
#include "ceres/parallel_for.h"
#include "ceres/preprocessor.h"
#include "ceres/problem_impl.h"
#include "ceres/solver.h"
#include "ceres/trust_region_preprocessor.h"

namespace ceres {
namespace internal {

Preprocessor* Preprocessor::Create(MinimizerType minimizer_type) {
  if (minimizer_type == TRUST_REGION) {
    return new TrustRegionPreprocessor;
  }

  if (minimizer_type == LINE_SEARCH) {
    return new LineSearchPreprocessor;
  }

  LOG(FATAL) << "Unknown minimizer_type: " << minimizer_type;
  return NULL;
}

Preprocessor::~Preprocessor() {
}

void ChangeNumThreadsIfNeeded(Solver::Options* options) {
  const int num_threads_available = MaxNumThreadsAvailable();
  if (options->num_threads > num_threads_available) {
    LOG(WARNING)
        << "Specified options.num_threads: " << options->num_threads
        << " exceeds maximum available from the threading model Ceres "
        << "was compiled with: " << num_threads_available
        << ".  Bounding to maximum number available.";
    options->num_threads = num_threads_available;
  }
}

void SetupCommonMinimizerOptions(PreprocessedProblem* pp) {
  const Solver::Options& options = pp->options;
  Program* program = pp->reduced_program.get();

  // Assuming that the parameter blocks in the program have been
  // reordered as needed, extract them into a contiguous vector.
  pp->reduced_parameters.resize(program->NumParameters());
  double* reduced_parameters = pp->reduced_parameters.data();
  program->ParameterBlocksToStateVector(reduced_parameters);

  Minimizer::Options& minimizer_options = pp->minimizer_options;
  minimizer_options = Minimizer::Options(options);
  minimizer_options.evaluator = pp->evaluator;

  if (options.logging_type != SILENT) {
    pp->logging_callback.reset(
        new LoggingCallback(options.minimizer_type,
                            options.minimizer_progress_to_stdout));
    minimizer_options.callbacks.insert(minimizer_options.callbacks.begin(),
                                       pp->logging_callback.get());
  }

  if (options.update_state_every_iteration) {
    pp->state_updating_callback.reset(
      new StateUpdatingCallback(program, reduced_parameters));
    // This must get pushed to the front of the callbacks so that it
    // is run before any of the user callbacks.
    minimizer_options.callbacks.insert(minimizer_options.callbacks.begin(),
                                       pp->state_updating_callback.get());
  }
}

}  // namespace internal
}  // namespace ceres
