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

#ifndef CERES_INTERNAL_ITERATIVE_SCHUR_COMPLEMENT_SOLVER_H_
#define CERES_INTERNAL_ITERATIVE_SCHUR_COMPLEMENT_SOLVER_H_

#include "ceres/linear_solver.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/types.h"

namespace ceres {
namespace internal {

class BlockSparseMatrix;
class ImplicitSchurComplement;
class Preconditioner;

// This class implements an iterative solver for the linear least
// squares problems that have a bi-partite sparsity structure common
// to Structure from Motion problems.
//
// The algorithm used by this solver was developed in a series of
// papers - "Agarwal et al, Bundle Adjustment in the Large, ECCV 2010"
// and "Wu et al, Multicore Bundle Adjustment, submitted to CVPR
// 2011" at the Univeristy of Washington.
//
// The key idea is that one can run Conjugate Gradients on the Schur
// Complement system without explicitly forming the Schur Complement
// in memory. The heavy lifting for this is done by the
// ImplicitSchurComplement class. Not forming the Schur complement in
// memory and factoring it results in substantial savings in time and
// memory. Further, iterative solvers like this open up the
// possibility of solving the Newton equations in a non-linear solver
// only approximately and terminating early, thereby saving even more
// time.
//
// For the curious, running CG on the Schur complement is the same as
// running CG on the Normal Equations with an SSOR preconditioner. For
// a proof of this fact and others related to this solver please see
// the section on Domain Decomposition Methods in Saad's book
// "Iterative Methods for Sparse Linear Systems".
class IterativeSchurComplementSolver : public BlockSparseMatrixSolver {
 public:
  explicit IterativeSchurComplementSolver(const LinearSolver::Options& options);
  virtual ~IterativeSchurComplementSolver();

 private:
  virtual LinearSolver::Summary SolveImpl(
      BlockSparseMatrix* A,
      const double* b,
      const LinearSolver::PerSolveOptions& options,
      double* x);

  LinearSolver::Options options_;
  scoped_ptr<internal::ImplicitSchurComplement> schur_complement_;
  scoped_ptr<Preconditioner> preconditioner_;
  Vector reduced_linear_system_solution_;
  CERES_DISALLOW_COPY_AND_ASSIGN(IterativeSchurComplementSolver);
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_ITERATIVE_SCHUR_COMPLEMENT_SOLVER_H_
