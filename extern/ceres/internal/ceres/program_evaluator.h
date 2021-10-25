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
// Author: keir@google.com (Keir Mierle)
//
// The ProgramEvaluator runs the cost functions contained in each residual block
// and stores the result into a jacobian. The particular type of jacobian is
// abstracted out using two template parameters:
//
//   - An "EvaluatePreparer" that is responsible for creating the array with
//     pointers to the jacobian blocks where the cost function evaluates to.
//   - A "JacobianWriter" that is responsible for storing the resulting
//     jacobian blocks in the passed sparse matrix.
//
// This abstraction affords an efficient evaluator implementation while still
// supporting writing to multiple sparse matrix formats. For example, when the
// ProgramEvaluator is parameterized for writing to block sparse matrices, the
// residual jacobians are written directly into their final position in the
// block sparse matrix by the user's CostFunction; there is no copying.
//
// The evaluation is threaded with OpenMP.
//
// The EvaluatePreparer and JacobianWriter interfaces are as follows:
//
//   class EvaluatePreparer {
//     // Prepare the jacobians array for use as the destination of a call to
//     // a cost function's evaluate method.
//     void Prepare(const ResidualBlock* residual_block,
//                  int residual_block_index,
//                  SparseMatrix* jacobian,
//                  double** jacobians);
//   }
//
//   class JacobianWriter {
//     // Create a jacobian that this writer can write. Same as
//     // Evaluator::CreateJacobian.
//     SparseMatrix* CreateJacobian() const;
//
//     // Create num_threads evaluate preparers. Caller owns result which must
//     // be freed with delete[]. Resulting preparers are valid while *this is.
//     EvaluatePreparer* CreateEvaluatePreparers(int num_threads);
//
//     // Write the block jacobians from a residual block evaluation to the
//     // larger sparse jacobian.
//     void Write(int residual_id,
//                int residual_offset,
//                double** jacobians,
//                SparseMatrix* jacobian);
//   }
//
// Note: The ProgramEvaluator is not thread safe, since internally it maintains
// some per-thread scratch space.

#ifndef CERES_INTERNAL_PROGRAM_EVALUATOR_H_
#define CERES_INTERNAL_PROGRAM_EVALUATOR_H_

// This include must come before any #ifndef check on Ceres compile options.
#include "ceres/internal/port.h"

#ifdef CERES_USE_OPENMP
#include <omp.h>
#endif

#include <map>
#include <string>
#include <vector>
#include "ceres/execution_summary.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/parameter_block.h"
#include "ceres/program.h"
#include "ceres/residual_block.h"
#include "ceres/small_blas.h"

namespace ceres {
namespace internal {

struct NullJacobianFinalizer {
  void operator()(SparseMatrix* jacobian, int num_parameters) {}
};

template<typename EvaluatePreparer,
         typename JacobianWriter,
         typename JacobianFinalizer = NullJacobianFinalizer>
class ProgramEvaluator : public Evaluator {
 public:
  ProgramEvaluator(const Evaluator::Options &options, Program* program)
      : options_(options),
        program_(program),
        jacobian_writer_(options, program),
        evaluate_preparers_(
            jacobian_writer_.CreateEvaluatePreparers(options.num_threads)) {
#ifndef CERES_USE_OPENMP
    if (options_.num_threads > 1) {
      LOG(WARNING)
          << "OpenMP support is not compiled into this binary; "
          << "only options.num_threads = 1 is supported. Switching "
          << "to single threaded mode.";
      options_.num_threads = 1;
    }
#endif

    BuildResidualLayout(*program, &residual_layout_);
    evaluate_scratch_.reset(CreateEvaluatorScratch(*program,
                                                   options.num_threads));
  }

  // Implementation of Evaluator interface.
  SparseMatrix* CreateJacobian() const {
    return jacobian_writer_.CreateJacobian();
  }

  bool Evaluate(const Evaluator::EvaluateOptions& evaluate_options,
                const double* state,
                double* cost,
                double* residuals,
                double* gradient,
                SparseMatrix* jacobian) {
    ScopedExecutionTimer total_timer("Evaluator::Total", &execution_summary_);
    ScopedExecutionTimer call_type_timer(gradient == NULL && jacobian == NULL
                                         ? "Evaluator::Residual"
                                         : "Evaluator::Jacobian",
                                         &execution_summary_);

    // The parameters are stateful, so set the state before evaluating.
    if (!program_->StateVectorToParameterBlocks(state)) {
      return false;
    }

    if (residuals != NULL) {
      VectorRef(residuals, program_->NumResiduals()).setZero();
    }

    if (jacobian != NULL) {
      jacobian->SetZero();
    }

    // Each thread gets it's own cost and evaluate scratch space.
    for (int i = 0; i < options_.num_threads; ++i) {
      evaluate_scratch_[i].cost = 0.0;
      if (gradient != NULL) {
        VectorRef(evaluate_scratch_[i].gradient.get(),
                  program_->NumEffectiveParameters()).setZero();
      }
    }

    // This bool is used to disable the loop if an error is encountered
    // without breaking out of it. The remaining loop iterations are still run,
    // but with an empty body, and so will finish quickly.
    bool abort = false;
    int num_residual_blocks = program_->NumResidualBlocks();
#pragma omp parallel for num_threads(options_.num_threads)
    for (int i = 0; i < num_residual_blocks; ++i) {
// Disable the loop instead of breaking, as required by OpenMP.
#pragma omp flush(abort)
      if (abort) {
        continue;
      }

#ifdef CERES_USE_OPENMP
      int thread_id = omp_get_thread_num();
#else
      int thread_id = 0;
#endif
      EvaluatePreparer* preparer = &evaluate_preparers_[thread_id];
      EvaluateScratch* scratch = &evaluate_scratch_[thread_id];

      // Prepare block residuals if requested.
      const ResidualBlock* residual_block = program_->residual_blocks()[i];
      double* block_residuals = NULL;
      if (residuals != NULL) {
        block_residuals = residuals + residual_layout_[i];
      } else if (gradient != NULL) {
        block_residuals = scratch->residual_block_residuals.get();
      }

      // Prepare block jacobians if requested.
      double** block_jacobians = NULL;
      if (jacobian != NULL || gradient != NULL) {
        preparer->Prepare(residual_block,
                          i,
                          jacobian,
                          scratch->jacobian_block_ptrs.get());
        block_jacobians = scratch->jacobian_block_ptrs.get();
      }

      // Evaluate the cost, residuals, and jacobians.
      double block_cost;
      if (!residual_block->Evaluate(
              evaluate_options.apply_loss_function,
              &block_cost,
              block_residuals,
              block_jacobians,
              scratch->residual_block_evaluate_scratch.get())) {
        abort = true;
// This ensures that the OpenMP threads have a consistent view of 'abort'. Do
// the flush inside the failure case so that there is usually only one
// synchronization point per loop iteration instead of two.
#pragma omp flush(abort)
        continue;
      }

      scratch->cost += block_cost;

      // Store the jacobians, if they were requested.
      if (jacobian != NULL) {
        jacobian_writer_.Write(i,
                               residual_layout_[i],
                               block_jacobians,
                               jacobian);
      }

      // Compute and store the gradient, if it was requested.
      if (gradient != NULL) {
        int num_residuals = residual_block->NumResiduals();
        int num_parameter_blocks = residual_block->NumParameterBlocks();
        for (int j = 0; j < num_parameter_blocks; ++j) {
          const ParameterBlock* parameter_block =
              residual_block->parameter_blocks()[j];
          if (parameter_block->IsConstant()) {
            continue;
          }

          MatrixTransposeVectorMultiply<Eigen::Dynamic, Eigen::Dynamic, 1>(
              block_jacobians[j],
              num_residuals,
              parameter_block->LocalSize(),
              block_residuals,
              scratch->gradient.get() + parameter_block->delta_offset());
        }
      }
    }

    if (!abort) {
      const int num_parameters = program_->NumEffectiveParameters();

      // Sum the cost and gradient (if requested) from each thread.
      (*cost) = 0.0;
      if (gradient != NULL) {
        VectorRef(gradient, num_parameters).setZero();
      }
      for (int i = 0; i < options_.num_threads; ++i) {
        (*cost) += evaluate_scratch_[i].cost;
        if (gradient != NULL) {
          VectorRef(gradient, num_parameters) +=
              VectorRef(evaluate_scratch_[i].gradient.get(), num_parameters);
        }
      }

      // Finalize the Jacobian if it is available.
      // `num_parameters` is passed to the finalizer so that additional
      // storage can be reserved for additional diagonal elements if
      // necessary.
      if (jacobian != NULL) {
        JacobianFinalizer f;
        f(jacobian, num_parameters);
      }
    }
    return !abort;
  }

  bool Plus(const double* state,
            const double* delta,
            double* state_plus_delta) const {
    return program_->Plus(state, delta, state_plus_delta);
  }

  int NumParameters() const {
    return program_->NumParameters();
  }
  int NumEffectiveParameters() const {
    return program_->NumEffectiveParameters();
  }

  int NumResiduals() const {
    return program_->NumResiduals();
  }

  virtual std::map<std::string, int> CallStatistics() const {
    return execution_summary_.calls();
  }

  virtual std::map<std::string, double> TimeStatistics() const {
    return execution_summary_.times();
  }

 private:
  // Per-thread scratch space needed to evaluate and store each residual block.
  struct EvaluateScratch {
    void Init(int max_parameters_per_residual_block,
              int max_scratch_doubles_needed_for_evaluate,
              int max_residuals_per_residual_block,
              int num_parameters) {
      residual_block_evaluate_scratch.reset(
          new double[max_scratch_doubles_needed_for_evaluate]);
      gradient.reset(new double[num_parameters]);
      VectorRef(gradient.get(), num_parameters).setZero();
      residual_block_residuals.reset(
          new double[max_residuals_per_residual_block]);
      jacobian_block_ptrs.reset(
          new double*[max_parameters_per_residual_block]);
    }

    double cost;
    scoped_array<double> residual_block_evaluate_scratch;
    // The gradient in the local parameterization.
    scoped_array<double> gradient;
    // Enough space to store the residual for the largest residual block.
    scoped_array<double> residual_block_residuals;
    scoped_array<double*> jacobian_block_ptrs;
  };

  static void BuildResidualLayout(const Program& program,
                                  std::vector<int>* residual_layout) {
    const std::vector<ResidualBlock*>& residual_blocks =
        program.residual_blocks();
    residual_layout->resize(program.NumResidualBlocks());
    int residual_pos = 0;
    for (int i = 0; i < residual_blocks.size(); ++i) {
      const int num_residuals = residual_blocks[i]->NumResiduals();
      (*residual_layout)[i] = residual_pos;
      residual_pos += num_residuals;
    }
  }

  // Create scratch space for each thread evaluating the program.
  static EvaluateScratch* CreateEvaluatorScratch(const Program& program,
                                                 int num_threads) {
    int max_parameters_per_residual_block =
        program.MaxParametersPerResidualBlock();
    int max_scratch_doubles_needed_for_evaluate =
        program.MaxScratchDoublesNeededForEvaluate();
    int max_residuals_per_residual_block =
        program.MaxResidualsPerResidualBlock();
    int num_parameters = program.NumEffectiveParameters();

    EvaluateScratch* evaluate_scratch = new EvaluateScratch[num_threads];
    for (int i = 0; i < num_threads; i++) {
      evaluate_scratch[i].Init(max_parameters_per_residual_block,
                               max_scratch_doubles_needed_for_evaluate,
                               max_residuals_per_residual_block,
                               num_parameters);
    }
    return evaluate_scratch;
  }

  Evaluator::Options options_;
  Program* program_;
  JacobianWriter jacobian_writer_;
  scoped_array<EvaluatePreparer> evaluate_preparers_;
  scoped_array<EvaluateScratch> evaluate_scratch_;
  std::vector<int> residual_layout_;
  ::ceres::internal::ExecutionSummary execution_summary_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_PROGRAM_EVALUATOR_H_
