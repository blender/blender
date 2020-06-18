// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2019 Google Inc. All rights reserved.
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
// This is the implementation of the public Problem API. The pointer to
// implementation (PIMPL) idiom makes it possible for Ceres internal code to
// refer to the private data members without needing to exposing it to the
// world. An alternative to PIMPL is to have a factory which returns instances
// of a virtual base class; while that approach would work, it requires clients
// to always put a Problem object into a scoped pointer; this needlessly muddies
// client code for little benefit. Therefore, the PIMPL comprise was chosen.

#ifndef CERES_PUBLIC_PROBLEM_IMPL_H_
#define CERES_PUBLIC_PROBLEM_IMPL_H_

#include <array>
#include <map>
#include <memory>
#include <unordered_set>
#include <vector>

#include "ceres/context_impl.h"
#include "ceres/internal/port.h"
#include "ceres/problem.h"
#include "ceres/types.h"

namespace ceres {

class CostFunction;
class EvaluationCallback;
class LossFunction;
class LocalParameterization;
struct CRSMatrix;

namespace internal {

class Program;
class ResidualBlock;

class ProblemImpl {
 public:
  typedef std::map<double*, ParameterBlock*> ParameterMap;
  typedef std::unordered_set<ResidualBlock*> ResidualBlockSet;
  typedef std::map<CostFunction*, int> CostFunctionRefCount;
  typedef std::map<LossFunction*, int> LossFunctionRefCount;

  ProblemImpl();
  explicit ProblemImpl(const Problem::Options& options);
  ProblemImpl(const ProblemImpl&) = delete;
  void operator=(const ProblemImpl&) = delete;

  ~ProblemImpl();

  // See the public problem.h file for description of these methods.
  ResidualBlockId AddResidualBlock(CostFunction* cost_function,
                                   LossFunction* loss_function,
                                   double* const* const parameter_blocks,
                                   int num_parameter_blocks);

  template <typename... Ts>
  ResidualBlockId AddResidualBlock(CostFunction* cost_function,
                                   LossFunction* loss_function,
                                   double* x0,
                                   Ts*... xs) {
    const std::array<double*, sizeof...(Ts) + 1> parameter_blocks{{x0, xs...}};
    return AddResidualBlock(cost_function,
                            loss_function,
                            parameter_blocks.data(),
                            static_cast<int>(parameter_blocks.size()));
  }

  void AddParameterBlock(double* values, int size);
  void AddParameterBlock(double* values,
                         int size,
                         LocalParameterization* local_parameterization);

  void RemoveResidualBlock(ResidualBlock* residual_block);
  void RemoveParameterBlock(const double* values);

  void SetParameterBlockConstant(const double* values);
  void SetParameterBlockVariable(double* values);
  bool IsParameterBlockConstant(const double* values) const;

  void SetParameterization(double* values,
                           LocalParameterization* local_parameterization);
  const LocalParameterization* GetParameterization(const double* values) const;

  void SetParameterLowerBound(double* values, int index, double lower_bound);
  void SetParameterUpperBound(double* values, int index, double upper_bound);
  double GetParameterLowerBound(const double* values, int index) const;
  double GetParameterUpperBound(const double* values, int index) const;

  bool Evaluate(const Problem::EvaluateOptions& options,
                double* cost,
                std::vector<double>* residuals,
                std::vector<double>* gradient,
                CRSMatrix* jacobian);

  bool EvaluateResidualBlock(ResidualBlock* residual_block,
                             bool apply_loss_function,
                             double* cost,
                             double* residuals,
                             double** jacobians) const;

  int NumParameterBlocks() const;
  int NumParameters() const;
  int NumResidualBlocks() const;
  int NumResiduals() const;

  int ParameterBlockSize(const double* parameter_block) const;
  int ParameterBlockLocalSize(const double* parameter_block) const;

  bool HasParameterBlock(const double* parameter_block) const;

  void GetParameterBlocks(std::vector<double*>* parameter_blocks) const;
  void GetResidualBlocks(std::vector<ResidualBlockId>* residual_blocks) const;

  void GetParameterBlocksForResidualBlock(
      const ResidualBlockId residual_block,
      std::vector<double*>* parameter_blocks) const;

  const CostFunction* GetCostFunctionForResidualBlock(
      const ResidualBlockId residual_block) const;
  const LossFunction* GetLossFunctionForResidualBlock(
      const ResidualBlockId residual_block) const;

  void GetResidualBlocksForParameterBlock(
      const double* values,
      std::vector<ResidualBlockId>* residual_blocks) const;

  const Program& program() const { return *program_; }
  Program* mutable_program() { return program_.get(); }

  const ParameterMap& parameter_map() const { return parameter_block_map_; }
  const ResidualBlockSet& residual_block_set() const {
    CHECK(options_.enable_fast_removal)
        << "Fast removal not enabled, residual_block_set is not maintained.";
    return residual_block_set_;
  }

  ContextImpl* context() { return context_impl_; }

 private:
  ParameterBlock* InternalAddParameterBlock(double* values, int size);
  void InternalRemoveResidualBlock(ResidualBlock* residual_block);

  // Delete the arguments in question. These differ from the Remove* functions
  // in that they do not clean up references to the block to delete; they
  // merely delete them.
  template <typename Block>
  void DeleteBlockInVector(std::vector<Block*>* mutable_blocks,
                           Block* block_to_remove);
  void DeleteBlock(ResidualBlock* residual_block);
  void DeleteBlock(ParameterBlock* parameter_block);

  const Problem::Options options_;

  bool context_impl_owned_;
  ContextImpl* context_impl_;

  // The mapping from user pointers to parameter blocks.
  ParameterMap parameter_block_map_;

  // Iff enable_fast_removal is enabled, contains the current residual blocks.
  ResidualBlockSet residual_block_set_;

  // The actual parameter and residual blocks.
  std::unique_ptr<internal::Program> program_;

  // When removing parameter blocks, parameterizations have ambiguous
  // ownership. Instead of scanning the entire problem to see if the
  // parameterization is shared with other parameter blocks, buffer
  // them until destruction.
  //
  // TODO(keir): See if it makes sense to use sets instead.
  std::vector<LocalParameterization*> local_parameterizations_to_delete_;

  // For each cost function and loss function in the problem, a count
  // of the number of residual blocks that refer to them. When the
  // count goes to zero and the problem owns these objects, they are
  // destroyed.
  CostFunctionRefCount cost_function_ref_count_;
  LossFunctionRefCount loss_function_ref_count_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_PUBLIC_PROBLEM_IMPL_H_
