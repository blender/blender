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
//         keir@google.com (Keir Mierle)

#include "ceres/problem.h"

#include <vector>
#include "ceres/crs_matrix.h"
#include "ceres/problem_impl.h"

namespace ceres {

Problem::Problem() : problem_impl_(new internal::ProblemImpl) {}
Problem::Problem(const Problem::Options& options)
    : problem_impl_(new internal::ProblemImpl(options)) {}
Problem::~Problem() {}

ResidualBlockId Problem::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    const vector<double*>& parameter_blocks) {
  return problem_impl_->AddResidualBlock(cost_function,
                                         loss_function,
                                         parameter_blocks);
}

ResidualBlockId Problem::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* x0) {
  return problem_impl_->AddResidualBlock(cost_function,
                                         loss_function,
                                         x0);
}

ResidualBlockId Problem::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* x0, double* x1) {
  return problem_impl_->AddResidualBlock(cost_function,
                                         loss_function,
                                         x0, x1);
}

ResidualBlockId Problem::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* x0, double* x1, double* x2) {
  return problem_impl_->AddResidualBlock(cost_function,
                                         loss_function,
                                         x0, x1, x2);
}

ResidualBlockId Problem::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* x0, double* x1, double* x2, double* x3) {
  return problem_impl_->AddResidualBlock(cost_function,
                                         loss_function,
                                         x0, x1, x2, x3);
}

ResidualBlockId Problem::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* x0, double* x1, double* x2, double* x3, double* x4) {
  return problem_impl_->AddResidualBlock(cost_function,
                                         loss_function,
                                         x0, x1, x2, x3, x4);
}

ResidualBlockId Problem::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* x0, double* x1, double* x2, double* x3, double* x4, double* x5) {
  return problem_impl_->AddResidualBlock(cost_function,
                                         loss_function,
                                         x0, x1, x2, x3, x4, x5);
}

ResidualBlockId Problem::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* x0, double* x1, double* x2, double* x3, double* x4, double* x5,
    double* x6) {
  return problem_impl_->AddResidualBlock(cost_function,
                                         loss_function,
                                         x0, x1, x2, x3, x4, x5, x6);
}

ResidualBlockId Problem::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* x0, double* x1, double* x2, double* x3, double* x4, double* x5,
    double* x6, double* x7) {
  return problem_impl_->AddResidualBlock(cost_function,
                                         loss_function,
                                         x0, x1, x2, x3, x4, x5, x6, x7);
}

ResidualBlockId Problem::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* x0, double* x1, double* x2, double* x3, double* x4, double* x5,
    double* x6, double* x7, double* x8) {
  return problem_impl_->AddResidualBlock(cost_function,
                                         loss_function,
                                         x0, x1, x2, x3, x4, x5, x6, x7, x8);
}

ResidualBlockId Problem::AddResidualBlock(
    CostFunction* cost_function,
    LossFunction* loss_function,
    double* x0, double* x1, double* x2, double* x3, double* x4, double* x5,
    double* x6, double* x7, double* x8, double* x9) {
  return problem_impl_->AddResidualBlock(
      cost_function,
      loss_function,
      x0, x1, x2, x3, x4, x5, x6, x7, x8, x9);
}

void Problem::AddParameterBlock(double* values, int size) {
  problem_impl_->AddParameterBlock(values, size);
}

void Problem::AddParameterBlock(double* values,
                                int size,
                                LocalParameterization* local_parameterization) {
  problem_impl_->AddParameterBlock(values, size, local_parameterization);
}

void Problem::RemoveResidualBlock(ResidualBlockId residual_block) {
  problem_impl_->RemoveResidualBlock(residual_block);
}

void Problem::RemoveParameterBlock(double* values) {
  problem_impl_->RemoveParameterBlock(values);
}

void Problem::SetParameterBlockConstant(double* values) {
  problem_impl_->SetParameterBlockConstant(values);
}

void Problem::SetParameterBlockVariable(double* values) {
  problem_impl_->SetParameterBlockVariable(values);
}

void Problem::SetParameterization(
    double* values,
    LocalParameterization* local_parameterization) {
  problem_impl_->SetParameterization(values, local_parameterization);
}

const LocalParameterization* Problem::GetParameterization(
    double* values) const {
  return problem_impl_->GetParameterization(values);
}

void Problem::SetParameterLowerBound(double* values,
                                     int index,
                                     double lower_bound) {
  problem_impl_->SetParameterLowerBound(values, index, lower_bound);
}

void Problem::SetParameterUpperBound(double* values,
                                     int index,
                                     double upper_bound) {
  problem_impl_->SetParameterUpperBound(values, index, upper_bound);
}

bool Problem::Evaluate(const EvaluateOptions& evaluate_options,
                       double* cost,
                       vector<double>* residuals,
                       vector<double>* gradient,
                       CRSMatrix* jacobian) {
  return problem_impl_->Evaluate(evaluate_options,
                                 cost,
                                 residuals,
                                 gradient,
                                 jacobian);
}

int Problem::NumParameterBlocks() const {
  return problem_impl_->NumParameterBlocks();
}

int Problem::NumParameters() const {
  return problem_impl_->NumParameters();
}

int Problem::NumResidualBlocks() const {
  return problem_impl_->NumResidualBlocks();
}

int Problem::NumResiduals() const {
  return problem_impl_->NumResiduals();
}

int Problem::ParameterBlockSize(const double* parameter_block) const {
  return problem_impl_->ParameterBlockSize(parameter_block);
};

int Problem::ParameterBlockLocalSize(const double* parameter_block) const {
  return problem_impl_->ParameterBlockLocalSize(parameter_block);
};

bool Problem::HasParameterBlock(const double* values) const {
  return problem_impl_->HasParameterBlock(values);
}

void Problem::GetParameterBlocks(vector<double*>* parameter_blocks) const {
  problem_impl_->GetParameterBlocks(parameter_blocks);
}

void Problem::GetResidualBlocks(
    vector<ResidualBlockId>* residual_blocks) const {
  problem_impl_->GetResidualBlocks(residual_blocks);
}

void Problem::GetParameterBlocksForResidualBlock(
    const ResidualBlockId residual_block,
    vector<double*>* parameter_blocks) const {
  problem_impl_->GetParameterBlocksForResidualBlock(residual_block,
                                                    parameter_blocks);
}

void Problem::GetResidualBlocksForParameterBlock(
    const double* values,
    vector<ResidualBlockId>* residual_blocks) const {
  problem_impl_->GetResidualBlocksForParameterBlock(values,
                                                    residual_blocks);
}

}  // namespace ceres
