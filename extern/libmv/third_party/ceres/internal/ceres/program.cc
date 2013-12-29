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

#include "ceres/program.h"

#include <map>
#include <vector>
#include "ceres/casts.h"
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/cost_function.h"
#include "ceres/evaluator.h"
#include "ceres/internal/port.h"
#include "ceres/local_parameterization.h"
#include "ceres/loss_function.h"
#include "ceres/map_util.h"
#include "ceres/parameter_block.h"
#include "ceres/problem.h"
#include "ceres/residual_block.h"
#include "ceres/stl_util.h"

namespace ceres {
namespace internal {

Program::Program() {}

Program::Program(const Program& program)
    : parameter_blocks_(program.parameter_blocks_),
      residual_blocks_(program.residual_blocks_) {
}

const vector<ParameterBlock*>& Program::parameter_blocks() const {
  return parameter_blocks_;
}

const vector<ResidualBlock*>& Program::residual_blocks() const {
  return residual_blocks_;
}

vector<ParameterBlock*>* Program::mutable_parameter_blocks() {
  return &parameter_blocks_;
}

vector<ResidualBlock*>* Program::mutable_residual_blocks() {
  return &residual_blocks_;
}

bool Program::StateVectorToParameterBlocks(const double *state) {
  for (int i = 0; i < parameter_blocks_.size(); ++i) {
    if (!parameter_blocks_[i]->IsConstant() &&
        !parameter_blocks_[i]->SetState(state)) {
      return false;
    }
    state += parameter_blocks_[i]->Size();
  }
  return true;
}

void Program::ParameterBlocksToStateVector(double *state) const {
  for (int i = 0; i < parameter_blocks_.size(); ++i) {
    parameter_blocks_[i]->GetState(state);
    state += parameter_blocks_[i]->Size();
  }
}

void Program::CopyParameterBlockStateToUserState() {
  for (int i = 0; i < parameter_blocks_.size(); ++i) {
    parameter_blocks_[i]->GetState(parameter_blocks_[i]->mutable_user_state());
  }
}

bool Program::SetParameterBlockStatePtrsToUserStatePtrs() {
  for (int i = 0; i < parameter_blocks_.size(); ++i) {
    if (!parameter_blocks_[i]->IsConstant() &&
        !parameter_blocks_[i]->SetState(parameter_blocks_[i]->user_state())) {
      return false;
    }
  }
  return true;
}

bool Program::Plus(const double* state,
                   const double* delta,
                   double* state_plus_delta) const {
  for (int i = 0; i < parameter_blocks_.size(); ++i) {
    if (!parameter_blocks_[i]->Plus(state, delta, state_plus_delta)) {
      return false;
    }
    state += parameter_blocks_[i]->Size();
    delta += parameter_blocks_[i]->LocalSize();
    state_plus_delta += parameter_blocks_[i]->Size();
  }
  return true;
}

void Program::SetParameterOffsetsAndIndex() {
  // Set positions for all parameters appearing as arguments to residuals to one
  // past the end of the parameter block array.
  for (int i = 0; i < residual_blocks_.size(); ++i) {
    ResidualBlock* residual_block = residual_blocks_[i];
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
    delta_offset += parameter_blocks_[i]->LocalSize();
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
    delta_offset += parameter_blocks_[i]->LocalSize();
  }

  return true;
}

int Program::NumResidualBlocks() const {
  return residual_blocks_.size();
}

int Program::NumParameterBlocks() const {
  return parameter_blocks_.size();
}

int Program::NumResiduals() const {
  int num_residuals = 0;
  for (int i = 0; i < residual_blocks_.size(); ++i) {
    num_residuals += residual_blocks_[i]->NumResiduals();
  }
  return num_residuals;
}

int Program::NumParameters() const {
  int num_parameters = 0;
  for (int i = 0; i < parameter_blocks_.size(); ++i) {
    num_parameters += parameter_blocks_[i]->Size();
  }
  return num_parameters;
}

int Program::NumEffectiveParameters() const {
  int num_parameters = 0;
  for (int i = 0; i < parameter_blocks_.size(); ++i) {
    num_parameters += parameter_blocks_[i]->LocalSize();
  }
  return num_parameters;
}

int Program::MaxScratchDoublesNeededForEvaluate() const {
  // Compute the scratch space needed for evaluate.
  int max_scratch_bytes_for_evaluate = 0;
  for (int i = 0; i < residual_blocks_.size(); ++i) {
    max_scratch_bytes_for_evaluate =
        max(max_scratch_bytes_for_evaluate,
            residual_blocks_[i]->NumScratchDoublesForEvaluate());
  }
  return max_scratch_bytes_for_evaluate;
}

int Program::MaxDerivativesPerResidualBlock() const {
  int max_derivatives = 0;
  for (int i = 0; i < residual_blocks_.size(); ++i) {
    int derivatives = 0;
    ResidualBlock* residual_block = residual_blocks_[i];
    int num_parameters = residual_block->NumParameterBlocks();
    for (int j = 0; j < num_parameters; ++j) {
      derivatives += residual_block->NumResiduals() *
                     residual_block->parameter_blocks()[j]->LocalSize();
    }
    max_derivatives = max(max_derivatives, derivatives);
  }
  return max_derivatives;
}

int Program::MaxParametersPerResidualBlock() const {
  int max_parameters = 0;
  for (int i = 0; i < residual_blocks_.size(); ++i) {
    max_parameters = max(max_parameters,
                         residual_blocks_[i]->NumParameterBlocks());
  }
  return max_parameters;
}

int Program::MaxResidualsPerResidualBlock() const {
  int max_residuals = 0;
  for (int i = 0; i < residual_blocks_.size(); ++i) {
    max_residuals = max(max_residuals,
                        residual_blocks_[i]->NumResiduals());
  }
  return max_residuals;
}

string Program::ToString() const {
  string ret = "Program dump\n";
  ret += StringPrintf("Number of parameter blocks: %d\n", NumParameterBlocks());
  ret += StringPrintf("Number of parameters: %d\n", NumParameters());
  ret += "Parameters:\n";
  for (int i = 0; i < parameter_blocks_.size(); ++i) {
    ret += StringPrintf("%d: %s\n",
                        i, parameter_blocks_[i]->ToString().c_str());
  }
  return ret;
}

}  // namespace internal
}  // namespace ceres
