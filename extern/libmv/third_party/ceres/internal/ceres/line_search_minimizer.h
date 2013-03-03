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

#ifndef CERES_INTERNAL_LINE_SEARCH_MINIMIZER_H_
#define CERES_INTERNAL_LINE_SEARCH_MINIMIZER_H_

#include "ceres/minimizer.h"
#include "ceres/solver.h"
#include "ceres/types.h"
#include "ceres/internal/eigen.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

// Generic line search minimization algorithm.
//
// For example usage, see SolverImpl::Minimize.
class LineSearchMinimizer : public Minimizer {
 public:
  struct State {
    State(int num_parameters,
          int num_effective_parameters)
        : cost(0.0),
          gradient(num_effective_parameters),
          gradient_squared_norm(0.0),
          search_direction(num_effective_parameters),
          directional_derivative(0.0),
          step_size(0.0) {
    }

    double cost;
    Vector gradient;
    double gradient_squared_norm;
    double gradient_max_norm;
    Vector search_direction;
    double directional_derivative;
    double step_size;
  };

  ~LineSearchMinimizer() {}
  virtual void Minimize(const Minimizer::Options& options,
                        double* parameters,
                        Solver::Summary* summary);
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_LINE_SEARCH_MINIMIZER_H_
