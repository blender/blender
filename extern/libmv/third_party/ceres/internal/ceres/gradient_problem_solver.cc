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

#include "ceres/gradient_problem_solver.h"

#include "ceres/callbacks.h"
#include "ceres/gradient_problem.h"
#include "ceres/gradient_problem_evaluator.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/port.h"
#include "ceres/map_util.h"
#include "ceres/minimizer.h"
#include "ceres/solver.h"
#include "ceres/solver_utils.h"
#include "ceres/stringprintf.h"
#include "ceres/types.h"
#include "ceres/wall_time.h"

namespace ceres {
using internal::StringPrintf;
using internal::StringAppendF;

namespace {

Solver::Options GradientProblemSolverOptionsToSolverOptions(
    const GradientProblemSolver::Options& options) {
#define COPY_OPTION(x) solver_options.x = options.x

  Solver::Options solver_options;
  solver_options.minimizer_type = LINE_SEARCH;
  COPY_OPTION(line_search_direction_type);
  COPY_OPTION(line_search_type);
  COPY_OPTION(nonlinear_conjugate_gradient_type);
  COPY_OPTION(max_lbfgs_rank);
  COPY_OPTION(use_approximate_eigenvalue_bfgs_scaling);
  COPY_OPTION(line_search_interpolation_type);
  COPY_OPTION(min_line_search_step_size);
  COPY_OPTION(line_search_sufficient_function_decrease);
  COPY_OPTION(max_line_search_step_contraction);
  COPY_OPTION(min_line_search_step_contraction);
  COPY_OPTION(max_num_line_search_step_size_iterations);
  COPY_OPTION(max_num_line_search_direction_restarts);
  COPY_OPTION(line_search_sufficient_curvature_decrease);
  COPY_OPTION(max_line_search_step_expansion);
  COPY_OPTION(max_num_iterations);
  COPY_OPTION(max_solver_time_in_seconds);
  COPY_OPTION(function_tolerance);
  COPY_OPTION(gradient_tolerance);
  COPY_OPTION(logging_type);
  COPY_OPTION(minimizer_progress_to_stdout);
  COPY_OPTION(callbacks);
  return solver_options;
#undef COPY_OPTION
}


}  // namespace

GradientProblemSolver::~GradientProblemSolver() {
}

void GradientProblemSolver::Solve(const GradientProblemSolver::Options& options,
                                  const GradientProblem& problem,
                                  double* parameters_ptr,
                                  GradientProblemSolver::Summary* summary) {
  using internal::scoped_ptr;
  using internal::WallTimeInSeconds;
  using internal::Minimizer;
  using internal::GradientProblemEvaluator;
  using internal::LoggingCallback;
  using internal::SetSummaryFinalCost;

  double start_time = WallTimeInSeconds();
  Solver::Options solver_options =
      GradientProblemSolverOptionsToSolverOptions(options);

  *CHECK_NOTNULL(summary) = Summary();
  summary->num_parameters                    = problem.NumParameters();
  summary->num_local_parameters              = problem.NumLocalParameters();
  summary->line_search_direction_type        = options.line_search_direction_type;         //  NOLINT
  summary->line_search_interpolation_type    = options.line_search_interpolation_type;     //  NOLINT
  summary->line_search_type                  = options.line_search_type;
  summary->max_lbfgs_rank                    = options.max_lbfgs_rank;
  summary->nonlinear_conjugate_gradient_type = options.nonlinear_conjugate_gradient_type;  //  NOLINT

  // Check validity
  if (!solver_options.IsValid(&summary->message)) {
    LOG(ERROR) << "Terminating: " << summary->message;
    return;
  }

  // Assuming that the parameter blocks in the program have been
  Minimizer::Options minimizer_options;
  minimizer_options = Minimizer::Options(solver_options);
  minimizer_options.evaluator.reset(new GradientProblemEvaluator(problem));

  scoped_ptr<IterationCallback> logging_callback;
  if (options.logging_type != SILENT) {
    logging_callback.reset(
        new LoggingCallback(LINE_SEARCH, options.minimizer_progress_to_stdout));
    minimizer_options.callbacks.insert(minimizer_options.callbacks.begin(),
                                       logging_callback.get());
  }

  scoped_ptr<Minimizer> minimizer(Minimizer::Create(LINE_SEARCH));
  Vector solution(problem.NumParameters());
  VectorRef parameters(parameters_ptr, problem.NumParameters());
  solution = parameters;

  Solver::Summary solver_summary;
  solver_summary.fixed_cost = 0.0;
  solver_summary.preprocessor_time_in_seconds = 0.0;
  solver_summary.postprocessor_time_in_seconds = 0.0;

  minimizer->Minimize(minimizer_options, solution.data(), &solver_summary);

  summary->termination_type = solver_summary.termination_type;
  summary->message          = solver_summary.message;
  summary->initial_cost     = solver_summary.initial_cost;
  summary->final_cost       = solver_summary.final_cost;
  summary->iterations       = solver_summary.iterations;

  if (summary->IsSolutionUsable()) {
    parameters = solution;
    SetSummaryFinalCost(summary);
  }

  const map<string, double>& evaluator_time_statistics =
       minimizer_options.evaluator->TimeStatistics();
  summary->cost_evaluation_time_in_seconds =
      FindWithDefault(evaluator_time_statistics, "Evaluator::Residual", 0.0);
  summary->gradient_evaluation_time_in_seconds =
      FindWithDefault(evaluator_time_statistics, "Evaluator::Jacobian", 0.0);

  summary->total_time_in_seconds = WallTimeInSeconds() - start_time;
}

// Invalid values for most fields, to ensure that we are not
// accidentally reporting default values.
GradientProblemSolver::Summary::Summary()
    : termination_type(FAILURE),
      message("ceres::GradientProblemSolve was not called."),
      initial_cost(-1.0),
      final_cost(-1.0),
      total_time_in_seconds(-1.0),
      cost_evaluation_time_in_seconds(-1.0),
      gradient_evaluation_time_in_seconds(-1.0),
      num_parameters(-1),
      num_local_parameters(-1),
      line_search_direction_type(LBFGS),
      line_search_type(ARMIJO),
      line_search_interpolation_type(BISECTION),
      nonlinear_conjugate_gradient_type(FLETCHER_REEVES),
      max_lbfgs_rank(-1) {
}

bool GradientProblemSolver::Summary::IsSolutionUsable() const {
  return internal::IsSolutionUsable(*this);
}

string GradientProblemSolver::Summary::BriefReport() const {
  return StringPrintf("Ceres GradientProblemSolver Report: "
                      "Iterations: %d, "
                      "Initial cost: %e, "
                      "Final cost: %e, "
                      "Termination: %s",
                      static_cast<int>(iterations.size()),
                      initial_cost,
                      final_cost,
                      TerminationTypeToString(termination_type));
};

string GradientProblemSolver::Summary::FullReport() const {
  using internal::VersionString;

  string report = string("\nSolver Summary (v " + VersionString() + ")\n\n");

  StringAppendF(&report, "Parameters          % 25d\n", num_parameters);
  if (num_local_parameters != num_parameters) {
    StringAppendF(&report, "Local parameters    % 25d\n",
                  num_local_parameters);
  }

  string line_search_direction_string;
  if (line_search_direction_type == LBFGS) {
    line_search_direction_string = StringPrintf("LBFGS (%d)", max_lbfgs_rank);
  } else if (line_search_direction_type == NONLINEAR_CONJUGATE_GRADIENT) {
    line_search_direction_string =
        NonlinearConjugateGradientTypeToString(
            nonlinear_conjugate_gradient_type);
  } else {
    line_search_direction_string =
        LineSearchDirectionTypeToString(line_search_direction_type);
  }

  StringAppendF(&report, "Line search direction     %19s\n",
                line_search_direction_string.c_str());

  const string line_search_type_string =
      StringPrintf("%s %s",
                   LineSearchInterpolationTypeToString(
                       line_search_interpolation_type),
                   LineSearchTypeToString(line_search_type));
  StringAppendF(&report, "Line search type          %19s\n",
                line_search_type_string.c_str());
  StringAppendF(&report, "\n");

  StringAppendF(&report, "\nCost:\n");
  StringAppendF(&report, "Initial        % 30e\n", initial_cost);
  if (termination_type != FAILURE &&
      termination_type != USER_FAILURE) {
    StringAppendF(&report, "Final          % 30e\n", final_cost);
    StringAppendF(&report, "Change         % 30e\n",
                  initial_cost - final_cost);
  }

  StringAppendF(&report, "\nMinimizer iterations         % 16d\n",
                static_cast<int>(iterations.size()));

  StringAppendF(&report, "\nTime (in seconds):\n");

  StringAppendF(&report, "\n  Cost evaluation     %23.3f\n",
                cost_evaluation_time_in_seconds);
  StringAppendF(&report, "  Gradient evaluation %23.3f\n",
                gradient_evaluation_time_in_seconds);

  StringAppendF(&report, "Total               %25.3f\n\n",
                total_time_in_seconds);

  StringAppendF(&report, "Termination:        %25s (%s)\n",
                TerminationTypeToString(termination_type), message.c_str());
  return report;
};

void Solve(const GradientProblemSolver::Options& options,
           const GradientProblem& problem,
           double* parameters,
           GradientProblemSolver::Summary* summary) {
  GradientProblemSolver solver;
  solver.Solve(options, problem, parameters, summary);
}

}  // namespace ceres
