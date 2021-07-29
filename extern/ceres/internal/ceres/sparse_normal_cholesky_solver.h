// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2015 Google Inc. All rights reserved.
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
//
// A solver for sparse linear least squares problem based on solving
// the normal equations via a sparse cholesky factorization.

#ifndef CERES_INTERNAL_SPARSE_NORMAL_CHOLESKY_SOLVER_H_
#define CERES_INTERNAL_SPARSE_NORMAL_CHOLESKY_SOLVER_H_

#include <vector>

// This include must come before any #ifndef check on Ceres compile options.
#include "ceres/internal/port.h"

#include "ceres/internal/macros.h"
#include "ceres/linear_solver.h"
#include "ceres/suitesparse.h"
#include "ceres/cxsparse.h"

#ifdef CERES_USE_EIGEN_SPARSE
#include "Eigen/SparseCholesky"
#endif

namespace ceres {
namespace internal {

class CompressedRowSparseMatrix;

// Solves the normal equations (A'A + D'D) x = A'b, using the CHOLMOD sparse
// cholesky solver.
class SparseNormalCholeskySolver : public CompressedRowSparseMatrixSolver {
 public:
  explicit SparseNormalCholeskySolver(const LinearSolver::Options& options);
  virtual ~SparseNormalCholeskySolver();

 private:
  virtual LinearSolver::Summary SolveImpl(
      CompressedRowSparseMatrix* A,
      const double* b,
      const LinearSolver::PerSolveOptions& options,
      double* x);

  LinearSolver::Summary SolveImplUsingSuiteSparse(
      CompressedRowSparseMatrix* A,
      double* rhs_and_solution);


  LinearSolver::Summary SolveImplUsingCXSparse(
      CompressedRowSparseMatrix* A,
      double* rhs_and_solution);

  LinearSolver::Summary SolveImplUsingEigen(
      CompressedRowSparseMatrix* A,
      double* rhs_and_solution);

  void FreeFactorization();

  SuiteSparse ss_;
  // Cached factorization
  cholmod_factor* factor_;

  CXSparse cxsparse_;
  // Cached factorization
  cs_dis* cxsparse_factor_;

#ifdef CERES_USE_EIGEN_SPARSE

  // The preprocessor gymnastics here are dealing with the fact that
  // before version 3.2.2, Eigen did not support a third template
  // parameter to specify the ordering.
#if EIGEN_VERSION_AT_LEAST(3,2,2)
  typedef Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>, Eigen::Upper,
                                Eigen::NaturalOrdering<int> >
  SimplicialLDLTWithNaturalOrdering;
  scoped_ptr<SimplicialLDLTWithNaturalOrdering> natural_ldlt_;

  typedef Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>, Eigen::Upper,
                                Eigen::AMDOrdering<int> >
  SimplicialLDLTWithAMDOrdering;
  scoped_ptr<SimplicialLDLTWithAMDOrdering> amd_ldlt_;

#else
  typedef Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>, Eigen::Upper>
  SimplicialLDLTWithAMDOrdering;

  scoped_ptr<SimplicialLDLTWithAMDOrdering> amd_ldlt_;
#endif

#endif

  scoped_ptr<CompressedRowSparseMatrix> outer_product_;
  std::vector<int> pattern_;
  const LinearSolver::Options options_;
  CERES_DISALLOW_COPY_AND_ASSIGN(SparseNormalCholeskySolver);
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_SPARSE_NORMAL_CHOLESKY_SOLVER_H_
