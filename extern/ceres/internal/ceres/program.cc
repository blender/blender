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

#include "ceres/program.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ceres/array_utils.h"
#include "ceres/casts.h"
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/cost_function.h"
#include "ceres/evaluator.h"
#include "ceres/internal/export.h"
#include "ceres/loss_function.h"
#include "ceres/manifold.h"
#include "ceres/map_util.h"
#include "ceres/parallel_for.h"
#include "ceres/parameter_block.h"
#include "ceres/problem.h"
#include "ceres/residual_block.h"
#include "ceres/stl_util.h"
#include "ceres/triplet_sparse_matrix.h"

namespace ceres::internal {

const std::vector<ParameterBlock*>& Program::parameter_blocks() const {
  return parameter_blocks_;
}

const std::vector<ResidualBlock*>& Program::residual_blocks() const {
  return residual_blocks_;
}

std::vector<ParameterBlock*>* Program::mutable_parameter_blocks() {
  return &parameter_blocks_;
}

std::vector<ResidualBlock*>* Program::mutable_residual_blocks() {
  return &residual_blocks_;
}

EvaluationCallback* Program::mutable_evaluation_callback() {
  return evaluation_callback_;
}

bool Program::StateVectorToParameterBlocks(const double* state) {
  for (auto* parameter_block : parameter_blocks_) {
    if (!parameter_block->IsConstant() && !parameter_block->SetState(state)) {
      return false;
    }
    state += parameter_block->Size();
  }
  return true;
}

void Program::ParameterBlocksToStateVector(double* state) const {
  for (auto* parameter_block : parameter_blocks_) {
    parameter_block->GetState(state);
    state += parameter_block->Size();
  }
}

void Program::CopyParameterBlockStateToUserState() {
  for (auto* parameter_block : parameter_blocks_) {
    parameter_block->GetState(parameter_block->mutable_user_state());
  }
}

bool Program::SetParameterBlockStatePtrsToUserStatePtrs() {
  for (auto* parameter_block : parameter_blocks_) {
    if (!parameter_block->IsConstant() &&
        !parameter_block->SetState(parameter_block->user_state())) {
      return false;
    }
  }
  return true;
}

bool Program::Plus(const double* state,
                   const double* delta,
                   double* state_plus_delta,
                   ContextImpl* context,
                   int num_threads) const {
  std::atomic<bool> abort(false);
  auto* parameter_blocks = parameter_blocks_.data();
  ParallelFor(
      context,
      0,
      parameter_blocks_.size(),
      num_threads,
      [&abort, state, delta, state_plus_delta, parameter_blocks](int block_id) {
        if (abort) {
          return;
        }
        auto parameter_block = parameter_blocks[block_id];

        auto block_state = state + parameter_block->state_offset();
        auto block_delta = delta + parameter_block->delta_offset();
        auto block_state_plus_delta =
            state_plus_delta + parameter_block->state_offset();
        if (!parameter_block->Plus(
                block_state, block_delta, block_state_plus_delta)) {
          abort = true;
        }
      });
  return abort == false;
}

void Program::SetParameterOffsetsAndIndex() {
  // Set positions for all parameters appearing as arguments to residuals to one
  // past the end of the parameter block array.
  for (auto* residual_block : residual_blocks_) {
    for (int j = 0; j < residual_block->NumParameterBlocks(); ++j) {
      residual_block->parameter_blocks()[j]->set_index(-1);
    }
  }
  // For parameters that appear in the program, set their position and offset.
  int state_offset = 0;
  int delta_offset = 0;
  for (int i = 0; i < parameter_blocks_.size(); ++i) {
    parameter_blocks_[i]->set_index(i);
    parameter_blocks_[i]->set_state_offset(state_offset);
    parameter_blocks_[i]->set_delta_offset(delta_offset);
    state_offset += parameter_blocks_[i]->Size();
    delta_offset += parameter_blocks_[i]->TangentSize();
  }
}

bool Program::IsValid() const {
  for (int i = 0; i < residual_blocks_.size(); ++i) {
    const ResidualBlock* residual_block = residual_blocks_[i];
    if (residual_block->index() != i) {
      LOG(WARNING) << "Residual block: " << i
                   << " has incorrect index: " << residual_block->index();
      return false;
    }
  }

  int state_offset = 0;
  int delta_offset = 0;
  for (int i = 0; i < parameter_blocks_.size(); ++i) {
    const ParameterBlock* parameter_block = parameter_blocks_[i];
    if (parameter_block->index() != i ||
        parameter_block->state_offset() != state_offset ||
        parameter_block->delta_offset() != delta_offset) {
      LOG(WARNING) << "Parameter block: " << i
                   << "has incorrect indexing information: "
                   << parameter_block->ToString();
      return false;
    }

    state_offset += parameter_blocks_[i]->Size();
    delta_offset += parameter_blocks_[i]->TangentSize();
  }

  return true;
}

bool Program::ParameterBlocksAreFinite(std::string* message) const {
  CHECK(message != nullptr);
  for (auto* parameter_block : parameter_blocks_) {
    const double* array = parameter_block->user_state();
    const int size = parameter_block->Size();
    const int invalid_index = FindInvalidValue(size, array);
    if (invalid_index != size) {
      *message = StringPrintf(
          "ParameterBlock: %p with size %d has at least one invalid value.\n"
          "First invalid value is at index: %d.\n"
          "Parameter block values: ",
          array,
          size,
          invalid_index);
      AppendArrayToString(size, array, message);
      return false;
    }
  }
  return true;
}

bool Program::IsBoundsConstrained() const {
  for (auto* parameter_block : parameter_blocks_) {
    if (parameter_block->IsConstant()) {
      continue;
    }
    const int size = parameter_block->Size();
    for (int j = 0; j < size; ++j) {
      const double lower_bound = parameter_block->LowerBoundForParameter(j);
      const double upper_bound = parameter_block->UpperBoundForParameter(j);
      if (lower_bound > -std::numeric_limits<double>::max() ||
          upper_bound < std::numeric_limits<double>::max()) {
        return true;
      }
    }
  }
  return false;
}

bool Program::IsFeasible(std::string* message) const {
  CHECK(message != nullptr);
  for (auto* parameter_block : parameter_blocks_) {
    const double* parameters = parameter_block->user_state();
    const int size = parameter_block->Size();
    if (parameter_block->IsConstant()) {
      // Constant parameter blocks must start in the feasible region
      // to ultimately produce a feasible solution, since Ceres cannot
      // change them.
      for (int j = 0; j < size; ++j) {
        const double lower_bound = parameter_block->LowerBoundForParameter(j);
        const double upper_bound = parameter_block->UpperBoundForParameter(j);
        if (parameters[j] < lower_bound || parameters[j] > upper_bound) {
          *message = StringPrintf(
              "ParameterBlock: %p with size %d has at least one infeasible "
              "value."
              "\nFirst infeasible value is at index: %d."
              "\nLower bound: %e, value: %e, upper bound: %e"
              "\nParameter block values: ",
              parameters,
              size,
              j,
              lower_bound,
              parameters[j],
              upper_bound);
          AppendArrayToString(size, parameters, message);
          return false;
        }
      }
    } else {
      // Variable parameter blocks must have non-empty feasible
      // regions, otherwise there is no way to produce a feasible
      // solution.
      for (int j = 0; j < size; ++j) {
        const double lower_bound = parameter_block->LowerBoundForParameter(j);
        const double upper_bound = parameter_block->UpperBoundForParameter(j);
        if (lower_bound >= upper_bound) {
          *message = StringPrintf(
              "ParameterBlock: %p with size %d has at least one infeasible "
              "bound."
              "\nFirst infeasible bound is at index: %d."
              "\nLower bound: %e, upper bound: %e"
              "\nParameter block values: ",
              parameters,
              size,
              j,
              lower_bound,
              upper_bound);
          AppendArrayToString(size, parameters, message);
          return false;
        }
      }
    }
  }

  return true;
}

std::unique_ptr<Program> Program::CreateReducedProgram(
    std::vector<double*>* removed_parameter_blocks,
    double* fixed_cost,
    std::string* error) const {
  CHECK(removed_parameter_blocks != nullptr);
  CHECK(fixed_cost != nullptr);
  CHECK(error != nullptr);

  std::unique_ptr<Program> reduced_program = std::make_unique<Program>(*this);
  if (!reduced_program->RemoveFixedBlocks(
          removed_parameter_blocks, fixed_cost, error)) {
    return nullptr;
  }

  reduced_program->SetParameterOffsetsAndIndex();
  return reduced_program;
}

bool Program::RemoveFixedBlocks(std::vector<double*>* removed_parameter_blocks,
                                double* fixed_cost,
                                std::string* error) {
  CHECK(removed_parameter_blocks != nullptr);
  CHECK(fixed_cost != nullptr);
  CHECK(error != nullptr);

  std::unique_ptr<double[]> residual_block_evaluate_scratch;
  residual_block_evaluate_scratch =
      std::make_unique<double[]>(MaxScratchDoublesNeededForEvaluate());
  *fixed_cost = 0.0;

  bool need_to_call_prepare_for_evaluation = evaluation_callback_ != nullptr;

  // Mark all the parameters as unused. Abuse the index member of the
  // parameter blocks for the marking.
  for (auto* parameter_block : parameter_blocks_) {
    parameter_block->set_index(-1);
  }

  // Filter out residual that have all-constant parameters, and mark
  // all the parameter blocks that appear in residuals.
  int num_active_residual_blocks = 0;
  for (int i = 0; i < residual_blocks_.size(); ++i) {
    ResidualBlock* residual_block = residual_blocks_[i];
    int num_parameter_blocks = residual_block->NumParameterBlocks();

    // Determine if the residual block is fixed, and also mark varying
    // parameters that appear in the residual block.
    bool all_constant = true;
    for (int k = 0; k < num_parameter_blocks; k++) {
      ParameterBlock* parameter_block = residual_block->parameter_blocks()[k];
      if (!parameter_block->IsConstant()) {
        all_constant = false;
        parameter_block->set_index(1);
      }
    }

    if (!all_constant) {
      residual_blocks_[num_active_residual_blocks++] = residual_block;
      continue;
    }

    // This is an exceedingly rare case, where the user has residual
    // blocks which are effectively constant but they are also
    // performance sensitive enough to add an EvaluationCallback.
    //
    // In this case before we evaluate the cost of the constant
    // residual blocks, we must call
    // EvaluationCallback::PrepareForEvaluation(). Because this call
    // can be costly, we only call this if we actually encounter a
    // residual block with all constant parameter blocks.
    //
    // It is worth nothing that there is a minor inefficiency here,
    // that the iteration 0 of TrustRegionMinimizer will also cause
    // PrepareForEvaluation to be called on the same point, but with
    // evaluate_jacobians = true. We could try and optimize this here,
    // but given the rarity of this case, the additional complexity
    // and long range dependency is not worth it.
    if (need_to_call_prepare_for_evaluation) {
      constexpr bool kNewPoint = true;
      constexpr bool kDoNotEvaluateJacobians = false;
      evaluation_callback_->PrepareForEvaluation(kDoNotEvaluateJacobians,
                                                 kNewPoint);
      need_to_call_prepare_for_evaluation = false;
    }

    // The residual is constant and will be removed, so its cost is
    // added to the variable fixed_cost.
    double cost = 0.0;
    if (!residual_block->Evaluate(true,
                                  &cost,
                                  nullptr,
                                  nullptr,
                                  residual_block_evaluate_scratch.get())) {
      *error = StringPrintf(
          "Evaluation of the residual %d failed during "
          "removal of fixed residual blocks.",
          i);
      return false;
    }

    *fixed_cost += cost;
  }
  residual_blocks_.resize(num_active_residual_blocks);

  // Filter out unused or fixed parameter blocks.
  int num_active_parameter_blocks = 0;
  removed_parameter_blocks->clear();
  for (auto* parameter_block : parameter_blocks_) {
    if (parameter_block->index() == -1) {
      removed_parameter_blocks->push_back(
          parameter_block->mutable_user_state());
    } else {
      parameter_blocks_[num_active_parameter_blocks++] = parameter_block;
    }
  }
  parameter_blocks_.resize(num_active_parameter_blocks);

  if (!(((NumResidualBlocks() == 0) && (NumParameterBlocks() == 0)) ||
        ((NumResidualBlocks() != 0) && (NumParameterBlocks() != 0)))) {
    *error = "Congratulations, you found a bug in Ceres. Please report it.";
    return false;
  }

  return true;
}

bool Program::IsParameterBlockSetIndependent(
    const std::set<double*>& independent_set) const {
  // Loop over each residual block and ensure that no two parameter
  // blocks in the same residual block are part of
  // parameter_block_ptrs as that would violate the assumption that it
  // is an independent set in the Hessian matrix.
  for (const ResidualBlock* residual_block : residual_blocks_) {
    ParameterBlock* const* parameter_blocks =
        residual_block->parameter_blocks();
    const int num_parameter_blocks = residual_block->NumParameterBlocks();
    int count = 0;
    for (int i = 0; i < num_parameter_blocks; ++i) {
      count += independent_set.count(parameter_blocks[i]->mutable_user_state());
    }
    if (count > 1) {
      return false;
    }
  }
  return true;
}

std::unique_ptr<TripletSparseMatrix>
Program::CreateJacobianBlockSparsityTranspose(int start_residual_block) const {
  // Matrix to store the block sparsity structure of the Jacobian.
  const int num_rows = NumParameterBlocks();
  const int num_cols = NumResidualBlocks() - start_residual_block;

  std::unique_ptr<TripletSparseMatrix> tsm(
      new TripletSparseMatrix(num_rows, num_cols, 10 * num_cols));
  int num_nonzeros = 0;
  int* rows = tsm->mutable_rows();
  int* cols = tsm->mutable_cols();
  double* values = tsm->mutable_values();

  for (int c = start_residual_block; c < residual_blocks_.size(); ++c) {
    const ResidualBlock* residual_block = residual_blocks_[c];
    const int num_parameter_blocks = residual_block->NumParameterBlocks();
    ParameterBlock* const* parameter_blocks =
        residual_block->parameter_blocks();

    for (int j = 0; j < num_parameter_blocks; ++j) {
      if (parameter_blocks[j]->IsConstant()) {
        continue;
      }

      // Re-size the matrix if needed.
      if (num_nonzeros >= tsm->max_num_nonzeros()) {
        tsm->set_num_nonzeros(num_nonzeros);
        tsm->Reserve(2 * num_nonzeros);
        rows = tsm->mutable_rows();
        cols = tsm->mutable_cols();
        values = tsm->mutable_values();
      }

      const int r = parameter_blocks[j]->index();
      rows[num_nonzeros] = r;
      cols[num_nonzeros] = c - start_residual_block;
      values[num_nonzeros] = 1.0;
      ++num_nonzeros;
    }
  }

  tsm->set_num_nonzeros(num_nonzeros);
  return tsm;
}

int Program::NumResidualBlocks() const { return residual_blocks_.size(); }

int Program::NumParameterBlocks() const { return parameter_blocks_.size(); }

int Program::NumResiduals() const {
  int num_residuals = 0;
  for (auto* residual_block : residual_blocks_) {
    num_residuals += residual_block->NumResiduals();
  }
  return num_residuals;
}

int Program::NumParameters() const {
  int num_parameters = 0;
  for (auto* parameter_block : parameter_blocks_) {
    num_parameters += parameter_block->Size();
  }
  return num_parameters;
}

int Program::NumEffectiveParameters() const {
  int num_parameters = 0;
  for (auto* parameter_block : parameter_blocks_) {
    num_parameters += parameter_block->TangentSize();
  }
  return num_parameters;
}

// TODO(sameeragarwal): The following methods should just be updated
// incrementally and the values cached, rather than the linear
// complexity we have right now on every call.
int Program::MaxScratchDoublesNeededForEvaluate() const {
  // Compute the scratch space needed for evaluate.
  int max_scratch_bytes_for_evaluate = 0;
  for (auto* residual_block : residual_blocks_) {
    max_scratch_bytes_for_evaluate =
        std::max(max_scratch_bytes_for_evaluate,
                 residual_block->NumScratchDoublesForEvaluate());
  }
  return max_scratch_bytes_for_evaluate;
}

int Program::MaxDerivativesPerResidualBlock() const {
  int max_derivatives = 0;
  for (auto* residual_block : residual_blocks_) {
    int derivatives = 0;
    int num_parameters = residual_block->NumParameterBlocks();
    for (int j = 0; j < num_parameters; ++j) {
      derivatives += residual_block->NumResiduals() *
                     residual_block->parameter_blocks()[j]->TangentSize();
    }
    max_derivatives = std::max(max_derivatives, derivatives);
  }
  return max_derivatives;
}

int Program::MaxParametersPerResidualBlock() const {
  int max_parameters = 0;
  for (auto* residual_block : residual_blocks_) {
    max_parameters =
        std::max(max_parameters, residual_block->NumParameterBlocks());
  }
  return max_parameters;
}

int Program::MaxResidualsPerResidualBlock() const {
  int max_residuals = 0;
  for (auto* residual_block : residual_blocks_) {
    max_residuals = std::max(max_residuals, residual_block->NumResiduals());
  }
  return max_residuals;
}

std::string Program::ToString() const {
  std::string ret = "Program dump\n";
  ret += StringPrintf("Number of parameter blocks: %d\n", NumParameterBlocks());
  ret += StringPrintf("Number of parameters: %d\n", NumParameters());
  ret += "Parameters:\n";
  for (int i = 0; i < parameter_blocks_.size(); ++i) {
    ret +=
        StringPrintf("%d: %s\n", i, parameter_blocks_[i]->ToString().c_str());
  }
  return ret;
}

}  // namespace ceres::internal
