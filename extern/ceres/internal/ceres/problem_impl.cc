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
// Author: sameeragarwal@google.com (Sameer Agarwal)
//         mierle@gmail.com (Keir Mierle)

#include "ceres/problem_impl.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ceres/casts.h"
#include "ceres/compressed_row_jacobian_writer.h"
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/context_impl.h"
#include "ceres/cost_function.h"
#include "ceres/crs_matrix.h"
#include "ceres/evaluation_callback.h"
#include "ceres/evaluator.h"
#include "ceres/internal/export.h"
#include "ceres/internal/fixed_array.h"
#include "ceres/loss_function.h"
#include "ceres/manifold.h"
#include "ceres/map_util.h"
#include "ceres/parameter_block.h"
#include "ceres/program.h"
#include "ceres/program_evaluator.h"
#include "ceres/residual_block.h"
#include "ceres/scratch_evaluate_preparer.h"
#include "ceres/stl_util.h"
#include "ceres/stringprintf.h"
#include "glog/logging.h"

namespace ceres::internal {
namespace {
// Returns true if two regions of memory, a and b, with sizes size_a and size_b
// respectively, overlap.
bool RegionsAlias(const double* a, int size_a, const double* b, int size_b) {
  return (a < b) ? b < (a + size_a) : a < (b + size_b);
}

void CheckForNoAliasing(double* existing_block,
                        int existing_block_size,
                        double* new_block,
                        int new_block_size) {
  CHECK(!RegionsAlias(
      existing_block, existing_block_size, new_block, new_block_size))
      << "Aliasing detected between existing parameter block at memory "
      << "location " << existing_block << " and has size "
      << existing_block_size << " with new parameter "
      << "block that has memory address " << new_block << " and would have "
      << "size " << new_block_size << ".";
}

template <typename KeyType>
void DecrementValueOrDeleteKey(const KeyType key,
                               std::map<KeyType, int>* container) {
  auto it = container->find(key);
  if (it->second == 1) {
    delete key;
    container->erase(it);
  } else {
    --it->second;
  }
}

template <typename ForwardIterator>
void STLDeleteContainerPairFirstPointers(ForwardIterator begin,
                                         ForwardIterator end) {
  while (begin != end) {
    delete begin->first;
    ++begin;
  }
}

void InitializeContext(Context* context,
                       ContextImpl** context_impl,
                       bool* context_impl_owned) {
  if (context == nullptr) {
    *context_impl_owned = true;
    *context_impl = new ContextImpl;
  } else {
    *context_impl_owned = false;
    *context_impl = down_cast<ContextImpl*>(context);
  }
}

}  // namespace

ParameterBlock* ProblemImpl::InternalAddParameterBlock(double* values,
                                                       int size) {
  CHECK(values != nullptr) << "Null pointer passed to AddParameterBlock "
                           << "for a parameter with size " << size;

  // Ignore the request if there is a block for the given pointer already.
  auto it = parameter_block_map_.find(values);
  if (it != parameter_block_map_.end()) {
    if (!options_.disable_all_safety_checks) {
      int existing_size = it->second->Size();
      CHECK(size == existing_size)
          << "Tried adding a parameter block with the same double pointer, "
          << values << ", twice, but with different block sizes. Original "
          << "size was " << existing_size << " but new size is " << size;
    }
    return it->second;
  }

  if (!options_.disable_all_safety_checks) {
    // Before adding the parameter block, also check that it doesn't alias any
    // other parameter blocks.
    if (!parameter_block_map_.empty()) {
      auto lb = parameter_block_map_.lower_bound(values);

      // If lb is not the first block, check the previous block for aliasing.
      if (lb != parameter_block_map_.begin()) {
        auto previous = lb;
        --previous;
        CheckForNoAliasing(
            previous->first, previous->second->Size(), values, size);
      }

      // If lb is not off the end, check lb for aliasing.
      if (lb != parameter_block_map_.end()) {
        CheckForNoAliasing(lb->first, lb->second->Size(), values, size);
      }
    }
  }

  // Pass the index of the new parameter block as well to keep the index in
  // sync with the position of the parameter in the program's parameter vector.
  auto* new_parameter_block =
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
  CHECK(residual_block != nullptr);
  // Perform no check on the validity of residual_block, that is handled in
  // the public method: RemoveResidualBlock().

  // If needed, remove the parameter dependencies on this residual block.
  if (options_.enable_fast_removal) {
    const int num_parameter_blocks_for_residual =
        residual_block->NumParameterBlocks();
    for (int i = 0; i < num_parameter_blocks_for_residual; ++i) {
      residual_block->parameter_blocks()[i]->RemoveResidualBlock(
          residual_block);
    }

    auto it = residual_block_set_.find(residual_block);
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
  auto* cost_function =
      const_cast<CostFunction*>(residual_block->cost_function());
  if (options_.cost_function_ownership == TAKE_OWNERSHIP) {
    DecrementValueOrDeleteKey(cost_function, &cost_function_ref_count_);
  }

  auto* loss_function =
      const_cast<LossFunction*>(residual_block->loss_function());
  if (options_.loss_function_ownership == TAKE_OWNERSHIP &&
      loss_function != nullptr) {
    DecrementValueOrDeleteKey(loss_function, &loss_function_ref_count_);
  }

  delete residual_block;
}

// Deletes the parameter block in question, assuming there are no other
// references to it inside the problem (e.g. by any residual blocks).
void ProblemImpl::DeleteBlock(ParameterBlock* parameter_block) {
  parameter_block_map_.erase(parameter_block->mutable_user_state());
  delete parameter_block;
}

ProblemImpl::ProblemImpl()
    : options_(Problem::Options()), program_(new internal::Program) {
  InitializeContext(options_.context, &context_impl_, &context_impl_owned_);
}

ProblemImpl::ProblemImpl(const Problem::Options& options)
    : options_(options), program_(new internal::Program) {
  program_->evaluation_callback_ = options.evaluation_callback;
  InitializeContext(options_.context, &context_impl_, &context_impl_owned_);
}

ProblemImpl::~ProblemImpl() {
  STLDeleteContainerPointers(program_->residual_blocks_.begin(),
                             program_->residual_blocks_.end());

  if (options_.cost_function_ownership == TAKE_OWNERSHIP) {
    STLDeleteContainerPairFirstPointers(cost_function_ref_count_.begin(),
                                        cost_function_ref_count_.end());
  }

  if (options_.loss_function_ownership == TAKE_OWNERSHIP) {
    STLDeleteContainerPairFirstPointers(loss_function_ref_count_.begin(),
                                        loss_function_ref_count_.end());
  }

  // Collect the unique parameterizations and delete the parameters.
  for (auto* parameter_block : program_->parameter_blocks_) {
    DeleteBlock(parameter_block);
  }

  // Delete the owned manifolds.
  STLDeleteUniqueContainerPointers(manifolds_to_delete_.begin(),
                                   manifolds_to_delete_.end());

  if (context_impl_owned_) {
    delete context_impl_;
  }
}

ResidualBlockId ProblemImpl::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* const* const parameter_blocks,
    int num_parameter_blocks) {
  CHECK(cost_function != nullptr);
  CHECK_EQ(num_parameter_blocks, cost_function->parameter_block_sizes().size());

  // Check the sizes match.
  const std::vector<int32_t>& parameter_block_sizes =
      cost_function->parameter_block_sizes();

  if (!options_.disable_all_safety_checks) {
    CHECK_EQ(parameter_block_sizes.size(), num_parameter_blocks)
        << "Number of blocks input is different than the number of blocks "
        << "that the cost function expects.";

    // Check for duplicate parameter blocks.
    std::vector<double*> sorted_parameter_blocks(
        parameter_blocks, parameter_blocks + num_parameter_blocks);
    sort(sorted_parameter_blocks.begin(), sorted_parameter_blocks.end());
    const bool has_duplicate_items =
        (std::adjacent_find(sorted_parameter_blocks.begin(),
                            sorted_parameter_blocks.end()) !=
         sorted_parameter_blocks.end());
    if (has_duplicate_items) {
      std::string blocks;
      for (int i = 0; i < num_parameter_blocks; ++i) {
        blocks += StringPrintf(" %p ", parameter_blocks[i]);
      }

      LOG(FATAL) << "Duplicate parameter blocks in a residual parameter "
                 << "are not allowed. Parameter block pointers: [" << blocks
                 << "]";
    }
  }

  // Add parameter blocks and convert the double*'s to parameter blocks.
  std::vector<ParameterBlock*> parameter_block_ptrs(num_parameter_blocks);
  for (int i = 0; i < num_parameter_blocks; ++i) {
    parameter_block_ptrs[i] = InternalAddParameterBlock(
        parameter_blocks[i], parameter_block_sizes[i]);
  }

  if (!options_.disable_all_safety_checks) {
    // Check that the block sizes match the block sizes expected by the
    // cost_function.
    for (int i = 0; i < parameter_block_ptrs.size(); ++i) {
      CHECK_EQ(cost_function->parameter_block_sizes()[i],
               parameter_block_ptrs[i]->Size())
          << "The cost function expects parameter block " << i << " of size "
          << cost_function->parameter_block_sizes()[i]
          << " but was given a block of size "
          << parameter_block_ptrs[i]->Size();
    }
  }

  auto* new_residual_block =
      new ResidualBlock(cost_function,
                        loss_function,
                        parameter_block_ptrs,
                        program_->residual_blocks_.size());

  // Add dependencies on the residual to the parameter blocks.
  if (options_.enable_fast_removal) {
    for (int i = 0; i < num_parameter_blocks; ++i) {
      parameter_block_ptrs[i]->AddResidualBlock(new_residual_block);
    }
  }

  program_->residual_blocks_.push_back(new_residual_block);

  if (options_.enable_fast_removal) {
    residual_block_set_.insert(new_residual_block);
  }

  if (options_.cost_function_ownership == TAKE_OWNERSHIP) {
    // Increment the reference count, creating an entry in the table if
    // needed. Note: C++ maps guarantee that new entries have default
    // constructed values; this implies integers are zero initialized.
    ++cost_function_ref_count_[cost_function];
  }

  if (options_.loss_function_ownership == TAKE_OWNERSHIP &&
      loss_function != nullptr) {
    ++loss_function_ref_count_[loss_function];
  }

  return new_residual_block;
}

void ProblemImpl::AddParameterBlock(double* values, int size) {
  InternalAddParameterBlock(values, size);
}

void ProblemImpl::InternalSetManifold(double* /*values*/,
                                      ParameterBlock* parameter_block,
                                      Manifold* manifold) {
  if (manifold != nullptr && options_.manifold_ownership == TAKE_OWNERSHIP) {
    manifolds_to_delete_.push_back(manifold);
  }
  parameter_block->SetManifold(manifold);
}

void ProblemImpl::AddParameterBlock(double* values,
                                    int size,
                                    Manifold* manifold) {
  ParameterBlock* parameter_block = InternalAddParameterBlock(values, size);
  InternalSetManifold(values, parameter_block, manifold);
}

// Delete a block from a vector of blocks, maintaining the indexing invariant.
// This is done in constant time by moving an element from the end of the
// vector over the element to remove, then popping the last element. It
// destroys the ordering in the interest of speed.
template <typename Block>
void ProblemImpl::DeleteBlockInVector(std::vector<Block*>* mutable_blocks,
                                      Block* block_to_remove) {
  CHECK_EQ((*mutable_blocks)[block_to_remove->index()], block_to_remove)
      << "You found a Ceres bug! \n"
      << "Block requested: " << block_to_remove->ToString() << "\n"
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
  CHECK(residual_block != nullptr);

  // Verify that residual_block identifies a residual in the current problem.
  const std::string residual_not_found_message = StringPrintf(
      "Residual block to remove: %p not found. This usually means "
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
    CHECK(residual_block_set_.find(residual_block) != residual_block_set_.end())
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

void ProblemImpl::RemoveParameterBlock(const double* values) {
  ParameterBlock* parameter_block = FindWithDefault(
      parameter_block_map_, const_cast<double*>(values), nullptr);
  if (parameter_block == nullptr) {
    LOG(FATAL) << "Parameter block not found: " << values
               << ". You must add the parameter block to the problem before "
               << "it can be removed.";
  }

  if (options_.enable_fast_removal) {
    // Copy the dependent residuals from the parameter block because the set of
    // dependents will change after each call to RemoveResidualBlock().
    std::vector<ResidualBlock*> residual_blocks_to_remove(
        parameter_block->mutable_residual_blocks()->begin(),
        parameter_block->mutable_residual_blocks()->end());
    for (auto* residual_block : residual_blocks_to_remove) {
      InternalRemoveResidualBlock(residual_block);
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

void ProblemImpl::SetParameterBlockConstant(const double* values) {
  ParameterBlock* parameter_block = FindWithDefault(
      parameter_block_map_, const_cast<double*>(values), nullptr);
  if (parameter_block == nullptr) {
    LOG(FATAL) << "Parameter block not found: " << values
               << ". You must add the parameter block to the problem before "
               << "it can be set constant.";
  }

  parameter_block->SetConstant();
}

bool ProblemImpl::IsParameterBlockConstant(const double* values) const {
  const ParameterBlock* parameter_block = FindWithDefault(
      parameter_block_map_, const_cast<double*>(values), nullptr);
  CHECK(parameter_block != nullptr)
      << "Parameter block not found: " << values << ". You must add the "
      << "parameter block to the problem before it can be queried.";
  return parameter_block->IsConstant();
}

void ProblemImpl::SetParameterBlockVariable(double* values) {
  ParameterBlock* parameter_block =
      FindWithDefault(parameter_block_map_, values, nullptr);
  if (parameter_block == nullptr) {
    LOG(FATAL) << "Parameter block not found: " << values
               << ". You must add the parameter block to the problem before "
               << "it can be set varying.";
  }

  parameter_block->SetVarying();
}

void ProblemImpl::SetManifold(double* values, Manifold* manifold) {
  ParameterBlock* parameter_block =
      FindWithDefault(parameter_block_map_, values, nullptr);
  if (parameter_block == nullptr) {
    LOG(FATAL) << "Parameter block not found: " << values
               << ". You must add the parameter block to the problem before "
               << "you can set its manifold.";
  }

  InternalSetManifold(values, parameter_block, manifold);
}

const Manifold* ProblemImpl::GetManifold(const double* values) const {
  ParameterBlock* parameter_block = FindWithDefault(
      parameter_block_map_, const_cast<double*>(values), nullptr);
  if (parameter_block == nullptr) {
    LOG(FATAL) << "Parameter block not found: " << values
               << ". You must add the parameter block to the problem before "
               << "you can get its manifold.";
  }

  return parameter_block->manifold();
}

bool ProblemImpl::HasManifold(const double* values) const {
  return GetManifold(values) != nullptr;
}

void ProblemImpl::SetParameterLowerBound(double* values,
                                         int index,
                                         double lower_bound) {
  ParameterBlock* parameter_block =
      FindWithDefault(parameter_block_map_, values, nullptr);
  if (parameter_block == nullptr) {
    LOG(FATAL) << "Parameter block not found: " << values
               << ". You must add the parameter block to the problem before "
               << "you can set a lower bound on one of its components.";
  }

  parameter_block->SetLowerBound(index, lower_bound);
}

void ProblemImpl::SetParameterUpperBound(double* values,
                                         int index,
                                         double upper_bound) {
  ParameterBlock* parameter_block =
      FindWithDefault(parameter_block_map_, values, nullptr);
  if (parameter_block == nullptr) {
    LOG(FATAL) << "Parameter block not found: " << values
               << ". You must add the parameter block to the problem before "
               << "you can set an upper bound on one of its components.";
  }
  parameter_block->SetUpperBound(index, upper_bound);
}

double ProblemImpl::GetParameterLowerBound(const double* values,
                                           int index) const {
  ParameterBlock* parameter_block = FindWithDefault(
      parameter_block_map_, const_cast<double*>(values), nullptr);
  if (parameter_block == nullptr) {
    LOG(FATAL) << "Parameter block not found: " << values
               << ". You must add the parameter block to the problem before "
               << "you can get the lower bound on one of its components.";
  }
  return parameter_block->LowerBound(index);
}

double ProblemImpl::GetParameterUpperBound(const double* values,
                                           int index) const {
  ParameterBlock* parameter_block = FindWithDefault(
      parameter_block_map_, const_cast<double*>(values), nullptr);
  if (parameter_block == nullptr) {
    LOG(FATAL) << "Parameter block not found: " << values
               << ". You must add the parameter block to the problem before "
               << "you can set an upper bound on one of its components.";
  }
  return parameter_block->UpperBound(index);
}

bool ProblemImpl::Evaluate(const Problem::EvaluateOptions& evaluate_options,
                           double* cost,
                           std::vector<double>* residuals,
                           std::vector<double>* gradient,
                           CRSMatrix* jacobian) {
  if (cost == nullptr && residuals == nullptr && gradient == nullptr &&
      jacobian == nullptr) {
    return true;
  }

  // If the user supplied residual blocks, then use them, otherwise
  // take the residual blocks from the underlying program.
  Program program;
  *program.mutable_residual_blocks() =
      ((evaluate_options.residual_blocks.size() > 0)
           ? evaluate_options.residual_blocks
           : program_->residual_blocks());

  const std::vector<double*>& parameter_block_ptrs =
      evaluate_options.parameter_blocks;

  std::vector<ParameterBlock*> variable_parameter_blocks;
  std::vector<ParameterBlock*>& parameter_blocks =
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
      parameter_blocks[i] = FindWithDefault(
          parameter_block_map_, parameter_block_ptrs[i], nullptr);
      if (parameter_blocks[i] == nullptr) {
        LOG(FATAL) << "No known parameter block for "
                   << "Problem::Evaluate::Options.parameter_blocks[" << i << "]"
                   << " = " << parameter_block_ptrs[i];
      }
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
    std::vector<ParameterBlock*> all_parameter_blocks(
        program_->parameter_blocks());
    std::vector<ParameterBlock*> included_parameter_blocks(
        program.parameter_blocks());

    std::vector<ParameterBlock*> excluded_parameter_blocks;
    sort(all_parameter_blocks.begin(), all_parameter_blocks.end());
    sort(included_parameter_blocks.begin(), included_parameter_blocks.end());
    set_difference(all_parameter_blocks.begin(),
                   all_parameter_blocks.end(),
                   included_parameter_blocks.begin(),
                   included_parameter_blocks.end(),
                   back_inserter(excluded_parameter_blocks));

    variable_parameter_blocks.reserve(excluded_parameter_blocks.size());
    for (auto* parameter_block : excluded_parameter_blocks) {
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

  // The main thread also does work so we only need to launch num_threads - 1.
  context_impl_->EnsureMinimumThreads(evaluator_options.num_threads - 1);
  evaluator_options.context = context_impl_;
  evaluator_options.evaluation_callback =
      program_->mutable_evaluation_callback();
  std::unique_ptr<Evaluator> evaluator(
      new ProgramEvaluator<ScratchEvaluatePreparer,
                           CompressedRowJacobianWriter>(evaluator_options,
                                                        &program));

  if (residuals != nullptr) {
    residuals->resize(evaluator->NumResiduals());
  }

  if (gradient != nullptr) {
    gradient->resize(evaluator->NumEffectiveParameters());
  }

  std::unique_ptr<CompressedRowSparseMatrix> tmp_jacobian;
  if (jacobian != nullptr) {
    tmp_jacobian.reset(down_cast<CompressedRowSparseMatrix*>(
        evaluator->CreateJacobian().release()));
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
  bool status =
      evaluator->Evaluate(evaluator_evaluate_options,
                          parameters.data(),
                          &tmp_cost,
                          residuals != nullptr ? &(*residuals)[0] : nullptr,
                          gradient != nullptr ? &(*gradient)[0] : nullptr,
                          tmp_jacobian.get());

  // Make the parameter blocks that were temporarily marked constant,
  // variable again.
  for (auto* parameter_block : variable_parameter_blocks) {
    parameter_block->SetVarying();
  }

  if (status) {
    if (cost != nullptr) {
      *cost = tmp_cost;
    }
    if (jacobian != nullptr) {
      tmp_jacobian->ToCRSMatrix(jacobian);
    }
  }

  program_->SetParameterBlockStatePtrsToUserStatePtrs();
  program_->SetParameterOffsetsAndIndex();
  return status;
}

bool ProblemImpl::EvaluateResidualBlock(ResidualBlock* residual_block,
                                        bool apply_loss_function,
                                        bool new_point,
                                        double* cost,
                                        double* residuals,
                                        double** jacobians) const {
  auto evaluation_callback = program_->mutable_evaluation_callback();
  if (evaluation_callback) {
    evaluation_callback->PrepareForEvaluation(jacobians != nullptr, new_point);
  }

  ParameterBlock* const* parameter_blocks = residual_block->parameter_blocks();
  const int num_parameter_blocks = residual_block->NumParameterBlocks();
  for (int i = 0; i < num_parameter_blocks; ++i) {
    ParameterBlock* parameter_block = parameter_blocks[i];
    if (parameter_block->IsConstant()) {
      if (jacobians != nullptr && jacobians[i] != nullptr) {
        LOG(ERROR) << "Jacobian requested for parameter block : " << i
                   << ". But the parameter block is marked constant.";
        return false;
      }
    } else {
      CHECK(parameter_block->SetState(parameter_block->user_state()))
          << "Congratulations, you found a Ceres bug! Please report this error "
          << "to the developers.";
    }
  }

  double dummy_cost = 0.0;
  FixedArray<double, 32> scratch(
      residual_block->NumScratchDoublesForEvaluate());
  return residual_block->Evaluate(apply_loss_function,
                                  cost ? cost : &dummy_cost,
                                  residuals,
                                  jacobians,
                                  scratch.data());
}

int ProblemImpl::NumParameterBlocks() const {
  return program_->NumParameterBlocks();
}

int ProblemImpl::NumParameters() const { return program_->NumParameters(); }

int ProblemImpl::NumResidualBlocks() const {
  return program_->NumResidualBlocks();
}

int ProblemImpl::NumResiduals() const { return program_->NumResiduals(); }

int ProblemImpl::ParameterBlockSize(const double* values) const {
  ParameterBlock* parameter_block = FindWithDefault(
      parameter_block_map_, const_cast<double*>(values), nullptr);
  if (parameter_block == nullptr) {
    LOG(FATAL) << "Parameter block not found: " << values
               << ". You must add the parameter block to the problem before "
               << "you can get its size.";
  }

  return parameter_block->Size();
}

int ProblemImpl::ParameterBlockTangentSize(const double* values) const {
  ParameterBlock* parameter_block = FindWithDefault(
      parameter_block_map_, const_cast<double*>(values), nullptr);
  if (parameter_block == nullptr) {
    LOG(FATAL) << "Parameter block not found: " << values
               << ". You must add the parameter block to the problem before "
               << "you can get its tangent size.";
  }

  return parameter_block->TangentSize();
}

bool ProblemImpl::HasParameterBlock(const double* values) const {
  return (parameter_block_map_.find(const_cast<double*>(values)) !=
          parameter_block_map_.end());
}

void ProblemImpl::GetParameterBlocks(
    std::vector<double*>* parameter_blocks) const {
  CHECK(parameter_blocks != nullptr);
  parameter_blocks->resize(0);
  parameter_blocks->reserve(parameter_block_map_.size());
  for (const auto& entry : parameter_block_map_) {
    parameter_blocks->push_back(entry.first);
  }
}

void ProblemImpl::GetResidualBlocks(
    std::vector<ResidualBlockId>* residual_blocks) const {
  CHECK(residual_blocks != nullptr);
  *residual_blocks = program().residual_blocks();
}

void ProblemImpl::GetParameterBlocksForResidualBlock(
    const ResidualBlockId residual_block,
    std::vector<double*>* parameter_blocks) const {
  int num_parameter_blocks = residual_block->NumParameterBlocks();
  CHECK(parameter_blocks != nullptr);
  parameter_blocks->resize(num_parameter_blocks);
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
    const double* values, std::vector<ResidualBlockId>* residual_blocks) const {
  ParameterBlock* parameter_block = FindWithDefault(
      parameter_block_map_, const_cast<double*>(values), nullptr);
  if (parameter_block == nullptr) {
    LOG(FATAL) << "Parameter block not found: " << values
               << ". You must add the parameter block to the problem before "
               << "you can get the residual blocks that depend on it.";
  }

  if (options_.enable_fast_removal) {
    // In this case the residual blocks that depend on the parameter block are
    // stored in the parameter block already, so just copy them out.
    CHECK(residual_blocks != nullptr);
    residual_blocks->resize(parameter_block->mutable_residual_blocks()->size());
    std::copy(parameter_block->mutable_residual_blocks()->begin(),
              parameter_block->mutable_residual_blocks()->end(),
              residual_blocks->begin());
    return;
  }

  // Find residual blocks that depend on the parameter block.
  CHECK(residual_blocks != nullptr);
  residual_blocks->clear();
  const int num_residual_blocks = NumResidualBlocks();
  for (int i = 0; i < num_residual_blocks; ++i) {
    ResidualBlock* residual_block = (*(program_->mutable_residual_blocks()))[i];
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

}  // namespace ceres::internal
