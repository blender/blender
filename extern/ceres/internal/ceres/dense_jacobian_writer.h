// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2023 Google Inc. All rights reserved.
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
// A jacobian writer that writes to dense Eigen matrices.

#ifndef CERES_INTERNAL_DENSE_JACOBIAN_WRITER_H_
#define CERES_INTERNAL_DENSE_JACOBIAN_WRITER_H_

#include <memory>

#include "ceres/casts.h"
#include "ceres/dense_sparse_matrix.h"
#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/export.h"
#include "ceres/parameter_block.h"
#include "ceres/program.h"
#include "ceres/residual_block.h"
#include "ceres/scratch_evaluate_preparer.h"

namespace ceres {
namespace internal {

class CERES_NO_EXPORT DenseJacobianWriter {
 public:
  DenseJacobianWriter(Evaluator::Options /* ignored */, Program* program)
      : program_(program) {}

  // JacobianWriter interface.

  // Since the dense matrix has different layout than that assumed by the cost
  // functions, use scratch space to store the jacobians temporarily then copy
  // them over to the larger jacobian later.
  std::unique_ptr<ScratchEvaluatePreparer[]> CreateEvaluatePreparers(
      int num_threads) {
    return ScratchEvaluatePreparer::Create(*program_, num_threads);
  }

  std::unique_ptr<SparseMatrix> CreateJacobian() const {
    return std::make_unique<DenseSparseMatrix>(
        program_->NumResiduals(), program_->NumEffectiveParameters());
  }

  void Write(int residual_id,
             int residual_offset,
             double** jacobians,
             SparseMatrix* jacobian) {
    DenseSparseMatrix* dense_jacobian = down_cast<DenseSparseMatrix*>(jacobian);
    const ResidualBlock* residual_block =
        program_->residual_blocks()[residual_id];
    const int num_parameter_blocks = residual_block->NumParameterBlocks();
    const int num_residuals = residual_block->NumResiduals();

    // Now copy the jacobians for each parameter into the dense jacobian matrix.
    for (int j = 0; j < num_parameter_blocks; ++j) {
      ParameterBlock* parameter_block = residual_block->parameter_blocks()[j];

      // If the parameter block is fixed, then there is nothing to do.
      if (parameter_block->IsConstant()) {
        continue;
      }

      const int parameter_block_size = parameter_block->TangentSize();
      ConstMatrixRef parameter_jacobian(
          jacobians[j], num_residuals, parameter_block_size);

      dense_jacobian->mutable_matrix()->block(residual_offset,
                                              parameter_block->delta_offset(),
                                              num_residuals,
                                              parameter_block_size) =
          parameter_jacobian;
    }
  }

 private:
  Program* program_;
};

}  // namespace internal
}  // namespace ceres

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_INTERNAL_DENSE_JACOBIAN_WRITER_H_
