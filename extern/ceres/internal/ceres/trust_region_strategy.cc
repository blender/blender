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
//
// Author: sameeragarwal@google.com (Sameer Agarwal)
//         keir@google.com (Keir Mierle)

#include "ceres/trust_region_strategy.h"

#include <memory>

#include "ceres/dogleg_strategy.h"
#include "ceres/levenberg_marquardt_strategy.h"

namespace ceres::internal {

TrustRegionStrategy::~TrustRegionStrategy() = default;

std::unique_ptr<TrustRegionStrategy> TrustRegionStrategy::Create(
    const Options& options) {
  switch (options.trust_region_strategy_type) {
    case LEVENBERG_MARQUARDT:
      return std::make_unique<LevenbergMarquardtStrategy>(options);
    case DOGLEG:
      return std::make_unique<DoglegStrategy>(options);
    default:
      LOG(FATAL) << "Unknown trust region strategy: "
                 << options.trust_region_strategy_type;
  }

  LOG(FATAL) << "Unknown trust region strategy: "
             << options.trust_region_strategy_type;
  return nullptr;
}

}  // namespace ceres::internal
