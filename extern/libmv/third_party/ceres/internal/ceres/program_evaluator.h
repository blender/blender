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

#ifdef CERES_USE_OPENMP
#include <omp.h>
#endif

#include "ceres/parameter_block.h"
#include "ceres/program.h"
#include "ceres/residual_block.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/scoped_ptr.h"

namespace ceres {
namespace internal {

template<typename EvaluatePreparer, typename JacobianWriter>
class ProgramEvaluator : public Evaluator {
 public:
  ProgramEvaluator(const Evaluator::Options &options, Program* program)
      : options_(options),
        program_(program),
        jacobian_writer_(options, program),
        evaluate_preparers_(
            jacobian_writer_.CreateEvaluatePreparers(options.num_threads)) {
#ifndef CERES_USE_OPENMP
    CHECK_EQ(1, options_.num_threads)
        << "OpenMP support is not compiled into this binary; "
        << "only options.num_threads=1 is supported.";
#endif

    BuildResidualLayout(*program, &residual_layout_);
    evaluate_scratch_.reset(CreateEvaluatorScratch(*program,
                                                   options.num_threads));
  }

  // Implementation of Evaluator interface.
  SparseMatrix* CreateJacobian() const {
    return jacobian_writer_.CreateJacobian();
  }

  bool Evaluate(const double* state,
                double* cost,
                double* residuals,
                SparseMatrix* jacobian) {
    // The parameters are stateful, so set the state before evaluating.
    if (!program_->StateVectorToParameterBlocks(state)) {
      return false;
    }

    if (jacobian) {
      jacobian->SetZero();
    }

    // Each thread gets it's own cost and evaluate scratch space.
    for (int i = 0; i < options_.num_threads; ++i) {
      evaluate_scratch_[i].cost = 0.0;
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
      double* block_residuals = (residuals != NULL)
                              ? (residuals + residual_layout_[i])
                              : NULL;

      // Prepare block jacobians if requested.
      double** block_jacobians = NULL;
      if (jacobian != NULL) {
        preparer->Prepare(residual_block,
                          i,
                          jacobian,
                          scratch->jacobian_block_ptrs.get());
        block_jacobians = scratch->jacobian_block_ptrs.get();
      }

      // Evaluate the cost, residuals, and jacobians.
      double block_cost;
      if (!residual_block->Evaluate(&block_cost,
                                    block_residuals,
                                    block_jacobians,
                                    scratch->scratch.get())) {
        abort = true;
// This ensures that the OpenMP threads have a consistent view of 'abort'. Do
// the flush inside the failure case so that there is usually only one
// synchronization point per loop iteration instead of two.
#pragma omp flush(abort)
        continue;
      }

      scratch->cost += block_cost;

      if (jacobian != NULL) {
        jacobian_writer_.Write(i,
                               residual_layout_[i],
                               block_jacobians,
                               jacobian);
      }
    }

    if (!abort) {
      // Sum the cost from each thread.
      (*cost) = 0.0;
      for (int i = 0; i < options_.num_threads; ++i) {
        (*cost) += evaluate_scratch_[i].cost;
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

 private:
  struct EvaluateScratch {
    void Init(int max_parameters_per_residual_block,
              int max_scratch_doubles_needed_for_evaluate) {
      jacobian_block_ptrs.reset(
          new double*[max_parameters_per_residual_block]);
      scratch.reset(new double[max_scratch_doubles_needed_for_evaluate]);
    }

    double cost;
    scoped_array<double> scratch;
    scoped_array<double*> jacobian_block_ptrs;
  };

  static void BuildResidualLayout(const Program& program,
                                  vector<int>* residual_layout) {
    const vector<ResidualBlock*>& residual_blocks = program.residual_blocks();
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

    EvaluateScratch* evaluate_scratch = new EvaluateScratch[num_threads];
    for (int i = 0; i < num_threads; i++) {
      evaluate_scratch[i].Init(max_parameters_per_residual_block,
                               max_scratch_doubles_needed_for_evaluate);
    }
    return evaluate_scratch;
  }

  Evaluator::Options options_;
  Program* program_;
  JacobianWriter jacobian_writer_;
  scoped_array<EvaluatePreparer> evaluate_preparers_;
  scoped_array<EvaluateScratch> evaluate_scratch_;
  vector<int> residual_layout_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_PROGRAM_EVALUATOR_H_
