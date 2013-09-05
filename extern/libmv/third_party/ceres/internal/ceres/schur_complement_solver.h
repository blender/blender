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

#ifndef CERES_INTERNAL_SCHUR_COMPLEMENT_SOLVER_H_
#define CERES_INTERNAL_SCHUR_COMPLEMENT_SOLVER_H_

#include <set>
#include <utility>
#include <vector>

#include "ceres/block_random_access_matrix.h"
#include "ceres/block_sparse_matrix.h"
#include "ceres/block_structure.h"
#include "ceres/cxsparse.h"
#include "ceres/linear_solver.h"
#include "ceres/schur_eliminator.h"
#include "ceres/suitesparse.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/types.h"

namespace ceres {
namespace internal {

class BlockSparseMatrix;

// Base class for Schur complement based linear least squares
// solvers. It assumes that the input linear system Ax = b can be
// partitioned into
//
//  E y + F z = b
//
// Where x = [y;z] is a partition of the variables.  The paritioning
// of the variables is such that, E'E is a block diagonal
// matrix. Further, the rows of A are ordered so that for every
// variable block in y, all the rows containing that variable block
// occur as a vertically contiguous block. i.e the matrix A looks like
//
//              E                 F
//  A = [ y1   0   0   0 |  z1    0    0   0    z5]
//      [ y1   0   0   0 |  z1   z2    0   0     0]
//      [  0  y2   0   0 |   0    0   z3   0     0]
//      [  0   0  y3   0 |  z1   z2   z3  z4    z5]
//      [  0   0  y3   0 |  z1    0    0   0    z5]
//      [  0   0   0  y4 |   0    0    0   0    z5]
//      [  0   0   0  y4 |   0   z2    0   0     0]
//      [  0   0   0  y4 |   0    0    0   0     0]
//      [  0   0   0   0 |  z1    0    0   0     0]
//      [  0   0   0   0 |   0    0   z3  z4    z5]
//
// This structure should be reflected in the corresponding
// CompressedRowBlockStructure object associated with A. The linear
// system Ax = b should either be well posed or the array D below
// should be non-null and the diagonal matrix corresponding to it
// should be non-singular.
//
// SchurComplementSolver has two sub-classes.
//
// DenseSchurComplementSolver: For problems where the Schur complement
// matrix is small and dense, or if CHOLMOD/SuiteSparse is not
// installed. For structure from motion problems, this is solver can
// be used for problems with upto a few hundred cameras.
//
// SparseSchurComplementSolver: For problems where the Schur
// complement matrix is large and sparse. It requires that
// CHOLMOD/SuiteSparse be installed, as it uses CHOLMOD to find a
// sparse Cholesky factorization of the Schur complement. This solver
// can be used for solving structure from motion problems with tens of
// thousands of cameras, though depending on the exact sparsity
// structure, it maybe better to use an iterative solver.
//
// The two solvers can be instantiated by calling
// LinearSolver::CreateLinearSolver with LinearSolver::Options::type
// set to DENSE_SCHUR and SPARSE_SCHUR
// respectively. LinearSolver::Options::elimination_groups[0] should be
// at least 1.
class SchurComplementSolver : public BlockSparseMatrixSolver {
 public:
  explicit SchurComplementSolver(const LinearSolver::Options& options)
      : options_(options) {
    CHECK_GT(options.elimination_groups.size(), 1);
    CHECK_GT(options.elimination_groups[0], 0);
  }

  // LinearSolver methods
  virtual ~SchurComplementSolver() {}
  virtual LinearSolver::Summary SolveImpl(
      BlockSparseMatrix* A,
      const double* b,
      const LinearSolver::PerSolveOptions& per_solve_options,
      double* x);

 protected:
  const LinearSolver::Options& options() const { return options_; }

  const BlockRandomAccessMatrix* lhs() const { return lhs_.get(); }
  void set_lhs(BlockRandomAccessMatrix* lhs) { lhs_.reset(lhs); }
  const double* rhs() const { return rhs_.get(); }
  void set_rhs(double* rhs) { rhs_.reset(rhs); }

 private:
  virtual void InitStorage(const CompressedRowBlockStructure* bs) = 0;
  virtual bool SolveReducedLinearSystem(double* solution) = 0;

  LinearSolver::Options options_;

  scoped_ptr<SchurEliminatorBase> eliminator_;
  scoped_ptr<BlockRandomAccessMatrix> lhs_;
  scoped_array<double> rhs_;

  CERES_DISALLOW_COPY_AND_ASSIGN(SchurComplementSolver);
};

// Dense Cholesky factorization based solver.
class DenseSchurComplementSolver : public SchurComplementSolver {
 public:
  explicit DenseSchurComplementSolver(const LinearSolver::Options& options)
      : SchurComplementSolver(options) {}
  virtual ~DenseSchurComplementSolver() {}

 private:
  virtual void InitStorage(const CompressedRowBlockStructure* bs);
  virtual bool SolveReducedLinearSystem(double* solution);

  CERES_DISALLOW_COPY_AND_ASSIGN(DenseSchurComplementSolver);
};

#if !defined(CERES_NO_SUITESPARSE) || !defined(CERES_NO_CXSPARE)
// Sparse Cholesky factorization based solver.
class SparseSchurComplementSolver : public SchurComplementSolver {
 public:
  explicit SparseSchurComplementSolver(const LinearSolver::Options& options);
  virtual ~SparseSchurComplementSolver();

 private:
  virtual void InitStorage(const CompressedRowBlockStructure* bs);
  virtual bool SolveReducedLinearSystem(double* solution);
  bool SolveReducedLinearSystemUsingSuiteSparse(double* solution);
  bool SolveReducedLinearSystemUsingCXSparse(double* solution);

  // Size of the blocks in the Schur complement.
  vector<int> blocks_;

  SuiteSparse ss_;
  // Symbolic factorization of the reduced linear system. Precomputed
  // once and reused in subsequent calls.
  cholmod_factor* factor_;

  CXSparse cxsparse_;
  // Cached factorization
  cs_dis* cxsparse_factor_;
  CERES_DISALLOW_COPY_AND_ASSIGN(SparseSchurComplementSolver);
};

#endif  // !defined(CERES_NO_SUITESPARSE) || !defined(CERES_NO_CXSPARE)
}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_SCHUR_COMPLEMENT_SOLVER_H_
