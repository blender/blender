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
// Author: sameeragarwal@google.com (Sameer Agarwal)

#include <iostream>  // NO LINT
#include "ceres/callbacks.h"
#include "ceres/program.h"
#include "ceres/stringprintf.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

StateUpdatingCallback::StateUpdatingCallback(Program* program,
                                             double* parameters)
    : program_(program), parameters_(parameters) {}

StateUpdatingCallback::~StateUpdatingCallback() {}

CallbackReturnType StateUpdatingCallback::operator()(
    const IterationSummary& summary) {
  if (summary.step_is_successful) {
    program_->StateVectorToParameterBlocks(parameters_);
    program_->CopyParameterBlockStateToUserState();
  }
  return SOLVER_CONTINUE;
}

LoggingCallback::LoggingCallback(const MinimizerType minimizer_type,
                                 const bool log_to_stdout)
    : minimizer_type(minimizer_type),
      log_to_stdout_(log_to_stdout) {}

LoggingCallback::~LoggingCallback() {}

CallbackReturnType LoggingCallback::operator()(
    const IterationSummary& summary) {
  string output;
  if (minimizer_type == LINE_SEARCH) {
    const char* kReportRowFormat =
        "% 4d: f:% 8e d:% 3.2e g:% 3.2e h:% 3.2e "
        "s:% 3.2e e:% 3d it:% 3.2e tt:% 3.2e";
    output = StringPrintf(kReportRowFormat,
                          summary.iteration,
                          summary.cost,
                          summary.cost_change,
                          summary.gradient_max_norm,
                          summary.step_norm,
                          summary.step_size,
                          summary.line_search_function_evaluations,
                          summary.iteration_time_in_seconds,
                          summary.cumulative_time_in_seconds);
  } else if (minimizer_type == TRUST_REGION) {
    if (summary.iteration == 0) {
      output = "iter      cost      cost_change  |gradient|   |step|    tr_ratio  tr_radius  ls_iter  iter_time  total_time\n";
    }
    const char* kReportRowFormat =
        "% 4d % 8e   % 3.2e   % 3.2e  % 3.2e  % 3.2e % 3.2e     % 4d   % 3.2e   % 3.2e";
    output += StringPrintf(kReportRowFormat,
                          summary.iteration,
                          summary.cost,
                          summary.cost_change,
                          summary.gradient_max_norm,
                          summary.step_norm,
                          summary.relative_decrease,
                          summary.trust_region_radius,
                          summary.linear_solver_iterations,
                          summary.iteration_time_in_seconds,
                          summary.cumulative_time_in_seconds);
  } else {
    LOG(FATAL) << "Unknown minimizer type.";
  }

  if (log_to_stdout_) {
    cout << output << endl;
  } else {
    VLOG(1) << output;
  }
  return SOLVER_CONTINUE;
}

}  // namespace internal
}  // namespace ceres
