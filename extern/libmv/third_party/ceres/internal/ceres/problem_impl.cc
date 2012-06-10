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
//         keir@google.com (Keir Mierle)

#include "ceres/problem_impl.h"

#include <algorithm>
#include <cstddef>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <glog/logging.h>
#include "ceres/parameter_block.h"
#include "ceres/program.h"
#include "ceres/residual_block.h"
#include "ceres/stl_util.h"
#include "ceres/map_util.h"
#include "ceres/stringprintf.h"
#include "ceres/cost_function.h"
#include "ceres/loss_function.h"

namespace ceres {
namespace internal {

typedef map<double*, internal::ParameterBlock*> ParameterMap;

// Returns true if two regions of memory, a and b, with sizes size_a and size_b
// respectively, overlap.
static bool RegionsAlias(const double* a, int size_a,
                         const double* b, int size_b) {
  return (a < b) ? b < (a + size_a)
                 : a < (b + size_b);
}

static void CheckForNoAliasing(double* existing_block,
                               int existing_block_size,
                               double* new_block,
                               int new_block_size) {
  CHECK(!RegionsAlias(existing_block, existing_block_size,
                      new_block, new_block_size))
      << "Aliasing detected between existing parameter block at memory "
      << "location " << existing_block
      << " and has size " << existing_block_size << " with new parameter "
      << "block that has memory adderss " << new_block << " and would have "
      << "size " << new_block_size << ".";
}

static ParameterBlock* InternalAddParameterBlock(
    double* values,
    int size,
    ParameterMap* parameter_map,
    vector<ParameterBlock*>* parameter_blocks) {
  CHECK(values) << "Null pointer passed to AddParameterBlock for a parameter "
                << "with size " << size;

  // Ignore the request if there is a block for the given pointer already.
  ParameterMap::iterator it = parameter_map->find(values);
  if (it != parameter_map->end()) {
    int existing_size = it->second->Size();
    CHECK(size == existing_size)
        << "Tried adding a parameter block with the same double pointer, "
        << values << ", twice, but with different block sizes. Original "
        << "size was " << existing_size << " but new size is "
        << size;
    return it->second;
  }
  // Before adding the parameter block, also check that it doesn't alias any
  // other parameter blocks.
  if (!parameter_map->empty()) {
    ParameterMap::iterator lb = parameter_map->lower_bound(values);

    // If lb is not the first block, check the previous block for aliasing.
    if (lb != parameter_map->begin()) {
      ParameterMap::iterator previous = lb;
      --previous;
      CheckForNoAliasing(previous->first,
                         previous->second->Size(),
                         values,
                         size);
    }

    // If lb is not off the end, check lb for aliasing.
    if (lb != parameter_map->end()) {
      CheckForNoAliasing(lb->first,
                         lb->second->Size(),
                         values,
                         size);
    }
  }
  ParameterBlock* new_parameter_block = new ParameterBlock(values, size);
  (*parameter_map)[values] = new_parameter_block;
  parameter_blocks->push_back(new_parameter_block);
  return new_parameter_block;
}

ProblemImpl::ProblemImpl() : program_(new internal::Program) {}
ProblemImpl::ProblemImpl(const Problem::Options& options)
    : options_(options),
      program_(new internal::Program) {}

ProblemImpl::~ProblemImpl() {
  // Collect the unique cost/loss functions and delete the residuals.
  set<CostFunction*> cost_functions;
  set<LossFunction*> loss_functions;
  for (int i = 0; i < program_->residual_blocks_.size(); ++i) {
    ResidualBlock* residual_block = program_->residual_blocks_[i];

    // The const casts here are legit, since ResidualBlock holds these
    // pointers as const pointers but we have ownership of them and
    // have the right to destroy them when the destructor is called.
    if (options_.cost_function_ownership == TAKE_OWNERSHIP) {
      cost_functions.insert(
          const_cast<CostFunction*>(residual_block->cost_function()));
    }
    if (options_.loss_function_ownership == TAKE_OWNERSHIP) {
      loss_functions.insert(
          const_cast<LossFunction*>(residual_block->loss_function()));
    }

    delete residual_block;
  }

  // Collect the unique parameterizations and delete the parameters.
  set<LocalParameterization*> local_parameterizations;
  for (int i = 0; i < program_->parameter_blocks_.size(); ++i) {
    ParameterBlock* parameter_block = program_->parameter_blocks_[i];

    if (options_.local_parameterization_ownership == TAKE_OWNERSHIP) {
      local_parameterizations.insert(parameter_block->local_parameterization_);
    }

    delete parameter_block;
  }

  // Delete the owned cost/loss functions and parameterizations.
  STLDeleteContainerPointers(local_parameterizations.begin(),
                             local_parameterizations.end());
  STLDeleteContainerPointers(cost_functions.begin(),
                             cost_functions.end());
  STLDeleteContainerPointers(loss_functions.begin(),
                             loss_functions.end());
}

const ResidualBlock* ProblemImpl::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    const vector<double*>& parameter_blocks) {
  CHECK_NOTNULL(cost_function);
  CHECK_EQ(parameter_blocks.size(),
           cost_function->parameter_block_sizes().size());

  // Check the sizes match.
  const vector<int16>& parameter_block_sizes =
      cost_function->parameter_block_sizes();
  CHECK_EQ(parameter_block_sizes.size(), parameter_blocks.size())
      << "Number of blocks input is different than the number of blocks "
      << "that the cost function expects.";

  // Check for duplicate parameter blocks.
  vector<double*> sorted_parameter_blocks(parameter_blocks);
  sort(sorted_parameter_blocks.begin(), sorted_parameter_blocks.end());
  vector<double*>::const_iterator duplicate_items =
      unique(sorted_parameter_blocks.begin(),
             sorted_parameter_blocks.end());
  if (duplicate_items != sorted_parameter_blocks.end()) {
    string blocks;
    for (int i = 0; i < parameter_blocks.size(); ++i) {
      blocks += internal::StringPrintf(" %p ", parameter_blocks[i]);
    }

    LOG(FATAL) << "Duplicate parameter blocks in a residual parameter "
               << "are not allowed. Parameter block pointers: ["
               << blocks << "]";
  }

  // Add parameter blocks and convert the double*'s to parameter blocks.
  vector<ParameterBlock*> parameter_block_ptrs(parameter_blocks.size());
  for (int i = 0; i < parameter_blocks.size(); ++i) {
    parameter_block_ptrs[i] =
        InternalAddParameterBlock(parameter_blocks[i],
                                  parameter_block_sizes[i],
                                  &parameter_block_map_,
                                  &program_->parameter_blocks_);
  }

  // Check that the block sizes match the block sizes expected by the
  // cost_function.
  for (int i = 0; i < parameter_block_ptrs.size(); ++i) {
    CHECK_EQ(cost_function->parameter_block_sizes()[i],
             parameter_block_ptrs[i]->Size())
        << "The cost function expects parameter block " << i
        << " of size " << cost_function->parameter_block_sizes()[i]
        << " but was given a block of size "
        << parameter_block_ptrs[i]->Size();
  }

  ResidualBlock* new_residual_block =
      new ResidualBlock(cost_function,
                        loss_function,
                        parameter_block_ptrs);
  program_->residual_blocks_.push_back(new_residual_block);
  return new_residual_block;
}

// Unfortunately, macros don't help much to reduce this code, and var args don't
// work because of the ambiguous case that there is no loss function.
const ResidualBlock* ProblemImpl::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* x0) {
  vector<double*> residual_parameters;
  residual_parameters.push_back(x0);
  return AddResidualBlock(cost_function, loss_function, residual_parameters);
}

const ResidualBlock* ProblemImpl::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* x0, double* x1) {
  vector<double*> residual_parameters;
  residual_parameters.push_back(x0);
  residual_parameters.push_back(x1);
  return AddResidualBlock(cost_function, loss_function, residual_parameters);
}

const ResidualBlock* ProblemImpl::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* x0, double* x1, double* x2) {
  vector<double*> residual_parameters;
  residual_parameters.push_back(x0);
  residual_parameters.push_back(x1);
  residual_parameters.push_back(x2);
  return AddResidualBlock(cost_function, loss_function, residual_parameters);
}

const ResidualBlock* ProblemImpl::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* x0, double* x1, double* x2, double* x3) {
  vector<double*> residual_parameters;
  residual_parameters.push_back(x0);
  residual_parameters.push_back(x1);
  residual_parameters.push_back(x2);
  residual_parameters.push_back(x3);
  return AddResidualBlock(cost_function, loss_function, residual_parameters);
}

const ResidualBlock* ProblemImpl::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* x0, double* x1, double* x2, double* x3, double* x4) {
  vector<double*> residual_parameters;
  residual_parameters.push_back(x0);
  residual_parameters.push_back(x1);
  residual_parameters.push_back(x2);
  residual_parameters.push_back(x3);
  residual_parameters.push_back(x4);
  return AddResidualBlock(cost_function, loss_function, residual_parameters);
}

const ResidualBlock* ProblemImpl::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* x0, double* x1, double* x2, double* x3, double* x4, double* x5) {
  vector<double*> residual_parameters;
  residual_parameters.push_back(x0);
  residual_parameters.push_back(x1);
  residual_parameters.push_back(x2);
  residual_parameters.push_back(x3);
  residual_parameters.push_back(x4);
  residual_parameters.push_back(x5);
  return AddResidualBlock(cost_function, loss_function, residual_parameters);
}


void ProblemImpl::AddParameterBlock(double* values, int size) {
  InternalAddParameterBlock(values,
                            size,
                            &parameter_block_map_,
                            &program_->parameter_blocks_);
}

void ProblemImpl::AddParameterBlock(
    double* values,
    int size,
    LocalParameterization* local_parameterization) {
  ParameterBlock* parameter_block =
      InternalAddParameterBlock(values,
                                size,
                                &parameter_block_map_,
                                &program_->parameter_blocks_);
  if (local_parameterization != NULL) {
    parameter_block->SetParameterization(local_parameterization);
  }
}

void ProblemImpl::SetParameterBlockConstant(double* values) {
  FindOrDie(parameter_block_map_, values)->SetConstant();
}

void ProblemImpl::SetParameterBlockVariable(double* values) {
  FindOrDie(parameter_block_map_, values)->SetVarying();
}

void ProblemImpl::SetParameterization(
    double* values,
    LocalParameterization* local_parameterization) {
  FindOrDie(parameter_block_map_, values)
      ->SetParameterization(local_parameterization);
}

int ProblemImpl::NumParameterBlocks() const {
  return program_->NumParameterBlocks();
}

int ProblemImpl::NumParameters() const {
  return program_->NumParameters();
}

int ProblemImpl::NumResidualBlocks() const {
  return program_->NumResidualBlocks();
}

int ProblemImpl::NumResiduals() const {
  return program_->NumResiduals();
}

}  // namespace internal
}  // namespace ceres
