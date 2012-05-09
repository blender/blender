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
// Preconditioned Conjugate Gradients based solver for positive
// semidefinite linear systems.

#ifndef CERES_INTERNAL_CONJUGATE_GRADIENTS_SOLVER_H_
#define CERES_INTERNAL_CONJUGATE_GRADIENTS_SOLVER_H_

#include "ceres/linear_solver.h"
#include "ceres/internal/macros.h"

namespace ceres {
namespace internal {

class LinearOperator;

// This class implements the now classical Conjugate Gradients
// algorithm of Hestenes & Stiefel for solving postive semidefinite
// linear sytems. Optionally it can use a preconditioner also to
// reduce the condition number of the linear system and improve the
// convergence rate. Modern references for Conjugate Gradients are the
// books by Yousef Saad and Trefethen & Bau. This implementation of CG
// has been augmented with additional termination tests that are
// needed for forcing early termination when used as part of an
// inexact Newton solver.
//
// For more details see the documentation for
// LinearSolver::PerSolveOptions::r_tolerance and
// LinearSolver::PerSolveOptions::q_tolerance in linear_solver.h.
class ConjugateGradientsSolver : public LinearSolver {
 public:
  explicit ConjugateGradientsSolver(const LinearSolver::Options& options);
  virtual Summary Solve(LinearOperator* A,
                        const double* b,
                        const LinearSolver::PerSolveOptions& per_solve_options,
                        double* x);

 private:
  const LinearSolver::Options options_;
  DISALLOW_COPY_AND_ASSIGN(ConjugateGradientsSolver);
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_CONJUGATE_GRADIENTS_SOLVER_H_
