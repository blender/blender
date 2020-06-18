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

#include "ceres/problem.h"

#include <vector>

#include "ceres/crs_matrix.h"
#include "ceres/problem_impl.h"

namespace ceres {

using std::vector;

Problem::Problem() : impl_(new internal::ProblemImpl) {}
Problem::Problem(const Problem::Options& options)
    : impl_(new internal::ProblemImpl(options)) {}
// Not inline defaulted in declaration due to use of std::unique_ptr.
Problem::Problem(Problem&&) = default;
Problem& Problem::operator=(Problem&&) = default;
Problem::~Problem() {}

ResidualBlockId Problem::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    const vector<double*>& parameter_blocks) {
  return impl_->AddResidualBlock(cost_function,
                                 loss_function,
                                 parameter_blocks.data(),
                                 static_cast<int>(parameter_blocks.size()));
}

ResidualBlockId Problem::AddResidualBlock(CostFunction* cost_function,
                                          LossFunction* loss_function,
                                          double* const* const parameter_blocks,
                                          int num_parameter_blocks) {
  return impl_->AddResidualBlock(
      cost_function, loss_function, parameter_blocks, num_parameter_blocks);
}

void Problem::AddParameterBlock(double* values, int size) {
  impl_->AddParameterBlock(values, size);
}

void Problem::AddParameterBlock(double* values,
                                int size,
                                LocalParameterization* local_parameterization) {
  impl_->AddParameterBlock(values, size, local_parameterization);
}

void Problem::RemoveResidualBlock(ResidualBlockId residual_block) {
  impl_->RemoveResidualBlock(residual_block);
}

void Problem::RemoveParameterBlock(const double* values) {
  impl_->RemoveParameterBlock(values);
}

void Problem::SetParameterBlockConstant(const double* values) {
  impl_->SetParameterBlockConstant(values);
}

void Problem::SetParameterBlockVariable(double* values) {
  impl_->SetParameterBlockVariable(values);
}

bool Problem::IsParameterBlockConstant(const double* values) const {
  return impl_->IsParameterBlockConstant(values);
}

void Problem::SetParameterization(
    double* values, LocalParameterization* local_parameterization) {
  impl_->SetParameterization(values, local_parameterization);
}

const LocalParameterization* Problem::GetParameterization(
    const double* values) const {
  return impl_->GetParameterization(values);
}

void Problem::SetParameterLowerBound(double* values,
                                     int index,
                                     double lower_bound) {
  impl_->SetParameterLowerBound(values, index, lower_bound);
}

void Problem::SetParameterUpperBound(double* values,
                                     int index,
                                     double upper_bound) {
  impl_->SetParameterUpperBound(values, index, upper_bound);
}

double Problem::GetParameterUpperBound(const double* values, int index) const {
  return impl_->GetParameterUpperBound(values, index);
}

double Problem::GetParameterLowerBound(const double* values, int index) const {
  return impl_->GetParameterLowerBound(values, index);
}

bool Problem::Evaluate(const EvaluateOptions& evaluate_options,
                       double* cost,
                       vector<double>* residuals,
                       vector<double>* gradient,
                       CRSMatrix* jacobian) {
  return impl_->Evaluate(evaluate_options, cost, residuals, gradient, jacobian);
}

bool Problem::EvaluateResidualBlock(ResidualBlockId residual_block_id,
                                    bool apply_loss_function,
                                    double* cost,
                                    double* residuals,
                                    double** jacobians) const {
  return impl_->EvaluateResidualBlock(
      residual_block_id, apply_loss_function, cost, residuals, jacobians);
}

int Problem::NumParameterBlocks() const { return impl_->NumParameterBlocks(); }

int Problem::NumParameters() const { return impl_->NumParameters(); }

int Problem::NumResidualBlocks() const { return impl_->NumResidualBlocks(); }

int Problem::NumResiduals() const { return impl_->NumResiduals(); }

int Problem::ParameterBlockSize(const double* parameter_block) const {
  return impl_->ParameterBlockSize(parameter_block);
}

int Problem::ParameterBlockLocalSize(const double* parameter_block) const {
  return impl_->ParameterBlockLocalSize(parameter_block);
}

bool Problem::HasParameterBlock(const double* values) const {
  return impl_->HasParameterBlock(values);
}

void Problem::GetParameterBlocks(vector<double*>* parameter_blocks) const {
  impl_->GetParameterBlocks(parameter_blocks);
}

void Problem::GetResidualBlocks(
    vector<ResidualBlockId>* residual_blocks) const {
  impl_->GetResidualBlocks(residual_blocks);
}

void Problem::GetParameterBlocksForResidualBlock(
    const ResidualBlockId residual_block,
    vector<double*>* parameter_blocks) const {
  impl_->GetParameterBlocksForResidualBlock(residual_block, parameter_blocks);
}

const CostFunction* Problem::GetCostFunctionForResidualBlock(
    const ResidualBlockId residual_block) const {
  return impl_->GetCostFunctionForResidualBlock(residual_block);
}

const LossFunction* Problem::GetLossFunctionForResidualBlock(
    const ResidualBlockId residual_block) const {
  return impl_->GetLossFunctionForResidualBlock(residual_block);
}

void Problem::GetResidualBlocksForParameterBlock(
    const double* values, vector<ResidualBlockId>* residual_blocks) const {
  impl_->GetResidualBlocksForParameterBlock(values, residual_blocks);
}

}  // namespace ceres
