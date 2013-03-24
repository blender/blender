// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2012 Google Inc. All rights reserved.
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

#ifndef CERES_NO_LINE_SEARCH_MINIMIZER

#include "ceres/line_search_direction.h"
#include "ceres/line_search_minimizer.h"
#include "ceres/low_rank_inverse_hessian.h"
#include "ceres/internal/eigen.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

class SteepestDescent : public LineSearchDirection {
 public:
  virtual ~SteepestDescent() {}
  bool NextDirection(const LineSearchMinimizer::State& previous,
                     const LineSearchMinimizer::State& current,
                     Vector* search_direction) {
    *search_direction = -current.gradient;
    return true;
  }
};

class NonlinearConjugateGradient : public LineSearchDirection {
 public:
  NonlinearConjugateGradient(const NonlinearConjugateGradientType type,
                             const double function_tolerance)
      : type_(type),
        function_tolerance_(function_tolerance) {
  }

  bool NextDirection(const LineSearchMinimizer::State& previous,
                     const LineSearchMinimizer::State& current,
                     Vector* search_direction) {
    double beta = 0.0;
    Vector gradient_change;
    switch (type_) {
      case FLETCHER_REEVES:
        beta = current.gradient_squared_norm / previous.gradient_squared_norm;
        break;
      case POLAK_RIBIRERE:
        gradient_change = current.gradient - previous.gradient;
        beta = (current.gradient.dot(gradient_change) /
                previous.gradient_squared_norm);
        break;
      case HESTENES_STIEFEL:
        gradient_change = current.gradient - previous.gradient;
        beta =  (current.gradient.dot(gradient_change) /
                 previous.search_direction.dot(gradient_change));
        break;
      default:
        LOG(FATAL) << "Unknown nonlinear conjugate gradient type: " << type_;
    }

    *search_direction =  -current.gradient + beta * previous.search_direction;
    const double directional_derivative =
        current.gradient.dot(*search_direction);
    if (directional_derivative > -function_tolerance_) {
      LOG(WARNING) << "Restarting non-linear conjugate gradients: "
                   << directional_derivative;
      *search_direction = -current.gradient;
    };

    return true;
  }

 private:
  const NonlinearConjugateGradientType type_;
  const double function_tolerance_;
};

class LBFGS : public LineSearchDirection {
 public:
  LBFGS(const int num_parameters, const int max_lbfgs_rank)
      : low_rank_inverse_hessian_(num_parameters, max_lbfgs_rank) {}

  virtual ~LBFGS() {}

  bool NextDirection(const LineSearchMinimizer::State& previous,
                     const LineSearchMinimizer::State& current,
                     Vector* search_direction) {
    low_rank_inverse_hessian_.Update(
        previous.search_direction * previous.step_size,
        current.gradient - previous.gradient);
    search_direction->setZero();
    low_rank_inverse_hessian_.RightMultiply(current.gradient.data(),
                                            search_direction->data());
    *search_direction *= -1.0;
    return true;
  }

 private:
  LowRankInverseHessian low_rank_inverse_hessian_;
};

LineSearchDirection*
LineSearchDirection::Create(const LineSearchDirection::Options& options) {
  if (options.type == STEEPEST_DESCENT) {
    return new SteepestDescent;
  }

  if (options.type == NONLINEAR_CONJUGATE_GRADIENT) {
    return new NonlinearConjugateGradient(
        options.nonlinear_conjugate_gradient_type,
        options.function_tolerance);
  }

  if (options.type == ceres::LBFGS) {
    return new ceres::internal::LBFGS(options.num_parameters,
                                      options.max_lbfgs_rank);
  }

  LOG(ERROR) << "Unknown line search direction type: " << options.type;
  return NULL;
}

}  // namespace internal
}  // namespace ceres

#endif  // CERES_NO_LINE_SEARCH_MINIMIZER
