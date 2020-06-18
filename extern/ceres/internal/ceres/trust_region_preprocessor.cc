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

#include "ceres/trust_region_preprocessor.h"

#include <numeric>
#include <string>

#include "ceres/callbacks.h"
#include "ceres/context_impl.h"
#include "ceres/evaluator.h"
#include "ceres/linear_solver.h"
#include "ceres/minimizer.h"
#include "ceres/parameter_block.h"
#include "ceres/preconditioner.h"
#include "ceres/preprocessor.h"
#include "ceres/problem_impl.h"
#include "ceres/program.h"
#include "ceres/reorder_program.h"
#include "ceres/suitesparse.h"
#include "ceres/trust_region_strategy.h"
#include "ceres/wall_time.h"

namespace ceres {
namespace internal {

using std::vector;

namespace {

ParameterBlockOrdering* CreateDefaultLinearSolverOrdering(
    const Program& program) {
  ParameterBlockOrdering* ordering = new ParameterBlockOrdering;
  const vector<ParameterBlock*>& parameter_blocks = program.parameter_blocks();
  for (int i = 0; i < parameter_blocks.size(); ++i) {
    ordering->AddElementToGroup(
        const_cast<double*>(parameter_blocks[i]->user_state()), 0);
  }
  return ordering;
}

// Check if all the user supplied values in the parameter blocks are
// sane or not, and if the program is feasible or not.
bool IsProgramValid(const Program& program, std::string* error) {
  return (program.ParameterBlocksAreFinite(error) && program.IsFeasible(error));
}

void AlternateLinearSolverAndPreconditionerForSchurTypeLinearSolver(
    Solver::Options* options) {
  if (!IsSchurType(options->linear_solver_type)) {
    return;
  }

  const LinearSolverType linear_solver_type_given = options->linear_solver_type;
  const PreconditionerType preconditioner_type_given =
      options->preconditioner_type;
  options->linear_solver_type =
      LinearSolver::LinearSolverForZeroEBlocks(linear_solver_type_given);

  std::string message;
  if (linear_solver_type_given == ITERATIVE_SCHUR) {
    options->preconditioner_type =
        Preconditioner::PreconditionerForZeroEBlocks(preconditioner_type_given);

    message =
        StringPrintf("No E blocks. Switching from %s(%s) to %s(%s).",
                     LinearSolverTypeToString(linear_solver_type_given),
                     PreconditionerTypeToString(preconditioner_type_given),
                     LinearSolverTypeToString(options->linear_solver_type),
                     PreconditionerTypeToString(options->preconditioner_type));
  } else {
    message =
        StringPrintf("No E blocks. Switching from %s to %s.",
                     LinearSolverTypeToString(linear_solver_type_given),
                     LinearSolverTypeToString(options->linear_solver_type));
  }

  VLOG_IF(1, options->logging_type != SILENT) << message;
}

// Reorder the program to reduce fill-in and increase cache coherency.
bool ReorderProgram(PreprocessedProblem* pp) {
  const Solver::Options& options = pp->options;
  if (IsSchurType(options.linear_solver_type)) {
    return ReorderProgramForSchurTypeLinearSolver(
        options.linear_solver_type,
        options.sparse_linear_algebra_library_type,
        pp->problem->parameter_map(),
        options.linear_solver_ordering.get(),
        pp->reduced_program.get(),
        &pp->error);
  }

  if (options.linear_solver_type == SPARSE_NORMAL_CHOLESKY &&
      !options.dynamic_sparsity) {
    return ReorderProgramForSparseCholesky(
        options.sparse_linear_algebra_library_type,
        *options.linear_solver_ordering,
        0, /* use all the rows of the jacobian */
        pp->reduced_program.get(),
        &pp->error);
  }

  if (options.linear_solver_type == CGNR &&
      options.preconditioner_type == SUBSET) {
    pp->linear_solver_options.subset_preconditioner_start_row_block =
        ReorderResidualBlocksByPartition(
            options.residual_blocks_for_subset_preconditioner,
            pp->reduced_program.get());

    return ReorderProgramForSparseCholesky(
        options.sparse_linear_algebra_library_type,
        *options.linear_solver_ordering,
        pp->linear_solver_options.subset_preconditioner_start_row_block,
        pp->reduced_program.get(),
        &pp->error);
  }

  return true;
}

// Configure and create a linear solver object. In doing so, if a
// sparse direct factorization based linear solver is being used, then
// find a fill reducing ordering and reorder the program as needed
// too.
bool SetupLinearSolver(PreprocessedProblem* pp) {
  Solver::Options& options = pp->options;
  pp->linear_solver_options = LinearSolver::Options();

  if (!options.linear_solver_ordering) {
    // If the user has not supplied a linear solver ordering, then we
    // assume that they are giving all the freedom to us in choosing
    // the best possible ordering. This intent can be indicated by
    // putting all the parameter blocks in the same elimination group.
    options.linear_solver_ordering.reset(
        CreateDefaultLinearSolverOrdering(*pp->reduced_program));
  } else {
    // If the user supplied an ordering, then check if the first
    // elimination group is still non-empty after the reduced problem
    // has been constructed.
    //
    // This is important for Schur type linear solvers, where the
    // first elimination group is special -- it needs to be an
    // independent set.
    //
    // If the first elimination group is empty, then we cannot use the
    // user's requested linear solver (and a preconditioner as the
    // case may be) so we must use a different one.
    ParameterBlockOrdering* ordering = options.linear_solver_ordering.get();
    const int min_group_id = ordering->MinNonZeroGroup();
    ordering->Remove(pp->removed_parameter_blocks);
    if (IsSchurType(options.linear_solver_type) &&
        min_group_id != ordering->MinNonZeroGroup()) {
      AlternateLinearSolverAndPreconditionerForSchurTypeLinearSolver(&options);
    }
  }

  // Reorder the program to reduce fill in and improve cache coherency
  // of the Jacobian.
  if (!ReorderProgram(pp)) {
    return false;
  }

  // Configure the linear solver.
  pp->linear_solver_options.min_num_iterations =
      options.min_linear_solver_iterations;
  pp->linear_solver_options.max_num_iterations =
      options.max_linear_solver_iterations;
  pp->linear_solver_options.type = options.linear_solver_type;
  pp->linear_solver_options.preconditioner_type = options.preconditioner_type;
  pp->linear_solver_options.visibility_clustering_type =
      options.visibility_clustering_type;
  pp->linear_solver_options.sparse_linear_algebra_library_type =
      options.sparse_linear_algebra_library_type;
  pp->linear_solver_options.dense_linear_algebra_library_type =
      options.dense_linear_algebra_library_type;
  pp->linear_solver_options.use_explicit_schur_complement =
      options.use_explicit_schur_complement;
  pp->linear_solver_options.dynamic_sparsity = options.dynamic_sparsity;
  pp->linear_solver_options.use_mixed_precision_solves =
      options.use_mixed_precision_solves;
  pp->linear_solver_options.max_num_refinement_iterations =
      options.max_num_refinement_iterations;
  pp->linear_solver_options.num_threads = options.num_threads;
  pp->linear_solver_options.use_postordering = options.use_postordering;
  pp->linear_solver_options.context = pp->problem->context();

  if (IsSchurType(pp->linear_solver_options.type)) {
    OrderingToGroupSizes(options.linear_solver_ordering.get(),
                         &pp->linear_solver_options.elimination_groups);

    // Schur type solvers expect at least two elimination groups. If
    // there is only one elimination group, then it is guaranteed that
    // this group only contains e_blocks. Thus we add a dummy
    // elimination group with zero blocks in it.
    if (pp->linear_solver_options.elimination_groups.size() == 1) {
      pp->linear_solver_options.elimination_groups.push_back(0);
    }

    if (options.linear_solver_type == SPARSE_SCHUR) {
      // When using SPARSE_SCHUR, we ignore the user's postordering
      // preferences in certain cases.
      //
      // 1. SUITE_SPARSE is the sparse linear algebra library requested
      //    but cholmod_camd is not available.
      // 2. CX_SPARSE is the sparse linear algebra library requested.
      //
      // This ensures that the linear solver does not assume that a
      // fill-reducing pre-ordering has been done.
      //
      // TODO(sameeragarwal): Implement the reordering of parameter
      // blocks for CX_SPARSE.
      if ((options.sparse_linear_algebra_library_type == SUITE_SPARSE &&
           !SuiteSparse::
               IsConstrainedApproximateMinimumDegreeOrderingAvailable()) ||
          (options.sparse_linear_algebra_library_type == CX_SPARSE)) {
        pp->linear_solver_options.use_postordering = true;
      }
    }
  }

  pp->linear_solver.reset(LinearSolver::Create(pp->linear_solver_options));
  return (pp->linear_solver != nullptr);
}

// Configure and create the evaluator.
bool SetupEvaluator(PreprocessedProblem* pp) {
  const Solver::Options& options = pp->options;
  pp->evaluator_options = Evaluator::Options();
  pp->evaluator_options.linear_solver_type = options.linear_solver_type;
  pp->evaluator_options.num_eliminate_blocks = 0;
  if (IsSchurType(options.linear_solver_type)) {
    pp->evaluator_options.num_eliminate_blocks =
        options.linear_solver_ordering->group_to_elements()
            .begin()
            ->second.size();
  }

  pp->evaluator_options.num_threads = options.num_threads;
  pp->evaluator_options.dynamic_sparsity = options.dynamic_sparsity;
  pp->evaluator_options.context = pp->problem->context();
  pp->evaluator_options.evaluation_callback =
      pp->reduced_program->mutable_evaluation_callback();
  pp->evaluator.reset(Evaluator::Create(
      pp->evaluator_options, pp->reduced_program.get(), &pp->error));

  return (pp->evaluator != nullptr);
}

// If the user requested inner iterations, then find an inner
// iteration ordering as needed and configure and create a
// CoordinateDescentMinimizer object to perform the inner iterations.
bool SetupInnerIterationMinimizer(PreprocessedProblem* pp) {
  Solver::Options& options = pp->options;
  if (!options.use_inner_iterations) {
    return true;
  }

  if (pp->reduced_program->mutable_evaluation_callback()) {
    pp->error = "Inner iterations cannot be used with EvaluationCallbacks";
    return false;
  }

  // With just one parameter block, the outer iteration of the trust
  // region method and inner iterations are doing exactly the same
  // thing, and thus inner iterations are not needed.
  if (pp->reduced_program->NumParameterBlocks() == 1) {
    LOG(WARNING) << "Reduced problem only contains one parameter block."
                 << "Disabling inner iterations.";
    return true;
  }

  if (options.inner_iteration_ordering != nullptr) {
    // If the user supplied an ordering, then remove the set of
    // inactive parameter blocks from it
    options.inner_iteration_ordering->Remove(pp->removed_parameter_blocks);
    if (options.inner_iteration_ordering->NumElements() == 0) {
      LOG(WARNING) << "No remaining elements in the inner iteration ordering.";
      return true;
    }

    // Validate the reduced ordering.
    if (!CoordinateDescentMinimizer::IsOrderingValid(
            *pp->reduced_program,
            *options.inner_iteration_ordering,
            &pp->error)) {
      return false;
    }
  } else {
    // The user did not supply an ordering, so create one.
    options.inner_iteration_ordering.reset(
        CoordinateDescentMinimizer::CreateOrdering(*pp->reduced_program));
  }

  pp->inner_iteration_minimizer.reset(
      new CoordinateDescentMinimizer(pp->problem->context()));
  return pp->inner_iteration_minimizer->Init(*pp->reduced_program,
                                             pp->problem->parameter_map(),
                                             *options.inner_iteration_ordering,
                                             &pp->error);
}

// Configure and create a TrustRegionMinimizer object.
void SetupMinimizerOptions(PreprocessedProblem* pp) {
  const Solver::Options& options = pp->options;

  SetupCommonMinimizerOptions(pp);
  pp->minimizer_options.is_constrained =
      pp->reduced_program->IsBoundsConstrained();
  pp->minimizer_options.jacobian.reset(pp->evaluator->CreateJacobian());
  pp->minimizer_options.inner_iteration_minimizer =
      pp->inner_iteration_minimizer;

  TrustRegionStrategy::Options strategy_options;
  strategy_options.linear_solver = pp->linear_solver.get();
  strategy_options.initial_radius = options.initial_trust_region_radius;
  strategy_options.max_radius = options.max_trust_region_radius;
  strategy_options.min_lm_diagonal = options.min_lm_diagonal;
  strategy_options.max_lm_diagonal = options.max_lm_diagonal;
  strategy_options.trust_region_strategy_type =
      options.trust_region_strategy_type;
  strategy_options.dogleg_type = options.dogleg_type;
  pp->minimizer_options.trust_region_strategy.reset(
      TrustRegionStrategy::Create(strategy_options));
  CHECK(pp->minimizer_options.trust_region_strategy != nullptr);
}

}  // namespace

TrustRegionPreprocessor::~TrustRegionPreprocessor() {}

bool TrustRegionPreprocessor::Preprocess(const Solver::Options& options,
                                         ProblemImpl* problem,
                                         PreprocessedProblem* pp) {
  CHECK(pp != nullptr);
  pp->options = options;
  ChangeNumThreadsIfNeeded(&pp->options);

  pp->problem = problem;
  Program* program = problem->mutable_program();
  if (!IsProgramValid(*program, &pp->error)) {
    return false;
  }

  pp->reduced_program.reset(program->CreateReducedProgram(
      &pp->removed_parameter_blocks, &pp->fixed_cost, &pp->error));

  if (pp->reduced_program.get() == NULL) {
    return false;
  }

  if (pp->reduced_program->NumParameterBlocks() == 0) {
    // The reduced problem has no parameter or residual blocks. There
    // is nothing more to do.
    return true;
  }

  if (!SetupLinearSolver(pp) || !SetupEvaluator(pp) ||
      !SetupInnerIterationMinimizer(pp)) {
    return false;
  }

  SetupMinimizerOptions(pp);
  return true;
}

}  // namespace internal
}  // namespace ceres
