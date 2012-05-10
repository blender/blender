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
//
// Implementation of a simple LM algorithm which uses the step sizing
// rule of "Methods for Nonlinear Least Squares" by K. Madsen,
// H.B. Nielsen and O. Tingleff. Available to download from
//
// http://www2.imm.dtu.dk/pubdb/views/edoc_download.php/3215/pdf/imm3215.pdf
//
// The basic algorithm described in this note is an exact step
// algorithm that depends on the Newton(LM) step being solved exactly
// in each iteration. When a suitable iterative solver is available to
// solve the Newton(LM) step, the algorithm will automatically switch
// to an inexact step solution method. This trades some slowdown in
// convergence for significant savings in solve time and memory
// usage. Our implementation of the Truncated Newton algorithm follows
// the discussion and recommendataions in "Stephen G. Nash, A Survey
// of Truncated Newton Methods, Journal of Computational and Applied
// Mathematics, 124(1-2), 45-59, 2000.

#include "ceres/levenberg_marquardt.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include <glog/logging.h>
#include "Eigen/Core"
#include "ceres/evaluator.h"
#include "ceres/file.h"
#include "ceres/linear_least_squares_problems.h"
#include "ceres/linear_solver.h"
#include "ceres/matrix_proto.h"
#include "ceres/sparse_matrix.h"
#include "ceres/stringprintf.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/types.h"

namespace ceres {
namespace internal {
namespace {

// Numbers for clamping the size of the LM diagonal. The size of these
// numbers is heuristic. We will probably be adjusting them in the
// future based on more numerical experience. With jacobi scaling
// enabled, these numbers should be all but redundant.
const double kMinLevenbergMarquardtDiagonal = 1e-6;
const double kMaxLevenbergMarquardtDiagonal = 1e32;

// Small constant for various floating point issues.
const double kEpsilon = 1e-12;

// Number of times the linear solver should be retried in case of
// numerical failure. The retries are done by exponentially scaling up
// mu at each retry. This leads to stronger and stronger
// regularization making the linear least squares problem better
// conditioned at each retry.
const int kMaxLinearSolverRetries = 5;

// D = 1/sqrt(diag(J^T * J))
void EstimateScale(const SparseMatrix& jacobian, double* D) {
  CHECK_NOTNULL(D);
  jacobian.SquaredColumnNorm(D);
  for (int i = 0; i < jacobian.num_cols(); ++i) {
    D[i] = 1.0 / (kEpsilon + sqrt(D[i]));
  }
}

// D = diag(J^T * J)
void LevenbergMarquardtDiagonal(const SparseMatrix& jacobian,
                                double* D) {
  CHECK_NOTNULL(D);
  jacobian.SquaredColumnNorm(D);
  for (int i = 0; i < jacobian.num_cols(); ++i) {
    D[i] = min(max(D[i], kMinLevenbergMarquardtDiagonal),
               kMaxLevenbergMarquardtDiagonal);
  }
}

bool RunCallback(IterationCallback* callback,
                 const IterationSummary& iteration_summary,
                 Solver::Summary* summary) {
  const CallbackReturnType status = (*callback)(iteration_summary);
  switch (status) {
    case SOLVER_TERMINATE_SUCCESSFULLY:
      summary->termination_type = USER_SUCCESS;
      VLOG(1) << "Terminating on USER_SUCCESS.";
      return false;
    case SOLVER_ABORT:
      summary->termination_type = USER_ABORT;
      VLOG(1) << "Terminating on USER_ABORT.";
      return false;
    case SOLVER_CONTINUE:
      return true;
    default:
      LOG(FATAL) << "Unknown status returned by callback: "
                 << status;
	  return NULL;
  }
}

}  // namespace

LevenbergMarquardt::~LevenbergMarquardt() {}

void LevenbergMarquardt::Minimize(const Minimizer::Options& options,
                                  Evaluator* evaluator,
                                  LinearSolver* linear_solver,
                                  const double* initial_parameters,
                                  double* final_parameters,
                                  Solver::Summary* summary) {
  time_t start_time = time(NULL);
  const int num_parameters = evaluator->NumParameters();
  const int num_effective_parameters = evaluator->NumEffectiveParameters();
  const int num_residuals = evaluator->NumResiduals();

  summary->termination_type = NO_CONVERGENCE;
  summary->num_successful_steps = 0;
  summary->num_unsuccessful_steps = 0;

  // Allocate the various vectors needed by the algorithm.
  memcpy(final_parameters, initial_parameters,
         num_parameters * sizeof(*initial_parameters));

  VectorRef x(final_parameters, num_parameters);
  Vector x_new(num_parameters);

  Vector lm_step(num_effective_parameters);
  Vector gradient(num_effective_parameters);
  Vector scaled_gradient(num_effective_parameters);
  // Jacobi scaling vector
  Vector scale(num_effective_parameters);

  Vector f_model(num_residuals);
  Vector f(num_residuals);
  Vector f_new(num_residuals);
  Vector D(num_parameters);
  Vector muD(num_parameters);

  // Ask the Evaluator to create the jacobian matrix. The sparsity
  // pattern of this matrix is going to remain constant, so we only do
  // this once and then re-use this matrix for all subsequent Jacobian
  // computations.
  scoped_ptr<SparseMatrix> jacobian(evaluator->CreateJacobian());

  double x_norm  = x.norm();

  double cost = 0.0;
  D.setOnes();
  f.setZero();

  // Do initial cost and Jacobian evaluation.
  if (!evaluator->Evaluate(x.data(), &cost, f.data(), jacobian.get())) {
    LOG(WARNING) << "Failed to compute residuals and Jacobian. "
                 << "Terminating.";
    summary->termination_type = NUMERICAL_FAILURE;
    return;
  }

  if (options.jacobi_scaling) {
    EstimateScale(*jacobian, scale.data());
    jacobian->ScaleColumns(scale.data());
  } else {
    scale.setOnes();
  }

  // This is a poor way to do this computation. Even if fixed_cost is
  // zero, because we are subtracting two possibly large numbers, we
  // are depending on exact cancellation to give us a zero here. But
  // initial_cost and cost have been computed by two different
  // evaluators. One which runs on the whole problem (in
  // solver_impl.cc) in single threaded mode and another which runs
  // here on the reduced problem, so fixed_cost can (and does) contain
  // some numerical garbage with a relative magnitude of 1e-14.
  //
  // The right way to do this, would be to compute the fixed cost on
  // just the set of residual blocks which are held constant and were
  // removed from the original problem when the reduced problem was
  // constructed.
  summary->fixed_cost = summary->initial_cost - cost;

  double model_cost = f.squaredNorm() / 2.0;
  double total_cost = summary->fixed_cost + cost;

  scaled_gradient.setZero();
  jacobian->LeftMultiply(f.data(), scaled_gradient.data());
  gradient = scaled_gradient.array() / scale.array();

  double gradient_max_norm = gradient.lpNorm<Eigen::Infinity>();
  // We need the max here to guard againt the gradient being zero.
  const double gradient_max_norm_0 = max(gradient_max_norm, kEpsilon);
  double gradient_tolerance = options.gradient_tolerance * gradient_max_norm_0;

  double mu = options.tau;
  double nu = 2.0;
  int iteration = 0;
  double actual_cost_change = 0.0;
  double step_norm = 0.0;
  double relative_decrease = 0.0;

  // Insane steps are steps which are not sane, i.e. there is some
  // numerical kookiness going on with them. There are various reasons
  // for this kookiness, some easier to diagnose then others. From the
  // point of view of the non-linear solver, they are steps which
  // cannot be used. We return with NUMERICAL_FAILURE after
  // kMaxLinearSolverRetries consecutive insane steps.
  bool step_is_sane = false;
  int num_consecutive_insane_steps = 0;

  // Whether the step resulted in a sufficient decrease in the
  // objective function when compared to the decrease in the value of
  // the lineariztion.
  bool step_is_successful = false;

  // Parse the iterations for which to dump the linear problem.
  vector<int> iterations_to_dump = options.lsqp_iterations_to_dump;
  sort(iterations_to_dump.begin(), iterations_to_dump.end());

  IterationSummary iteration_summary;
  iteration_summary.iteration = iteration;
  iteration_summary.step_is_successful = false;
  iteration_summary.cost = total_cost;
  iteration_summary.cost_change = actual_cost_change;
  iteration_summary.gradient_max_norm = gradient_max_norm;
  iteration_summary.step_norm = step_norm;
  iteration_summary.relative_decrease = relative_decrease;
  iteration_summary.mu = mu;
  iteration_summary.eta = options.eta;
  iteration_summary.linear_solver_iterations = 0;
  iteration_summary.linear_solver_time_sec = 0.0;
  iteration_summary.iteration_time_sec = (time(NULL) - start_time);
  if (options.logging_type >= PER_MINIMIZER_ITERATION) {
    summary->iterations.push_back(iteration_summary);
  }

  // Check if the starting point is an optimum.
  VLOG(2) << "Gradient max norm: " << gradient_max_norm
          << " tolerance: " << gradient_tolerance
          << " ratio: " << gradient_max_norm / gradient_max_norm_0
          << " tolerance: " << options.gradient_tolerance;
  if (gradient_max_norm <= gradient_tolerance) {
    summary->termination_type = GRADIENT_TOLERANCE;
    VLOG(1) << "Terminating on GRADIENT_TOLERANCE. "
            << "Relative gradient max norm: "
            << gradient_max_norm / gradient_max_norm_0
            << " <= " << options.gradient_tolerance;
    return;
  }

  // Call the various callbacks.
  for (int i = 0; i < options.callbacks.size(); ++i) {
    if (!RunCallback(options.callbacks[i], iteration_summary, summary)) {
      return;
    }
  }

  // We only need the LM diagonal if we are actually going to do at
  // least one iteration of the optimization. So we wait to do it
  // until now.
  LevenbergMarquardtDiagonal(*jacobian, D.data());

  while ((iteration < options.max_num_iterations) &&
         (time(NULL) - start_time) <= options.max_solver_time_sec) {
    time_t iteration_start_time = time(NULL);
    step_is_sane = false;
    step_is_successful = false;

    IterationSummary iteration_summary;
    // The while loop here is just to provide an easily breakable
    // control structure. We are guaranteed to always exit this loop
    // at the end of one iteration or before.
    while (1) {
      muD = (mu * D).array().sqrt();
      LinearSolver::PerSolveOptions solve_options;
      solve_options.D = muD.data();
      solve_options.q_tolerance = options.eta;
      // Disable r_tolerance checking. Since we only care about
      // termination via the q_tolerance. As Nash and Sofer show,
      // r_tolerance based termination is essentially useless in
      // Truncated Newton methods.
      solve_options.r_tolerance = -1.0;

      const time_t linear_solver_start_time = time(NULL);
      LinearSolver::Summary linear_solver_summary =
          linear_solver->Solve(jacobian.get(),
                               f.data(),
                               solve_options,
                               lm_step.data());
      iteration_summary.linear_solver_time_sec =
          (time(NULL) - linear_solver_start_time);
      iteration_summary.linear_solver_iterations =
          linear_solver_summary.num_iterations;

      if (binary_search(iterations_to_dump.begin(),
                        iterations_to_dump.end(),
                        iteration)) {
        CHECK(DumpLinearLeastSquaresProblem(options.lsqp_dump_directory,
                                            iteration,
                                            options.lsqp_dump_format_type,
                                            jacobian.get(),
                                            muD.data(),
                                            f.data(),
                                            lm_step.data(),
                                            options.num_eliminate_blocks))
            << "Tried writing linear least squares problem: " 
            << options.lsqp_dump_directory
            << " but failed.";
      }

      // We ignore the case where the linear solver did not converge,
      // since the partial solution computed by it still maybe of use,
      // and there is no reason to ignore it, especially since we
      // spent so much time computing it.
      if ((linear_solver_summary.termination_type != TOLERANCE) &&
          (linear_solver_summary.termination_type != MAX_ITERATIONS)) {
        VLOG(1) << "Linear solver failure: retrying with a higher mu";
        break;
      }

      step_norm = (lm_step.array() * scale.array()).matrix().norm();

      // Check step length based convergence. If the step length is
      // too small, then we are done.
      const double step_size_tolerance =  options.parameter_tolerance *
          (x_norm + options.parameter_tolerance);

      VLOG(2) << "Step size: " << step_norm
              << " tolerance: " <<  step_size_tolerance
              << " ratio: " << step_norm / step_size_tolerance
              << " tolerance: " << options.parameter_tolerance;
      if (step_norm <= options.parameter_tolerance *
          (x_norm + options.parameter_tolerance)) {
        summary->termination_type = PARAMETER_TOLERANCE;
        VLOG(1) << "Terminating on PARAMETER_TOLERANCE."
                << "Relative step size: " << step_norm / step_size_tolerance
            << " <= " << options.parameter_tolerance;
        return;
      }

      Vector delta =  -(lm_step.array() * scale.array()).matrix();
      if (!evaluator->Plus(x.data(), delta.data(), x_new.data())) {
        LOG(WARNING) << "Failed to compute Plus(x, delta, x_plus_delta). "
                     << "Terminating.";
        summary->termination_type = NUMERICAL_FAILURE;
        return;
      }

      double cost_new = 0.0;
      if (!evaluator->Evaluate(x_new.data(), &cost_new, NULL, NULL)) {
        LOG(WARNING) << "Failed to compute the value of the objective "
                     << "function. Terminating.";
        summary->termination_type = NUMERICAL_FAILURE;
        return;
      }

      f_model.setZero();
      jacobian->RightMultiply(lm_step.data(), f_model.data());
      const double model_cost_new =
          (f.segment(0, num_residuals) - f_model).squaredNorm() / 2;

      actual_cost_change = cost - cost_new;
      double model_cost_change = model_cost - model_cost_new;

      VLOG(2) << "[Model cost] current: " << model_cost
              << " new : " << model_cost_new
              << " change: " << model_cost_change;

      VLOG(2) << "[Nonlinear cost] current: " << cost
              << " new : " << cost_new
              << " change: " << actual_cost_change
              << " relative change: " << fabs(actual_cost_change) / cost
              << " tolerance: " << options.function_tolerance;

      // In exact arithmetic model_cost_change should never be
      // negative. But due to numerical precision issues, we may end up
      // with a small negative number. model_cost_change which are
      // negative and large in absolute value are indicative of a
      // numerical failure in the solver.
      if (model_cost_change < -kEpsilon) {
        VLOG(1) << "Model cost change is negative.\n"
                << "Current : " << model_cost
                << " new : " << model_cost_new
                << " change: " << model_cost_change << "\n";
        break;
      }

      // If we have reached this far, then we are willing to trust the
      // numerical quality of the step.
      step_is_sane = true;
      num_consecutive_insane_steps = 0;

      // Check function value based convergence.
      if (fabs(actual_cost_change) < options.function_tolerance * cost) {
        VLOG(1) << "Termination on FUNCTION_TOLERANCE."
                << " Relative cost change: " << fabs(actual_cost_change) / cost
                << " tolerance: " << options.function_tolerance;
        summary->termination_type = FUNCTION_TOLERANCE;
        return;
      }

      // Clamp model_cost_change at kEpsilon from below.
      if (model_cost_change < kEpsilon) {
        VLOG(1) << "Clamping model cost change " << model_cost_change
                << " to " << kEpsilon;
        model_cost_change = kEpsilon;
      }

      relative_decrease = actual_cost_change / model_cost_change;
      VLOG(2) << "actual_cost_change / model_cost_change = "
              << relative_decrease;

      if (relative_decrease < options.min_relative_decrease) {
        VLOG(2) << "Unsuccessful step.";
        break;
      }

      VLOG(2) << "Successful step.";

      ++summary->num_successful_steps;
      x = x_new;
      x_norm = x.norm();

      if (!evaluator->Evaluate(x.data(), &cost, f.data(), jacobian.get())) {
        LOG(WARNING) << "Failed to compute residuals and jacobian. "
                     << "Terminating.";
        summary->termination_type = NUMERICAL_FAILURE;
        return;
      }

      if (options.jacobi_scaling) {
        jacobian->ScaleColumns(scale.data());
      }

      model_cost = f.squaredNorm() / 2.0;
      LevenbergMarquardtDiagonal(*jacobian, D.data());
      scaled_gradient.setZero();
      jacobian->LeftMultiply(f.data(), scaled_gradient.data());
      gradient = scaled_gradient.array() / scale.array();
      gradient_max_norm = gradient.lpNorm<Eigen::Infinity>();

      // Check gradient based convergence.
      VLOG(2) << "Gradient max norm: " << gradient_max_norm
              << " tolerance: " << gradient_tolerance
              << " ratio: " << gradient_max_norm / gradient_max_norm_0
              << " tolerance: " << options.gradient_tolerance;
      if (gradient_max_norm <= gradient_tolerance) {
        summary->termination_type = GRADIENT_TOLERANCE;
        VLOG(1) << "Terminating on GRADIENT_TOLERANCE. "
                << "Relative gradient max norm: "
                << gradient_max_norm / gradient_max_norm_0
                << " <= " << options.gradient_tolerance
                << " (tolerance).";
        return;
      }

      mu = mu * max(1.0 / 3.0, 1 - pow(2 * relative_decrease - 1, 3));
      nu = 2.0;
      step_is_successful = true;
      break;
    }

    if (!step_is_sane) {
      ++num_consecutive_insane_steps;
    }

    if (num_consecutive_insane_steps == kMaxLinearSolverRetries) {
      summary->termination_type = NUMERICAL_FAILURE;
      VLOG(1) << "Too many consecutive retries; ending with numerical fail.";

      if (!options.crash_and_dump_lsqp_on_failure) {
        return;
      }

      // Dump debugging information to disk.
      CHECK(options.lsqp_dump_format_type == TEXTFILE ||
            options.lsqp_dump_format_type == PROTOBUF)
          << "Dumping the linear least squares problem on crash "
          << "requires Solver::Options::lsqp_dump_format_type to be "
          << "PROTOBUF or TEXTFILE.";

      if (DumpLinearLeastSquaresProblem(options.lsqp_dump_directory,
                                        iteration,
                                        options.lsqp_dump_format_type,
                                        jacobian.get(),
                                        muD.data(),
                                        f.data(),
                                        lm_step.data(),
                                        options.num_eliminate_blocks)) {
        LOG(FATAL) << "Linear least squares problem saved to: " 
                   << options.lsqp_dump_directory
                   << ". Please provide this to the Ceres developers for "
                   << " debugging along with the v=2 log.";
      } else {
        LOG(FATAL) << "Tried writing linear least squares problem: " 
                   << options.lsqp_dump_directory
                   << " but failed.";
      }
    }

    if (!step_is_successful) {
      // Either the step did not lead to a decrease in cost or there
      // was numerical failure. In either case we will scale mu up and
      // retry. If it was a numerical failure, we hope that the
      // stronger regularization will make the linear system better
      // conditioned. If it was numerically sane, but there was no
      // decrease in cost, then increasing mu reduces the size of the
      // trust region and we look for a decrease closer to the
      // linearization point.
      ++summary->num_unsuccessful_steps;
      mu = mu * nu;
      nu = 2 * nu;
    }

    ++iteration;

    total_cost = summary->fixed_cost + cost;

    iteration_summary.iteration = iteration;
    iteration_summary.step_is_successful = step_is_successful;
    iteration_summary.cost = total_cost;
    iteration_summary.cost_change = actual_cost_change;
    iteration_summary.gradient_max_norm = gradient_max_norm;
    iteration_summary.step_norm = step_norm;
    iteration_summary.relative_decrease = relative_decrease;
    iteration_summary.mu = mu;
    iteration_summary.eta = options.eta;
    iteration_summary.iteration_time_sec = (time(NULL) - iteration_start_time);

    if (options.logging_type >= PER_MINIMIZER_ITERATION) {
      summary->iterations.push_back(iteration_summary);
    }

    // Call the various callbacks.
    for (int i = 0; i < options.callbacks.size(); ++i) {
      if (!RunCallback(options.callbacks[i], iteration_summary, summary)) {
        return;
      }
    }
  }
}

}  // namespace internal
}  // namespace ceres
