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
//         keir@google.com (Keir Mierle)
//
// Purpose : Class and struct definitions for parameter and residual blocks.

#ifndef CERES_INTERNAL_RESIDUAL_BLOCK_H_
#define CERES_INTERNAL_RESIDUAL_BLOCK_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ceres/cost_function.h"
#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/export.h"
#include "ceres/stringprintf.h"
#include "ceres/types.h"

namespace ceres {

class LossFunction;

namespace internal {

class ParameterBlock;

// A term in the least squares problem. The mathematical form of each term in
// the overall least-squares cost function is:
//
//    1
//   --- loss_function( || cost_function(block1, block2, ...) ||^2  ),
//    2
//
// Storing the cost function and the loss function separately permits optimizing
// the problem with standard non-linear least techniques, without requiring a
// more general non-linear solver.
//
// The residual block stores pointers to but does not own the cost functions,
// loss functions, and parameter blocks.
class CERES_NO_EXPORT ResidualBlock {
 public:
  // Construct the residual block with the given cost/loss functions. Loss may
  // be null. The index is the index of the residual block in the Program's
  // residual_blocks array.
  ResidualBlock(const CostFunction* cost_function,
                const LossFunction* loss_function,
                const std::vector<ParameterBlock*>& parameter_blocks,
                int index);

  // Evaluates the residual term, storing the scalar cost in *cost, the residual
  // components in *residuals, and the jacobians between the parameters and
  // residuals in jacobians[i], in row-major order. If residuals is nullptr, the
  // residuals are not computed. If jacobians is nullptr, no jacobians are
  // computed. If jacobians[i] is nullptr, then the jacobian for that parameter
  // is not computed.
  //
  // cost must not be null.
  //
  // Evaluate needs scratch space which must be supplied by the caller via
  // scratch. The array should have at least NumScratchDoublesForEvaluate()
  // space available.
  //
  // The return value indicates the success or failure. If the function returns
  // false, the caller should expect the output memory locations to have
  // been modified.
  //
  // The returned cost and jacobians have had robustification and manifold
  // projection applied already; for example, the jacobian for a 4-dimensional
  // quaternion parameter using the "Quaternion" manifold is num_residuals by 3
  // instead of num_residuals by 4.
  //
  // apply_loss_function as the name implies allows the user to switch
  // the application of the loss function on and off.
  bool Evaluate(bool apply_loss_function,
                double* cost,
                double* residuals,
                double** jacobians,
                double* scratch) const;

  const CostFunction* cost_function() const { return cost_function_; }
  const LossFunction* loss_function() const { return loss_function_; }

  // Access the parameter blocks for this residual. The array has size
  // NumParameterBlocks().
  ParameterBlock* const* parameter_blocks() const {
    return parameter_blocks_.get();
  }

  // Number of variable blocks that this residual term depends on.
  int NumParameterBlocks() const {
    return cost_function_->parameter_block_sizes().size();
  }

  // The size of the residual vector returned by this residual function.
  int NumResiduals() const { return cost_function_->num_residuals(); }

  // The minimum amount of scratch space needed to pass to Evaluate().
  int NumScratchDoublesForEvaluate() const;

  // This residual block's index in an array.
  int index() const { return index_; }
  void set_index(int index) { index_ = index; }

  std::string ToString() const {
    return StringPrintf("{residual block; index=%d}", index_);
  }

 private:
  const CostFunction* cost_function_;
  const LossFunction* loss_function_;
  std::unique_ptr<ParameterBlock*[]> parameter_blocks_;

  // The index of the residual, typically in a Program. This is only to permit
  // switching from a ResidualBlock* to an index in the Program's array, needed
  // to do efficient removals.
  int32_t index_;
};

}  // namespace internal
}  // namespace ceres

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_INTERNAL_RESIDUAL_BLOCK_H_
