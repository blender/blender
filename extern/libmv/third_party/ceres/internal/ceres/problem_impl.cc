// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2013 Google Inc. All rights reserved.
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
//         mierle@gmail.com (Keir Mierle)

#include "ceres/problem_impl.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "ceres/casts.h"
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/cost_function.h"
#include "ceres/crs_matrix.h"
#include "ceres/evaluator.h"
#include "ceres/loss_function.h"
#include "ceres/map_util.h"
#include "ceres/parameter_block.h"
#include "ceres/program.h"
#include "ceres/residual_block.h"
#include "ceres/stl_util.h"
#include "ceres/stringprintf.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

typedef map<double*, internal::ParameterBlock*> ParameterMap;

namespace {
internal::ParameterBlock* FindParameterBlockOrDie(
    const ParameterMap& parameter_map,
    double* parameter_block) {
  ParameterMap::const_iterator it = parameter_map.find(parameter_block);
  CHECK(it != parameter_map.end())
      << "Parameter block not found: " << parameter_block;
  return it->second;
}

// Returns true if two regions of memory, a and b, with sizes size_a and size_b
// respectively, overlap.
bool RegionsAlias(const double* a, int size_a,
                  const double* b, int size_b) {
  return (a < b) ? b < (a + size_a)
                 : a < (b + size_b);
}

void CheckForNoAliasing(double* existing_block,
                        int existing_block_size,
                        double* new_block,
                        int new_block_size) {
  CHECK(!RegionsAlias(existing_block, existing_block_size,
                      new_block, new_block_size))
      << "Aliasing detected between existing parameter block at memory "
      << "location " << existing_block
      << " and has size " << existing_block_size << " with new parameter "
      << "block that has memory address " << new_block << " and would have "
      << "size " << new_block_size << ".";
}

}  // namespace

ParameterBlock* ProblemImpl::InternalAddParameterBlock(double* values,
                                                       int size) {
  CHECK(values != NULL) << "Null pointer passed to AddParameterBlock "
                        << "for a parameter with size " << size;

  // Ignore the request if there is a block for the given pointer already.
  ParameterMap::iterator it = parameter_block_map_.find(values);
  if (it != parameter_block_map_.end()) {
    if (!options_.disable_all_safety_checks) {
      int existing_size = it->second->Size();
      CHECK(size == existing_size)
          << "Tried adding a parameter block with the same double pointer, "
          << values << ", twice, but with different block sizes. Original "
          << "size was " << existing_size << " but new size is "
          << size;
    }
    return it->second;
  }

  if (!options_.disable_all_safety_checks) {
    // Before adding the parameter block, also check that it doesn't alias any
    // other parameter blocks.
    if (!parameter_block_map_.empty()) {
      ParameterMap::iterator lb = parameter_block_map_.lower_bound(values);

      // If lb is not the first block, check the previous block for aliasing.
      if (lb != parameter_block_map_.begin()) {
        ParameterMap::iterator previous = lb;
        --previous;
        CheckForNoAliasing(previous->first,
                           previous->second->Size(),
                           values,
                           size);
      }

      // If lb is not off the end, check lb for aliasing.
      if (lb != parameter_block_map_.end()) {
        CheckForNoAliasing(lb->first,
                           lb->second->Size(),
                           values,
                           size);
      }
    }
  }

  // Pass the index of the new parameter block as well to keep the index in
  // sync with the position of the parameter in the program's parameter vector.
  ParameterBlock* new_parameter_block =
      new ParameterBlock(values, size, program_->parameter_blocks_.size());

  // For dynamic problems, add the list of dependent residual blocks, which is
  // empty to start.
  if (options_.enable_fast_removal) {
    new_parameter_block->EnableResidualBlockDependencies();
  }
  parameter_block_map_[values] = new_parameter_block;
  program_->parameter_blocks_.push_back(new_parameter_block);
  return new_parameter_block;
}

void ProblemImpl::InternalRemoveResidualBlock(ResidualBlock* residual_block) {
  CHECK_NOTNULL(residual_block);
  // Perform no check on the validity of residual_block, that is handled in
  // the public method: RemoveResidualBlock().

  // If needed, remove the parameter dependencies on this residual block.
  if (options_.enable_fast_removal) {
    const int num_parameter_blocks_for_residual =
        residual_block->NumParameterBlocks();
    for (int i = 0; i < num_parameter_blocks_for_residual; ++i) {
      residual_block->parameter_blocks()[i]
          ->RemoveResidualBlock(residual_block);
    }

    ResidualBlockSet::iterator it = residual_block_set_.find(residual_block);
    residual_block_set_.erase(it);
  }
  DeleteBlockInVector(program_->mutable_residual_blocks(), residual_block);
}

// Deletes the residual block in question, assuming there are no other
// references to it inside the problem (e.g. by another parameter). Referenced
// cost and loss functions are tucked away for future deletion, since it is not
// possible to know whether other parts of the problem depend on them without
// doing a full scan.
void ProblemImpl::DeleteBlock(ResidualBlock* residual_block) {
  // The const casts here are legit, since ResidualBlock holds these
  // pointers as const pointers but we have ownership of them and
  // have the right to destroy them when the destructor is called.
  if (options_.cost_function_ownership == TAKE_OWNERSHIP &&
      residual_block->cost_function() != NULL) {
    cost_functions_to_delete_.push_back(
        const_cast<CostFunction*>(residual_block->cost_function()));
  }
  if (options_.loss_function_ownership == TAKE_OWNERSHIP &&
      residual_block->loss_function() != NULL) {
    loss_functions_to_delete_.push_back(
        const_cast<LossFunction*>(residual_block->loss_function()));
  }
  delete residual_block;
}

// Deletes the parameter block in question, assuming there are no other
// references to it inside the problem (e.g. by any residual blocks).
// Referenced parameterizations are tucked away for future deletion, since it
// is not possible to know whether other parts of the problem depend on them
// without doing a full scan.
void ProblemImpl::DeleteBlock(ParameterBlock* parameter_block) {
  if (options_.local_parameterization_ownership == TAKE_OWNERSHIP &&
      parameter_block->local_parameterization() != NULL) {
    local_parameterizations_to_delete_.push_back(
        parameter_block->mutable_local_parameterization());
  }
  parameter_block_map_.erase(parameter_block->mutable_user_state());
  delete parameter_block;
}

ProblemImpl::ProblemImpl() : program_(new internal::Program) {}
ProblemImpl::ProblemImpl(const Problem::Options& options)
    : options_(options),
      program_(new internal::Program) {}

ProblemImpl::~ProblemImpl() {
  // Collect the unique cost/loss functions and delete the residuals.
  const int num_residual_blocks = program_->residual_blocks_.size();
  cost_functions_to_delete_.reserve(num_residual_blocks);
  loss_functions_to_delete_.reserve(num_residual_blocks);
  for (int i = 0; i < program_->residual_blocks_.size(); ++i) {
    DeleteBlock(program_->residual_blocks_[i]);
  }

  // Collect the unique parameterizations and delete the parameters.
  for (int i = 0; i < program_->parameter_blocks_.size(); ++i) {
    DeleteBlock(program_->parameter_blocks_[i]);
  }

  // Delete the owned cost/loss functions and parameterizations.
  STLDeleteUniqueContainerPointers(local_parameterizations_to_delete_.begin(),
                                   local_parameterizations_to_delete_.end());
  STLDeleteUniqueContainerPointers(cost_functions_to_delete_.begin(),
                                   cost_functions_to_delete_.end());
  STLDeleteUniqueContainerPointers(loss_functions_to_delete_.begin(),
                                   loss_functions_to_delete_.end());
}

ResidualBlock* ProblemImpl::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    const vector<double*>& parameter_blocks) {
  CHECK_NOTNULL(cost_function);
  CHECK_EQ(parameter_blocks.size(),
           cost_function->parameter_block_sizes().size());

  // Check the sizes match.
  const vector<int32>& parameter_block_sizes =
      cost_function->parameter_block_sizes();

  if (!options_.disable_all_safety_checks) {
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
        blocks += StringPrintf(" %p ", parameter_blocks[i]);
      }

      LOG(FATAL) << "Duplicate parameter blocks in a residual parameter "
                 << "are not allowed. Parameter block pointers: ["
                 << blocks << "]";
    }
  }

  // Add parameter blocks and convert the double*'s to parameter blocks.
  vector<ParameterBlock*> parameter_block_ptrs(parameter_blocks.size());
  for (int i = 0; i < parameter_blocks.size(); ++i) {
    parameter_block_ptrs[i] =
        InternalAddParameterBlock(parameter_blocks[i],
                                  parameter_block_sizes[i]);
  }

  if (!options_.disable_all_safety_checks) {
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
  }

  ResidualBlock* new_residual_block =
      new ResidualBlock(cost_function,
                        loss_function,
                        parameter_block_ptrs,
                        program_->residual_blocks_.size());

  // Add dependencies on the residual to the parameter blocks.
  if (options_.enable_fast_removal) {
    for (int i = 0; i < parameter_blocks.size(); ++i) {
      parameter_block_ptrs[i]->AddResidualBlock(new_residual_block);
    }
  }

  program_->residual_blocks_.push_back(new_residual_block);

  if (options_.enable_fast_removal) {
    residual_block_set_.insert(new_residual_block);
  }

  return new_residual_block;
}

// Unfortunately, macros don't help much to reduce this code, and var args don't
// work because of the ambiguous case that there is no loss function.
ResidualBlock* ProblemImpl::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* x0) {
  vector<double*> residual_parameters;
  residual_parameters.push_back(x0);
  return AddResidualBlock(cost_function, loss_function, residual_parameters);
}

ResidualBlock* ProblemImpl::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* x0, double* x1) {
  vector<double*> residual_parameters;
  residual_parameters.push_back(x0);
  residual_parameters.push_back(x1);
  return AddResidualBlock(cost_function, loss_function, residual_parameters);
}

ResidualBlock* ProblemImpl::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* x0, double* x1, double* x2) {
  vector<double*> residual_parameters;
  residual_parameters.push_back(x0);
  residual_parameters.push_back(x1);
  residual_parameters.push_back(x2);
  return AddResidualBlock(cost_function, loss_function, residual_parameters);
}

ResidualBlock* ProblemImpl::AddResidualBlock(
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

ResidualBlock* ProblemImpl::AddResidualBlock(
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

ResidualBlock* ProblemImpl::AddResidualBlock(
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

ResidualBlock* ProblemImpl::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* x0, double* x1, double* x2, double* x3, double* x4, double* x5,
    double* x6) {
  vector<double*> residual_parameters;
  residual_parameters.push_back(x0);
  residual_parameters.push_back(x1);
  residual_parameters.push_back(x2);
  residual_parameters.push_back(x3);
  residual_parameters.push_back(x4);
  residual_parameters.push_back(x5);
  residual_parameters.push_back(x6);
  return AddResidualBlock(cost_function, loss_function, residual_parameters);
}

ResidualBlock* ProblemImpl::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* x0, double* x1, double* x2, double* x3, double* x4, double* x5,
    double* x6, double* x7) {
  vector<double*> residual_parameters;
  residual_parameters.push_back(x0);
  residual_parameters.push_back(x1);
  residual_parameters.push_back(x2);
  residual_parameters.push_back(x3);
  residual_parameters.push_back(x4);
  residual_parameters.push_back(x5);
  residual_parameters.push_back(x6);
  residual_parameters.push_back(x7);
  return AddResidualBlock(cost_function, loss_function, residual_parameters);
}

ResidualBlock* ProblemImpl::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* x0, double* x1, double* x2, double* x3, double* x4, double* x5,
    double* x6, double* x7, double* x8) {
  vector<double*> residual_parameters;
  residual_parameters.push_back(x0);
  residual_parameters.push_back(x1);
  residual_parameters.push_back(x2);
  residual_parameters.push_back(x3);
  residual_parameters.push_back(x4);
  residual_parameters.push_back(x5);
  residual_parameters.push_back(x6);
  residual_parameters.push_back(x7);
  residual_parameters.push_back(x8);
  return AddResidualBlock(cost_function, loss_function, residual_parameters);
}

ResidualBlock* ProblemImpl::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* x0, double* x1, double* x2, double* x3, double* x4, double* x5,
    double* x6, double* x7, double* x8, double* x9) {
  vector<double*> residual_parameters;
  residual_parameters.push_back(x0);
  residual_parameters.push_back(x1);
  residual_parameters.push_back(x2);
  residual_parameters.push_back(x3);
  residual_parameters.push_back(x4);
  residual_parameters.push_back(x5);
  residual_parameters.push_back(x6);
  residual_parameters.push_back(x7);
  residual_parameters.push_back(x8);
  residual_parameters.push_back(x9);
  return AddResidualBlock(cost_function, loss_function, residual_parameters);
}

void ProblemImpl::AddParameterBlock(double* values, int size) {
  InternalAddParameterBlock(values, size);
}

void ProblemImpl::AddParameterBlock(
    double* values,
    int size,
    LocalParameterization* local_parameterization) {
  ParameterBlock* parameter_block =
      InternalAddParameterBlock(values, size);
  if (local_parameterization != NULL) {
    parameter_block->SetParameterization(local_parameterization);
  }
}

// Delete a block from a vector of blocks, maintaining the indexing invariant.
// This is done in constant time by moving an element from the end of the
// vector over the element to remove, then popping the last element. It
// destroys the ordering in the interest of speed.
template<typename Block>
void ProblemImpl::DeleteBlockInVector(vector<Block*>* mutable_blocks,
                                      Block* block_to_remove) {
  CHECK_EQ((*mutable_blocks)[block_to_remove->index()], block_to_remove)
      << "You found a Ceres bug! \n"
      << "Block requested: "
      << block_to_remove->ToString() << "\n"
      << "Block present: "
      << (*mutable_blocks)[block_to_remove->index()]->ToString();

  // Prepare the to-be-moved block for the new, lower-in-index position by
  // setting the index to the blocks final location.
  Block* tmp = mutable_blocks->back();
  tmp->set_index(block_to_remove->index());

  // Overwrite the to-be-deleted residual block with the one at the end.
  (*mutable_blocks)[block_to_remove->index()] = tmp;

  DeleteBlock(block_to_remove);

  // The block is gone so shrink the vector of blocks accordingly.
  mutable_blocks->pop_back();
}

void ProblemImpl::RemoveResidualBlock(ResidualBlock* residual_block) {
  CHECK_NOTNULL(residual_block);

  // Verify that residual_block identifies a residual in the current problem.
  const string residual_not_found_message =
      StringPrintf("Residual block to remove: %p not found. This usually means "
                   "one of three things have happened:\n"
                   " 1) residual_block is uninitialised and points to a random "
                   "area in memory.\n"
                   " 2) residual_block represented a residual that was added to"
                   " the problem, but referred to a parameter block which has "
                   "since been removed, which removes all residuals which "
                   "depend on that parameter block, and was thus removed.\n"
                   " 3) residual_block referred to a residual that has already "
                   "been removed from the problem (by the user).",
                   residual_block);
  if (options_.enable_fast_removal) {
    CHECK(residual_block_set_.find(residual_block) !=
          residual_block_set_.end())
        << residual_not_found_message;
  } else {
    // Perform a full search over all current residuals.
    CHECK(std::find(program_->residual_blocks().begin(),
                    program_->residual_blocks().end(),
                    residual_block) != program_->residual_blocks().end())
        << residual_not_found_message;
  }

  InternalRemoveResidualBlock(residual_block);
}

void ProblemImpl::RemoveParameterBlock(double* values) {
  ParameterBlock* parameter_block =
      FindParameterBlockOrDie(parameter_block_map_, values);

  if (options_.enable_fast_removal) {
    // Copy the dependent residuals from the parameter block because the set of
    // dependents will change after each call to RemoveResidualBlock().
    vector<ResidualBlock*> residual_blocks_to_remove(
        parameter_block->mutable_residual_blocks()->begin(),
        parameter_block->mutable_residual_blocks()->end());
    for (int i = 0; i < residual_blocks_to_remove.size(); ++i) {
      InternalRemoveResidualBlock(residual_blocks_to_remove[i]);
    }
  } else {
    // Scan all the residual blocks to remove ones that depend on the parameter
    // block. Do the scan backwards since the vector changes while iterating.
    const int num_residual_blocks = NumResidualBlocks();
    for (int i = num_residual_blocks - 1; i >= 0; --i) {
      ResidualBlock* residual_block =
          (*(program_->mutable_residual_blocks()))[i];
      const int num_parameter_blocks = residual_block->NumParameterBlocks();
      for (int j = 0; j < num_parameter_blocks; ++j) {
        if (residual_block->parameter_blocks()[j] == parameter_block) {
          InternalRemoveResidualBlock(residual_block);
          // The parameter blocks are guaranteed unique.
          break;
        }
      }
    }
  }
  DeleteBlockInVector(program_->mutable_parameter_blocks(), parameter_block);
}

void ProblemImpl::SetParameterBlockConstant(double* values) {
  FindParameterBlockOrDie(parameter_block_map_, values)->SetConstant();
}

void ProblemImpl::SetParameterBlockVariable(double* values) {
  FindParameterBlockOrDie(parameter_block_map_, values)->SetVarying();
}

void ProblemImpl::SetParameterization(
    double* values,
    LocalParameterization* local_parameterization) {
  FindParameterBlockOrDie(parameter_block_map_, values)
      ->SetParameterization(local_parameterization);
}

const LocalParameterization* ProblemImpl::GetParameterization(
    double* values) const {
  return FindParameterBlockOrDie(parameter_block_map_, values)
      ->local_parameterization();
}

void ProblemImpl::SetParameterLowerBound(double* values,
                                         int index,
                                         double lower_bound) {
  FindParameterBlockOrDie(parameter_block_map_, values)
      ->SetLowerBound(index, lower_bound);
}

void ProblemImpl::SetParameterUpperBound(double* values,
                                         int index,
                                         double upper_bound) {
  FindParameterBlockOrDie(parameter_block_map_, values)
      ->SetUpperBound(index, upper_bound);
}

bool ProblemImpl::Evaluate(const Problem::EvaluateOptions& evaluate_options,
                           double* cost,
                           vector<double>* residuals,
                           vector<double>* gradient,
                           CRSMatrix* jacobian) {
  if (cost == NULL &&
      residuals == NULL &&
      gradient == NULL &&
      jacobian == NULL) {
    LOG(INFO) << "Nothing to do.";
    return true;
  }

  // If the user supplied residual blocks, then use them, otherwise
  // take the residual blocks from the underlying program.
  Program program;
  *program.mutable_residual_blocks() =
      ((evaluate_options.residual_blocks.size() > 0)
       ? evaluate_options.residual_blocks : program_->residual_blocks());

  const vector<double*>& parameter_block_ptrs =
      evaluate_options.parameter_blocks;

  vector<ParameterBlock*> variable_parameter_blocks;
  vector<ParameterBlock*>& parameter_blocks =
      *program.mutable_parameter_blocks();

  if (parameter_block_ptrs.size() == 0) {
    // The user did not provide any parameter blocks, so default to
    // using all the parameter blocks in the order that they are in
    // the underlying program object.
    parameter_blocks = program_->parameter_blocks();
  } else {
    // The user supplied a vector of parameter blocks. Using this list
    // requires a number of steps.

    // 1. Convert double* into ParameterBlock*
    parameter_blocks.resize(parameter_block_ptrs.size());
    for (int i = 0; i < parameter_block_ptrs.size(); ++i) {
      parameter_blocks[i] =
          FindParameterBlockOrDie(parameter_block_map_,
                                  parameter_block_ptrs[i]);
    }

    // 2. The user may have only supplied a subset of parameter
    // blocks, so identify the ones that are not supplied by the user
    // and are NOT constant. These parameter blocks are stored in
    // variable_parameter_blocks.
    //
    // To ensure that the parameter blocks are not included in the
    // columns of the jacobian, we need to make sure that they are
    // constant during evaluation and then make them variable again
    // after we are done.
    vector<ParameterBlock*> all_parameter_blocks(program_->parameter_blocks());
    vector<ParameterBlock*> included_parameter_blocks(
        program.parameter_blocks());

    vector<ParameterBlock*> excluded_parameter_blocks;
    sort(all_parameter_blocks.begin(), all_parameter_blocks.end());
    sort(included_parameter_blocks.begin(), included_parameter_blocks.end());
    set_difference(all_parameter_blocks.begin(),
                   all_parameter_blocks.end(),
                   included_parameter_blocks.begin(),
                   included_parameter_blocks.end(),
                   back_inserter(excluded_parameter_blocks));

    variable_parameter_blocks.reserve(excluded_parameter_blocks.size());
    for (int i = 0; i < excluded_parameter_blocks.size(); ++i) {
      ParameterBlock* parameter_block = excluded_parameter_blocks[i];
      if (!parameter_block->IsConstant()) {
        variable_parameter_blocks.push_back(parameter_block);
        parameter_block->SetConstant();
      }
    }
  }

  // Setup the Parameter indices and offsets before an evaluator can
  // be constructed and used.
  program.SetParameterOffsetsAndIndex();

  Evaluator::Options evaluator_options;

  // Even though using SPARSE_NORMAL_CHOLESKY requires SuiteSparse or
  // CXSparse, here it just being used for telling the evaluator to
  // use a SparseRowCompressedMatrix for the jacobian. This is because
  // the Evaluator decides the storage for the Jacobian based on the
  // type of linear solver being used.
  evaluator_options.linear_solver_type = SPARSE_NORMAL_CHOLESKY;
  evaluator_options.num_threads = evaluate_options.num_threads;

  string error;
  scoped_ptr<Evaluator> evaluator(
      Evaluator::Create(evaluator_options, &program, &error));
  if (evaluator.get() == NULL) {
    LOG(ERROR) << "Unable to create an Evaluator object. "
               << "Error: " << error
               << "This is a Ceres bug; please contact the developers!";

    // Make the parameter blocks that were temporarily marked
    // constant, variable again.
    for (int i = 0; i < variable_parameter_blocks.size(); ++i) {
      variable_parameter_blocks[i]->SetVarying();
    }

    program_->SetParameterBlockStatePtrsToUserStatePtrs();
    program_->SetParameterOffsetsAndIndex();
    return false;
  }

  if (residuals !=NULL) {
    residuals->resize(evaluator->NumResiduals());
  }

  if (gradient != NULL) {
    gradient->resize(evaluator->NumEffectiveParameters());
  }

  scoped_ptr<CompressedRowSparseMatrix> tmp_jacobian;
  if (jacobian != NULL) {
    tmp_jacobian.reset(
        down_cast<CompressedRowSparseMatrix*>(evaluator->CreateJacobian()));
  }

  // Point the state pointers to the user state pointers. This is
  // needed so that we can extract a parameter vector which is then
  // passed to Evaluator::Evaluate.
  program.SetParameterBlockStatePtrsToUserStatePtrs();

  // Copy the value of the parameter blocks into a vector, since the
  // Evaluate::Evaluate method needs its input as such. The previous
  // call to SetParameterBlockStatePtrsToUserStatePtrs ensures that
  // these values are the ones corresponding to the actual state of
  // the parameter blocks, rather than the temporary state pointer
  // used for evaluation.
  Vector parameters(program.NumParameters());
  program.ParameterBlocksToStateVector(parameters.data());

  double tmp_cost = 0;

  Evaluator::EvaluateOptions evaluator_evaluate_options;
  evaluator_evaluate_options.apply_loss_function =
      evaluate_options.apply_loss_function;
  bool status = evaluator->Evaluate(evaluator_evaluate_options,
                                    parameters.data(),
                                    &tmp_cost,
                                    residuals != NULL ? &(*residuals)[0] : NULL,
                                    gradient != NULL ? &(*gradient)[0] : NULL,
                                    tmp_jacobian.get());

  // Make the parameter blocks that were temporarily marked constant,
  // variable again.
  for (int i = 0; i < variable_parameter_blocks.size(); ++i) {
    variable_parameter_blocks[i]->SetVarying();
  }

  if (status) {
    if (cost != NULL) {
      *cost = tmp_cost;
    }
    if (jacobian != NULL) {
      tmp_jacobian->ToCRSMatrix(jacobian);
    }
  }

  program_->SetParameterBlockStatePtrsToUserStatePtrs();
  program_->SetParameterOffsetsAndIndex();
  return status;
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

int ProblemImpl::ParameterBlockSize(const double* parameter_block) const {
  return FindParameterBlockOrDie(parameter_block_map_,
                                 const_cast<double*>(parameter_block))->Size();
};

int ProblemImpl::ParameterBlockLocalSize(const double* parameter_block) const {
  return FindParameterBlockOrDie(
      parameter_block_map_, const_cast<double*>(parameter_block))->LocalSize();
};

bool ProblemImpl::HasParameterBlock(const double* parameter_block) const {
  return (parameter_block_map_.find(const_cast<double*>(parameter_block)) !=
          parameter_block_map_.end());
}

void ProblemImpl::GetParameterBlocks(vector<double*>* parameter_blocks) const {
  CHECK_NOTNULL(parameter_blocks);
  parameter_blocks->resize(0);
  for (ParameterMap::const_iterator it = parameter_block_map_.begin();
       it != parameter_block_map_.end();
       ++it) {
    parameter_blocks->push_back(it->first);
  }
}

void ProblemImpl::GetResidualBlocks(
    vector<ResidualBlockId>* residual_blocks) const {
  CHECK_NOTNULL(residual_blocks);
  *residual_blocks = program().residual_blocks();
}

void ProblemImpl::GetParameterBlocksForResidualBlock(
    const ResidualBlockId residual_block,
    vector<double*>* parameter_blocks) const {
  int num_parameter_blocks = residual_block->NumParameterBlocks();
  CHECK_NOTNULL(parameter_blocks)->resize(num_parameter_blocks);
  for (int i = 0; i < num_parameter_blocks; ++i) {
    (*parameter_blocks)[i] =
        residual_block->parameter_blocks()[i]->mutable_user_state();
  }
}

const CostFunction* ProblemImpl::GetCostFunctionForResidualBlock(
    const ResidualBlockId residual_block) const {
  return residual_block->cost_function();
}

const LossFunction* ProblemImpl::GetLossFunctionForResidualBlock(
    const ResidualBlockId residual_block) const {
  return residual_block->loss_function();
}

void ProblemImpl::GetResidualBlocksForParameterBlock(
    const double* values,
    vector<ResidualBlockId>* residual_blocks) const {
  ParameterBlock* parameter_block =
      FindParameterBlockOrDie(parameter_block_map_,
                              const_cast<double*>(values));

  if (options_.enable_fast_removal) {
    // In this case the residual blocks that depend on the parameter block are
    // stored in the parameter block already, so just copy them out.
    CHECK_NOTNULL(residual_blocks)->resize(
        parameter_block->mutable_residual_blocks()->size());
    std::copy(parameter_block->mutable_residual_blocks()->begin(),
              parameter_block->mutable_residual_blocks()->end(),
              residual_blocks->begin());
    return;
  }

  // Find residual blocks that depend on the parameter block.
  CHECK_NOTNULL(residual_blocks)->clear();
  const int num_residual_blocks = NumResidualBlocks();
  for (int i = 0; i < num_residual_blocks; ++i) {
    ResidualBlock* residual_block =
        (*(program_->mutable_residual_blocks()))[i];
    const int num_parameter_blocks = residual_block->NumParameterBlocks();
    for (int j = 0; j < num_parameter_blocks; ++j) {
      if (residual_block->parameter_blocks()[j] == parameter_block) {
        residual_blocks->push_back(residual_block);
        // The parameter blocks are guaranteed unique.
        break;
      }
    }
  }
}

}  // namespace internal
}  // namespace ceres
