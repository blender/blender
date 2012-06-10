// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2010, 2011, 2012 Google Inc. All rights reserved.
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
//
// Implmentation of Levenberg Marquardt algorithm based on "Methods for
// Nonlinear Least Squares" by K. Madsen, H.B. Nielsen and
// O. Tingleff. Available to download from
//
// http://www2.imm.dtu.dk/pubdb/views/edoc_download.php/3215/pdf/imm3215.pdf
//

#ifndef CERES_INTERNAL_LEVENBERG_MARQUARDT_H_
#define CERES_INTERNAL_LEVENBERG_MARQUARDT_H_

#include "ceres/minimizer.h"
#include "ceres/solver.h"

namespace ceres {
namespace internal {

class Evaluator;
class LinearSolver;

class LevenbergMarquardt : public Minimizer {
 public:
  virtual ~LevenbergMarquardt();

  virtual void Minimize(const Minimizer::Options& options,
                        Evaluator* evaluator,
                        LinearSolver* linear_solver,
                        const double* initial_parameters,
                        double* final_parameters,
                        Solver::Summary* summary);
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_LEVENBERG_MARQUARDT_H_
