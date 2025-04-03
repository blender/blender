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

#include "ceres/covariance.h"

#include <utility>
#include <vector>

#include "ceres/covariance_impl.h"
#include "ceres/problem.h"
#include "ceres/problem_impl.h"

namespace ceres {

Covariance::Covariance(const Covariance::Options& options) {
  impl_ = std::make_unique<internal::CovarianceImpl>(options);
}

Covariance::~Covariance() = default;

bool Covariance::Compute(
    const std::vector<std::pair<const double*, const double*>>&
        covariance_blocks,
    Problem* problem) {
  return impl_->Compute(covariance_blocks, problem->mutable_impl());
}

bool Covariance::Compute(const std::vector<const double*>& parameter_blocks,
                         Problem* problem) {
  return impl_->Compute(parameter_blocks, problem->mutable_impl());
}

bool Covariance::GetCovarianceBlock(const double* parameter_block1,
                                    const double* parameter_block2,
                                    double* covariance_block) const {
  return impl_->GetCovarianceBlockInTangentOrAmbientSpace(parameter_block1,
                                                          parameter_block2,
                                                          true,  // ambient
                                                          covariance_block);
}

bool Covariance::GetCovarianceBlockInTangentSpace(
    const double* parameter_block1,
    const double* parameter_block2,
    double* covariance_block) const {
  return impl_->GetCovarianceBlockInTangentOrAmbientSpace(parameter_block1,
                                                          parameter_block2,
                                                          false,  // tangent
                                                          covariance_block);
}

bool Covariance::GetCovarianceMatrix(
    const std::vector<const double*>& parameter_blocks,
    double* covariance_matrix) const {
  return impl_->GetCovarianceMatrixInTangentOrAmbientSpace(parameter_blocks,
                                                           true,  // ambient
                                                           covariance_matrix);
}

bool Covariance::GetCovarianceMatrixInTangentSpace(
    const std::vector<const double*>& parameter_blocks,
    double* covariance_matrix) const {
  return impl_->GetCovarianceMatrixInTangentOrAmbientSpace(parameter_blocks,
                                                           false,  // tangent
                                                           covariance_matrix);
}

}  // namespace ceres
