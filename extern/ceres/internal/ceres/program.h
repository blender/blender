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

#ifndef CERES_INTERNAL_PROGRAM_H_
#define CERES_INTERNAL_PROGRAM_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ceres/evaluation_callback.h"
#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/export.h"

namespace ceres::internal {

class ParameterBlock;
class ProblemImpl;
class ResidualBlock;
class TripletSparseMatrix;
class ContextImpl;

// A nonlinear least squares optimization problem. This is different from the
// similarly-named "Problem" object, which offers a mutation interface for
// adding and modifying parameters and residuals. The Program contains the core
// part of the Problem, which is the parameters and the residuals, stored in a
// particular ordering. The ordering is critical, since it defines the mapping
// between (residual, parameter) pairs and a position in the jacobian of the
// objective function. Various parts of Ceres transform one Program into
// another; for example, the first stage of solving involves stripping all
// constant parameters and residuals. This is in contrast with Problem, which is
// not built for transformation.
class CERES_NO_EXPORT Program {
 public:
  // The ordered parameter and residual blocks for the program.
  const std::vector<ParameterBlock*>& parameter_blocks() const;
  const std::vector<ResidualBlock*>& residual_blocks() const;
  std::vector<ParameterBlock*>* mutable_parameter_blocks();
  std::vector<ResidualBlock*>* mutable_residual_blocks();
  EvaluationCallback* mutable_evaluation_callback();

  // Serialize to/from the program and update states.
  //
  // NOTE: Setting the state of a parameter block can trigger the
  // computation of the Jacobian of its manifold. If this computation fails for
  // some reason, then this method returns false and the state of the parameter
  // blocks cannot be trusted.
  bool StateVectorToParameterBlocks(const double* state);
  void ParameterBlocksToStateVector(double* state) const;

  // Copy internal state to the user's parameters.
  void CopyParameterBlockStateToUserState();

  // Set the parameter block pointers to the user pointers. Since this
  // runs parameter block set state internally, which may call manifold, this
  // can fail. False is returned on failure.
  bool SetParameterBlockStatePtrsToUserStatePtrs();

  // Update a state vector for the program given a delta.
  bool Plus(const double* state,
            const double* delta,
            double* state_plus_delta,
            ContextImpl* context,
            int num_threads) const;

  // Set the parameter indices and offsets. This permits mapping backward
  // from a ParameterBlock* to an index in the parameter_blocks() vector. For
  // any parameter block p, after calling SetParameterOffsetsAndIndex(), it
  // is true that
  //
  //   parameter_blocks()[p->index()] == p
  //
  // If a parameter appears in a residual but not in the parameter block, then
  // it will have an index of -1.
  //
  // This also updates p->state_offset() and p->delta_offset(), which are the
  // position of the parameter in the state and delta vector respectively.
  void SetParameterOffsetsAndIndex();

  // Check if the internal state of the program (the indexing and the
  // offsets) are correct.
  bool IsValid() const;

  bool ParameterBlocksAreFinite(std::string* message) const;

  // Returns true if the program has any non-constant parameter blocks
  // which have non-trivial bounds constraints.
  bool IsBoundsConstrained() const;

  // Returns false, if the program has any constant parameter blocks
  // which are not feasible, or any variable parameter blocks which
  // have a lower bound greater than or equal to the upper bound.
  bool IsFeasible(std::string* message) const;

  // Loop over each residual block and ensure that no two parameter
  // blocks in the same residual block are part of
  // parameter_blocks as that would violate the assumption that it
  // is an independent set in the Hessian matrix.
  bool IsParameterBlockSetIndependent(
      const std::set<double*>& independent_set) const;

  // Create a TripletSparseMatrix which contains the zero-one
  // structure corresponding to the block sparsity of the transpose of
  // the Jacobian matrix.
  //
  // start_residual_block which allows the user to ignore the first
  // start_residual_block residuals.
  std::unique_ptr<TripletSparseMatrix> CreateJacobianBlockSparsityTranspose(
      int start_residual_block = 0) const;

  // Create a copy of this program and removes constant parameter
  // blocks and residual blocks with no varying parameter blocks while
  // preserving their relative order.
  //
  // removed_parameter_blocks on exit will contain the list of
  // parameter blocks that were removed.
  //
  // fixed_cost will be equal to the sum of the costs of the residual
  // blocks that were removed.
  //
  // If there was a problem, then the function will return a nullptr
  // pointer and error will contain a human readable description of
  // the problem.
  std::unique_ptr<Program> CreateReducedProgram(
      std::vector<double*>* removed_parameter_blocks,
      double* fixed_cost,
      std::string* error) const;

  // See problem.h for what these do.
  int NumParameterBlocks() const;
  int NumParameters() const;
  int NumEffectiveParameters() const;
  int NumResidualBlocks() const;
  int NumResiduals() const;

  int MaxScratchDoublesNeededForEvaluate() const;
  int MaxDerivativesPerResidualBlock() const;
  int MaxParametersPerResidualBlock() const;
  int MaxResidualsPerResidualBlock() const;

  // A human-readable dump of the parameter blocks for debugging.
  // TODO(keir): If necessary, also dump the residual blocks.
  std::string ToString() const;

 private:
  // Remove constant parameter blocks and residual blocks with no
  // varying parameter blocks while preserving their relative order.
  //
  // removed_parameter_blocks on exit will contain the list of
  // parameter blocks that were removed.
  //
  // fixed_cost will be equal to the sum of the costs of the residual
  // blocks that were removed.
  //
  // If there was a problem, then the function will return false and
  // error will contain a human readable description of the problem.
  bool RemoveFixedBlocks(std::vector<double*>* removed_parameter_blocks,
                         double* fixed_cost,
                         std::string* message);

  // The Program does not own the ParameterBlock or ResidualBlock objects.
  std::vector<ParameterBlock*> parameter_blocks_;
  std::vector<ResidualBlock*> residual_blocks_;
  EvaluationCallback* evaluation_callback_ = nullptr;

  friend class ProblemImpl;
};

}  // namespace ceres::internal

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_INTERNAL_PROGRAM_H_
