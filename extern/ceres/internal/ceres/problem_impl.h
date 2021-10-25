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

#include <map>
#include <vector>

#include "ceres/internal/macros.h"
#include "ceres/internal/port.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/collections_port.h"
#include "ceres/problem.h"
#include "ceres/types.h"

namespace ceres {

class CostFunction;
class LossFunction;
class LocalParameterization;
struct CRSMatrix;

namespace internal {

class Program;
class ResidualBlock;

class ProblemImpl {
 public:
  typedef std::map<double*, ParameterBlock*> ParameterMap;
  typedef HashSet<ResidualBlock*> ResidualBlockSet;

  ProblemImpl();
  explicit ProblemImpl(const Problem::Options& options);

  ~ProblemImpl();

  // See the public problem.h file for description of these methods.
  ResidualBlockId AddResidualBlock(
      CostFunction* cost_function,
      LossFunction* loss_function,
      const std::vector<double*>& parameter_blocks);
  ResidualBlockId AddResidualBlock(CostFunction* cost_function,
                                   LossFunction* loss_function,
                                   double* x0);
  ResidualBlockId AddResidualBlock(CostFunction* cost_function,
                                   LossFunction* loss_function,
                                   double* x0, double* x1);
  ResidualBlockId AddResidualBlock(CostFunction* cost_function,
                                   LossFunction* loss_function,
                                   double* x0, double* x1, double* x2);
  ResidualBlockId AddResidualBlock(CostFunction* cost_function,
                                   LossFunction* loss_function,
                                   double* x0, double* x1, double* x2,
                                   double* x3);
  ResidualBlockId AddResidualBlock(CostFunction* cost_function,
                                   LossFunction* loss_function,
                                   double* x0, double* x1, double* x2,
                                   double* x3, double* x4);
  ResidualBlockId AddResidualBlock(CostFunction* cost_function,
                                   LossFunction* loss_function,
                                   double* x0, double* x1, double* x2,
                                   double* x3, double* x4, double* x5);
  ResidualBlockId AddResidualBlock(CostFunction* cost_function,
                                   LossFunction* loss_function,
                                   double* x0, double* x1, double* x2,
                                   double* x3, double* x4, double* x5,
                                   double* x6);
  ResidualBlockId AddResidualBlock(CostFunction* cost_function,
                                   LossFunction* loss_function,
                                   double* x0, double* x1, double* x2,
                                   double* x3, double* x4, double* x5,
                                   double* x6, double* x7);
  ResidualBlockId AddResidualBlock(CostFunction* cost_function,
                                   LossFunction* loss_function,
                                   double* x0, double* x1, double* x2,
                                   double* x3, double* x4, double* x5,
                                   double* x6, double* x7, double* x8);
  ResidualBlockId AddResidualBlock(CostFunction* cost_function,
                                   LossFunction* loss_function,
                                   double* x0, double* x1, double* x2,
                                   double* x3, double* x4, double* x5,
                                   double* x6, double* x7, double* x8,
                                   double* x9);
  void AddParameterBlock(double* values, int size);
  void AddParameterBlock(double* values,
                         int size,
                         LocalParameterization* local_parameterization);

  void RemoveResidualBlock(ResidualBlock* residual_block);
  void RemoveParameterBlock(double* values);

  void SetParameterBlockConstant(double* values);
  void SetParameterBlockVariable(double* values);
  bool IsParameterBlockConstant(double* values) const;

  void SetParameterization(double* values,
                           LocalParameterization* local_parameterization);
  const LocalParameterization* GetParameterization(double* values) const;

  void SetParameterLowerBound(double* values, int index, double lower_bound);
  void SetParameterUpperBound(double* values, int index, double upper_bound);

  bool Evaluate(const Problem::EvaluateOptions& options,
                double* cost,
                std::vector<double>* residuals,
                std::vector<double>* gradient,
                CRSMatrix* jacobian);

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

 private:
  ParameterBlock* InternalAddParameterBlock(double* values, int size);
  void InternalRemoveResidualBlock(ResidualBlock* residual_block);

  bool InternalEvaluate(Program* program,
                        double* cost,
                        std::vector<double>* residuals,
                        std::vector<double>* gradient,
                        CRSMatrix* jacobian);

  // Delete the arguments in question. These differ from the Remove* functions
  // in that they do not clean up references to the block to delete; they
  // merely delete them.
  template<typename Block>
  void DeleteBlockInVector(std::vector<Block*>* mutable_blocks,
                           Block* block_to_remove);
  void DeleteBlock(ResidualBlock* residual_block);
  void DeleteBlock(ParameterBlock* parameter_block);

  const Problem::Options options_;

  // The mapping from user pointers to parameter blocks.
  std::map<double*, ParameterBlock*> parameter_block_map_;

  // Iff enable_fast_removal is enabled, contains the current residual blocks.
  ResidualBlockSet residual_block_set_;

  // The actual parameter and residual blocks.
  internal::scoped_ptr<internal::Program> program_;

  // When removing residual and parameter blocks, cost/loss functions and
  // parameterizations have ambiguous ownership. Instead of scanning the entire
  // problem to see if the cost/loss/parameterization is shared with other
  // residual or parameter blocks, buffer them until destruction.
  //
  // TODO(keir): See if it makes sense to use sets instead.
  std::vector<CostFunction*> cost_functions_to_delete_;
  std::vector<LossFunction*> loss_functions_to_delete_;
  std::vector<LocalParameterization*> local_parameterizations_to_delete_;

  CERES_DISALLOW_COPY_AND_ASSIGN(ProblemImpl);
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_PUBLIC_PROBLEM_IMPL_H_
