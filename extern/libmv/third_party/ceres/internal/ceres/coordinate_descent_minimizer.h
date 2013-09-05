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

#ifndef CERES_INTERNAL_COORDINATE_DESCENT_MINIMIZER_H_
#define CERES_INTERNAL_COORDINATE_DESCENT_MINIMIZER_H_

#include <string>
#include <vector>

#include "ceres/evaluator.h"
#include "ceres/minimizer.h"
#include "ceres/problem_impl.h"
#include "ceres/program.h"
#include "ceres/solver.h"

namespace ceres {
namespace internal {

// Given a Program, and a ParameterBlockOrdering which partitions
// (non-exhaustively) the Hessian matrix into independent sets,
// perform coordinate descent on the parameter blocks in the
// ordering. The independent set structure allows for all parameter
// blocks in the same independent set to be optimized in parallel, and
// the order of the independent set determines the order in which the
// parameter block groups are optimized.
//
// The minimizer assumes that none of the parameter blocks in the
// program are constant.
class CoordinateDescentMinimizer : public Minimizer {
 public:
  bool Init(const Program& program,
            const ProblemImpl::ParameterMap& parameter_map,
            const ParameterBlockOrdering& ordering,
            string* error);

  // Minimizer interface.
  virtual ~CoordinateDescentMinimizer();
  virtual void Minimize(const Minimizer::Options& options,
                        double* parameters,
                        Solver::Summary* summary);

 private:
  void Solve(Program* program,
             LinearSolver* linear_solver,
             double* parameters,
             Solver::Summary* summary);

  vector<ParameterBlock*> parameter_blocks_;
  vector<vector<ResidualBlock*> > residual_blocks_;
  // The optimization is performed in rounds. In each round all the
  // parameter blocks that form one independent set are optimized in
  // parallel. This array, marks the boundaries of the independent
  // sets in parameter_blocks_.
  vector<int> independent_set_offsets_;

  Evaluator::Options evaluator_options_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_COORDINATE_DESCENT_MINIMIZER_H_
