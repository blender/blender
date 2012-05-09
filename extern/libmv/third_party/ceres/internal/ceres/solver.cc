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
// Author: keir@google.com (Keir Mierle)
//         sameeragarwal@google.com (Sameer Agarwal)

#include "ceres/solver.h"

#include <vector>
#include "ceres/levenberg_marquardt.h"
#include "ceres/program.h"
#include "ceres/solver_impl.h"
#include "ceres/stringprintf.h"
#include "ceres/problem.h"

namespace ceres {

Solver::~Solver() {}

// TODO(sameeragarwal): The timing code here should use a sub-second
// timer.
void Solver::Solve(const Solver::Options& options,
                   Problem* problem,
                   Solver::Summary* summary) {
  time_t start_time_seconds = time(NULL);
  internal::SolverImpl::Solve(options, problem, summary);
  summary->total_time_in_seconds =  time(NULL) - start_time_seconds;
  summary->preprocessor_time_in_seconds =
      summary->total_time_in_seconds - summary->minimizer_time_in_seconds;
}

void Solve(const Solver::Options& options,
           Problem* problem,
           Solver::Summary* summary) {
  time_t start_time_seconds = time(NULL);
  internal::SolverImpl::Solve(options, problem, summary);
  summary->total_time_in_seconds =  time(NULL) - start_time_seconds;
  summary->preprocessor_time_in_seconds =
      summary->total_time_in_seconds - summary->minimizer_time_in_seconds;
}

Solver::Summary::Summary()
    // Invalid values for most fields, to ensure that we are not
    // accidentally reporting default values.
    : termination_type(DID_NOT_RUN),
      initial_cost(-1.0),
      final_cost(-1.0),
      fixed_cost(-1.0),
      num_successful_steps(-1),
      num_unsuccessful_steps(-1),
      preprocessor_time_in_seconds(-1.0),
      minimizer_time_in_seconds(-1.0),
      total_time_in_seconds(-1.0),
      num_parameter_blocks(-1),
      num_parameters(-1),
      num_residual_blocks(-1),
      num_residuals(-1),
      num_parameter_blocks_reduced(-1),
      num_parameters_reduced(-1),
      num_residual_blocks_reduced(-1),
      num_residuals_reduced(-1),
      num_eliminate_blocks_given(-1),
      num_eliminate_blocks_used(-1),
      num_threads_given(-1),
      num_threads_used(-1),
      num_linear_solver_threads_given(-1),
      num_linear_solver_threads_used(-1),
      linear_solver_type_given(SPARSE_NORMAL_CHOLESKY),
      linear_solver_type_used(SPARSE_NORMAL_CHOLESKY),
      preconditioner_type(IDENTITY),
      ordering_type(NATURAL) {
}

string Solver::Summary::BriefReport() const {
  string report = "Ceres Solver Report: ";
  if (termination_type == DID_NOT_RUN) {
    CHECK(!error.empty())
          << "Solver terminated with DID_NOT_RUN but the solver did not "
          << "return a reason. This is a Ceres error. Please report this "
          << "to the Ceres team";
    return report + "Termination: DID_NOT_RUN, because " + error;
  }

  internal::StringAppendF(&report, "Iterations: %d",
                          num_successful_steps + num_unsuccessful_steps);
  internal::StringAppendF(&report, ", Initial cost: %e", initial_cost);

  // If the solver failed or was aborted, then the final_cost has no
  // meaning.
  if (termination_type != NUMERICAL_FAILURE &&
      termination_type != USER_ABORT) {
    internal::StringAppendF(&report, ", Final cost: %e", final_cost);
  }

  internal::StringAppendF(&report, ", Termination: %s.",
                          SolverTerminationTypeToString(termination_type));
  return report;
};

string Solver::Summary::FullReport() const {
  string report =
      "\n"
      "Ceres Solver Report\n"
      "-------------------\n";

  if (termination_type == DID_NOT_RUN) {
    internal::StringAppendF(&report, "                      Original\n");
    internal::StringAppendF(&report, "Parameter blocks    % 10d\n",
                            num_parameter_blocks);
    internal::StringAppendF(&report, "Parameters          % 10d\n",
                            num_parameters);
    internal::StringAppendF(&report, "Residual blocks     % 10d\n",
                            num_residual_blocks);
    internal::StringAppendF(&report, "Residual            % 10d\n\n",
                            num_residuals);
  } else {
    internal::StringAppendF(&report, "%45s    %21s\n", "Original", "Reduced");
    internal::StringAppendF(&report, "Parameter blocks    % 25d% 25d\n",
                            num_parameter_blocks, num_parameter_blocks_reduced);
    internal::StringAppendF(&report, "Parameters          % 25d% 25d\n",
                            num_parameters, num_parameters_reduced);
    internal::StringAppendF(&report, "Residual blocks     % 25d% 25d\n",
                            num_residual_blocks, num_residual_blocks_reduced);
    internal::StringAppendF(&report, "Residual            % 25d% 25d\n\n",
                          num_residuals, num_residuals_reduced);
  }

  internal::StringAppendF(&report,   "%45s    %21s\n", "Given",  "Used");
  internal::StringAppendF(&report, "Linear solver       %25s%25s\n",
                          LinearSolverTypeToString(linear_solver_type_given),
                          LinearSolverTypeToString(linear_solver_type_used));

  if (linear_solver_type_given == CGNR ||
      linear_solver_type_given == ITERATIVE_SCHUR) {
    internal::StringAppendF(&report, "Preconditioner      %25s%25s\n",
                            PreconditionerTypeToString(preconditioner_type),
                            PreconditionerTypeToString(preconditioner_type));
  } else {
    internal::StringAppendF(&report, "Preconditioner      %25s%25s\n",
                            "N/A", "N/A");
  }

  internal::StringAppendF(&report, "Ordering            %25s%25s\n",
                          OrderingTypeToString(ordering_type),
                          OrderingTypeToString(ordering_type));

  if (IsSchurType(linear_solver_type_given)) {
    if (ordering_type == SCHUR) {
      internal::StringAppendF(&report, "num_eliminate_blocks%25s% 25d\n",
                              "N/A",
                              num_eliminate_blocks_used);
    } else {
      internal::StringAppendF(&report, "num_eliminate_blocks% 25d% 25d\n",
                              num_eliminate_blocks_given,
                              num_eliminate_blocks_used);
    }
  }

  internal::StringAppendF(&report, "Threads:            % 25d% 25d\n",
                          num_threads_given, num_threads_used);
  internal::StringAppendF(&report, "Linear Solver Threads:% 23d% 25d\n",
                          num_linear_solver_threads_given,
                          num_linear_solver_threads_used);


  if (termination_type == DID_NOT_RUN) {
    CHECK(!error.empty())
        << "Solver terminated with DID_NOT_RUN but the solver did not "
        << "return a reason. This is a Ceres error. Please report this "
        << "to the Ceres team";
    internal::StringAppendF(&report, "Termination:           %20s\n",
                            "DID_NOT_RUN");
    internal::StringAppendF(&report, "Reason: %s\n", error.c_str());
    return report;
  }

  internal::StringAppendF(&report, "\nCost:\n");
  internal::StringAppendF(&report, "Initial        % 30e\n", initial_cost);
  if (termination_type != NUMERICAL_FAILURE && termination_type != USER_ABORT) {
    internal::StringAppendF(&report, "Final          % 30e\n", final_cost);
    internal::StringAppendF(&report, "Change         % 30e\n",
                            initial_cost - final_cost);
  }

  internal::StringAppendF(&report, "\nNumber of iterations:\n");
  internal::StringAppendF(&report, "Successful               % 20d\n",
                          num_successful_steps);
  internal::StringAppendF(&report, "Unsuccessful             % 20d\n",
                          num_unsuccessful_steps);
  internal::StringAppendF(&report, "Total                    % 20d\n",
                          num_successful_steps + num_unsuccessful_steps);
  internal::StringAppendF(&report, "\nTime (in seconds):\n");
  internal::StringAppendF(&report, "Preprocessor        % 25e\n",
                          preprocessor_time_in_seconds);
  internal::StringAppendF(&report, "Minimizer           % 25e\n",
                          minimizer_time_in_seconds);
  internal::StringAppendF(&report, "Total               % 25e\n",
                          total_time_in_seconds);

  internal::StringAppendF(&report, "Termination:        %25s\n",
                SolverTerminationTypeToString(termination_type));
  return report;
};

}  // namespace ceres
