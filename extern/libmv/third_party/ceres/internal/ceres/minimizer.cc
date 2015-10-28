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

#include "ceres/line_search_minimizer.h"
#include "ceres/minimizer.h"
#include "ceres/trust_region_minimizer.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

Minimizer* Minimizer::Create(MinimizerType minimizer_type) {
  if (minimizer_type == TRUST_REGION) {
    return new TrustRegionMinimizer;
  }

  if (minimizer_type == LINE_SEARCH) {
    return new LineSearchMinimizer;
  }

  LOG(FATAL) << "Unknown minimizer_type: " << minimizer_type;
  return NULL;
}


Minimizer::~Minimizer() {}

bool Minimizer::RunCallbacks(const Minimizer::Options& options,
                             const IterationSummary& iteration_summary,
                             Solver::Summary* summary) {
  const bool is_not_silent = !options.is_silent;
  CallbackReturnType status = SOLVER_CONTINUE;
  int i = 0;
  while (status == SOLVER_CONTINUE && i < options.callbacks.size()) {
    status = (*options.callbacks[i])(iteration_summary);
    ++i;
  }
  switch (status) {
    case SOLVER_CONTINUE:
      return true;
    case SOLVER_TERMINATE_SUCCESSFULLY:
      summary->termination_type = USER_SUCCESS;
      summary->message =
          "User callback returned SOLVER_TERMINATE_SUCCESSFULLY.";
      VLOG_IF(1, is_not_silent) << "Terminating: " << summary->message;
      return false;
    case SOLVER_ABORT:
      summary->termination_type = USER_FAILURE;
      summary->message = "User callback returned SOLVER_ABORT.";
      VLOG_IF(1, is_not_silent) << "Terminating: " << summary->message;
      return false;
    default:
      LOG(FATAL) << "Unknown type of user callback status";
  }
  return false;
}

}  // namespace internal
}  // namespace ceres
