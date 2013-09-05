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
#include "ceres/problem.h"
#include "ceres/problem_impl.h"
#include "ceres/program.h"
#include "ceres/solver_impl.h"
#include "ceres/stringprintf.h"
#include "ceres/wall_time.h"

namespace ceres {
namespace {

void StringifyOrdering(const vector<int>& ordering, string* report) {
  if (ordering.size() == 0) {
    internal::StringAppendF(report, "AUTOMATIC");
    return;
  }

  for (int i = 0; i < ordering.size() - 1; ++i) {
    internal::StringAppendF(report, "%d, ", ordering[i]);
  }
  internal::StringAppendF(report, "%d", ordering.back());
}

}  // namespace

Solver::Options::~Options() {
  delete linear_solver_ordering;
  delete inner_iteration_ordering;
}

Solver::~Solver() {}

void Solver::Solve(const Solver::Options& options,
                   Problem* problem,
                   Solver::Summary* summary) {
  double start_time_seconds = internal::WallTimeInSeconds();
  internal::ProblemImpl* problem_impl =
      CHECK_NOTNULL(problem)->problem_impl_.get();
  internal::SolverImpl::Solve(options, problem_impl, summary);
  summary->total_time_in_seconds =
      internal::WallTimeInSeconds() - start_time_seconds;
}

void Solve(const Solver::Options& options,
           Problem* problem,
           Solver::Summary* summary) {
  Solver solver;
  solver.Solve(options, problem, summary);
}

Solver::Summary::Summary()
    // Invalid values for most fields, to ensure that we are not
    // accidentally reporting default values.
    : minimizer_type(TRUST_REGION),
      termination_type(DID_NOT_RUN),
      initial_cost(-1.0),
      final_cost(-1.0),
      fixed_cost(-1.0),
      num_successful_steps(-1),
      num_unsuccessful_steps(-1),
      num_inner_iteration_steps(-1),
      preprocessor_time_in_seconds(-1.0),
      minimizer_time_in_seconds(-1.0),
      postprocessor_time_in_seconds(-1.0),
      total_time_in_seconds(-1.0),
      linear_solver_time_in_seconds(-1.0),
      residual_evaluation_time_in_seconds(-1.0),
      jacobian_evaluation_time_in_seconds(-1.0),
      inner_iteration_time_in_seconds(-1.0),
      num_parameter_blocks(-1),
      num_parameters(-1),
      num_effective_parameters(-1),
      num_residual_blocks(-1),
      num_residuals(-1),
      num_parameter_blocks_reduced(-1),
      num_parameters_reduced(-1),
      num_effective_parameters_reduced(-1),
      num_residual_blocks_reduced(-1),
      num_residuals_reduced(-1),
      num_threads_given(-1),
      num_threads_used(-1),
      num_linear_solver_threads_given(-1),
      num_linear_solver_threads_used(-1),
      linear_solver_type_given(SPARSE_NORMAL_CHOLESKY),
      linear_solver_type_used(SPARSE_NORMAL_CHOLESKY),
      inner_iterations_given(false),
      inner_iterations_used(false),
      preconditioner_type(IDENTITY),
      trust_region_strategy_type(LEVENBERG_MARQUARDT),
      dense_linear_algebra_library_type(EIGEN),
      sparse_linear_algebra_library_type(SUITE_SPARSE),
      line_search_direction_type(LBFGS),
      line_search_type(ARMIJO) {
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

using internal::StringAppendF;
using internal::StringPrintf;

string Solver::Summary::FullReport() const {
  string report =
      "\n"
      "Ceres Solver Report\n"
      "-------------------\n";

  if (termination_type == DID_NOT_RUN) {
    StringAppendF(&report, "                      Original\n");
    StringAppendF(&report, "Parameter blocks    % 10d\n", num_parameter_blocks);
    StringAppendF(&report, "Parameters          % 10d\n", num_parameters);
    if (num_effective_parameters != num_parameters) {
      StringAppendF(&report, "Effective parameters% 10d\n", num_parameters);
    }

    StringAppendF(&report, "Residual blocks     % 10d\n",
                  num_residual_blocks);
    StringAppendF(&report, "Residuals           % 10d\n\n",
                  num_residuals);
  } else {
    StringAppendF(&report, "%45s    %21s\n", "Original", "Reduced");
    StringAppendF(&report, "Parameter blocks    % 25d% 25d\n",
                  num_parameter_blocks, num_parameter_blocks_reduced);
    StringAppendF(&report, "Parameters          % 25d% 25d\n",
                  num_parameters, num_parameters_reduced);
    if (num_effective_parameters_reduced != num_parameters_reduced) {
      StringAppendF(&report, "Effective parameters% 25d% 25d\n",
                    num_effective_parameters, num_effective_parameters_reduced);
    }
    StringAppendF(&report, "Residual blocks     % 25d% 25d\n",
                  num_residual_blocks, num_residual_blocks_reduced);
    StringAppendF(&report, "Residual            % 25d% 25d\n",
                  num_residuals, num_residuals_reduced);
  }

  if (minimizer_type == TRUST_REGION) {
    // TRUST_SEARCH HEADER
    StringAppendF(&report, "\nMinimizer                 %19s\n",
                  "TRUST_REGION");

    if (linear_solver_type_used == DENSE_NORMAL_CHOLESKY ||
        linear_solver_type_used == DENSE_SCHUR ||
        linear_solver_type_used == DENSE_QR) {
      StringAppendF(&report, "\nDense linear algebra library  %15s\n",
                    DenseLinearAlgebraLibraryTypeToString(
                        dense_linear_algebra_library_type));
    }

    if (linear_solver_type_used == SPARSE_NORMAL_CHOLESKY ||
        linear_solver_type_used == SPARSE_SCHUR ||
        (linear_solver_type_used == ITERATIVE_SCHUR &&
         (preconditioner_type == CLUSTER_JACOBI ||
          preconditioner_type == CLUSTER_TRIDIAGONAL))) {
      StringAppendF(&report, "\nSparse linear algebra library %15s\n",
                    SparseLinearAlgebraLibraryTypeToString(
                        sparse_linear_algebra_library_type));
    }

    StringAppendF(&report, "Trust region strategy     %19s",
                  TrustRegionStrategyTypeToString(
                      trust_region_strategy_type));
    if (trust_region_strategy_type == DOGLEG) {
      if (dogleg_type == TRADITIONAL_DOGLEG) {
        StringAppendF(&report, " (TRADITIONAL)");
      } else {
        StringAppendF(&report, " (SUBSPACE)");
      }
    }
    StringAppendF(&report, "\n");
    StringAppendF(&report, "\n");

    StringAppendF(&report, "%45s    %21s\n", "Given",  "Used");
    StringAppendF(&report, "Linear solver       %25s%25s\n",
                  LinearSolverTypeToString(linear_solver_type_given),
                  LinearSolverTypeToString(linear_solver_type_used));

    if (linear_solver_type_given == CGNR ||
        linear_solver_type_given == ITERATIVE_SCHUR) {
      StringAppendF(&report, "Preconditioner      %25s%25s\n",
                    PreconditionerTypeToString(preconditioner_type),
                    PreconditionerTypeToString(preconditioner_type));
    }

    StringAppendF(&report, "Threads             % 25d% 25d\n",
                  num_threads_given, num_threads_used);
    StringAppendF(&report, "Linear solver threads % 23d% 25d\n",
                  num_linear_solver_threads_given,
                  num_linear_solver_threads_used);

    if (IsSchurType(linear_solver_type_used)) {
      string given;
      StringifyOrdering(linear_solver_ordering_given, &given);
      string used;
      StringifyOrdering(linear_solver_ordering_used, &used);
      StringAppendF(&report,
                    "Linear solver ordering %22s %24s\n",
                    given.c_str(),
                    used.c_str());
    }

    if (inner_iterations_given) {
      StringAppendF(&report,
                    "Use inner iterations     %20s     %20s\n",
                    inner_iterations_given ? "True" : "False",
                    inner_iterations_used ? "True" : "False");
    }

    if (inner_iterations_used) {
      string given;
      StringifyOrdering(inner_iteration_ordering_given, &given);
      string used;
      StringifyOrdering(inner_iteration_ordering_used, &used);
    StringAppendF(&report,
                  "Inner iteration ordering %20s %24s\n",
                  given.c_str(),
                  used.c_str());
    }
  } else {
    // LINE_SEARCH HEADER
    StringAppendF(&report, "\nMinimizer                 %19s\n", "LINE_SEARCH");


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

    StringAppendF(&report, "%45s    %21s\n", "Given",  "Used");
    StringAppendF(&report, "Threads             % 25d% 25d\n",
                  num_threads_given, num_threads_used);
  }

  if (termination_type == DID_NOT_RUN) {
    CHECK(!error.empty())
        << "Solver terminated with DID_NOT_RUN but the solver did not "
        << "return a reason. This is a Ceres error. Please report this "
        << "to the Ceres team";
    StringAppendF(&report, "Termination:           %20s\n",
                  "DID_NOT_RUN");
    StringAppendF(&report, "Reason: %s\n", error.c_str());
    return report;
  }

  StringAppendF(&report, "\nCost:\n");
  StringAppendF(&report, "Initial        % 30e\n", initial_cost);
  if (termination_type != NUMERICAL_FAILURE &&
      termination_type != USER_ABORT) {
    StringAppendF(&report, "Final          % 30e\n", final_cost);
    StringAppendF(&report, "Change         % 30e\n",
                  initial_cost - final_cost);
  }

  StringAppendF(&report, "\nMinimizer iterations         % 16d\n",
                num_successful_steps + num_unsuccessful_steps);

  // Successful/Unsuccessful steps only matter in the case of the
  // trust region solver. Line search terminates when it encounters
  // the first unsuccessful step.
  if (minimizer_type == TRUST_REGION) {
    StringAppendF(&report, "Successful steps               % 14d\n",
                  num_successful_steps);
    StringAppendF(&report, "Unsuccessful steps             % 14d\n",
                  num_unsuccessful_steps);
  }
  if (inner_iterations_used) {
    StringAppendF(&report, "Steps with inner iterations    % 14d\n",
                  num_inner_iteration_steps);
  }

  StringAppendF(&report, "\nTime (in seconds):\n");
  StringAppendF(&report, "Preprocessor        %25.3f\n",
                preprocessor_time_in_seconds);

  StringAppendF(&report, "\n  Residual evaluation %23.3f\n",
                residual_evaluation_time_in_seconds);
  StringAppendF(&report, "  Jacobian evaluation %23.3f\n",
                jacobian_evaluation_time_in_seconds);

  if (minimizer_type == TRUST_REGION) {
    StringAppendF(&report, "  Linear solver       %23.3f\n",
                  linear_solver_time_in_seconds);
  }

  if (inner_iterations_used) {
    StringAppendF(&report, "  Inner iterations    %23.3f\n",
                  inner_iteration_time_in_seconds);
  }

  StringAppendF(&report, "Minimizer           %25.3f\n\n",
                minimizer_time_in_seconds);

  StringAppendF(&report, "Postprocessor        %24.3f\n",
                postprocessor_time_in_seconds);

  StringAppendF(&report, "Total               %25.3f\n\n",
                total_time_in_seconds);

  StringAppendF(&report, "Termination:        %25s\n",
                SolverTerminationTypeToString(termination_type));
  return report;
};

}  // namespace ceres
