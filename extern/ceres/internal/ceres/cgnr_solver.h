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
// Author: keir@google.com (Keir Mierle)

#ifndef CERES_INTERNAL_CGNR_SOLVER_H_
#define CERES_INTERNAL_CGNR_SOLVER_H_

#include <memory>

#include "ceres/conjugate_gradients_solver.h"
#include "ceres/cuda_sparse_matrix.h"
#include "ceres/cuda_vector.h"
#include "ceres/internal/export.h"
#include "ceres/linear_solver.h"

namespace ceres::internal {

class Preconditioner;

class BlockJacobiPreconditioner;

// A conjugate gradients on the normal equations solver. This directly solves
// for the solution to
//
//   (A^T A + D^T D)x = A^T b
//
// as required for solving for x in the least squares sense. Currently only
// block diagonal preconditioning is supported.
class CERES_NO_EXPORT CgnrSolver final : public BlockSparseMatrixSolver {
 public:
  explicit CgnrSolver(LinearSolver::Options options);
  CgnrSolver(const CgnrSolver&) = delete;
  void operator=(const CgnrSolver&) = delete;
  ~CgnrSolver() override;

  Summary SolveImpl(BlockSparseMatrix* A,
                    const double* b,
                    const LinearSolver::PerSolveOptions& per_solve_options,
                    double* x) final;

 private:
  const LinearSolver::Options options_;
  std::unique_ptr<Preconditioner> preconditioner_;
  Vector cg_solution_;
  Vector* scratch_[4] = {nullptr, nullptr, nullptr, nullptr};
};

#ifndef CERES_NO_CUDA
class CudaPreconditioner : public ConjugateGradientsLinearOperator<CudaVector> {
 public:
  virtual void Update(const CompressedRowSparseMatrix& A, const double* D) = 0;
  virtual ~CudaPreconditioner() = default;
};

// A Cuda-accelerated version of CgnrSolver.
// This solver assumes that the sparsity structure of A remains constant for its
// lifetime.
class CERES_NO_EXPORT CudaCgnrSolver final
    : public CompressedRowSparseMatrixSolver {
 public:
  explicit CudaCgnrSolver(LinearSolver::Options options);
  static std::unique_ptr<CudaCgnrSolver> Create(LinearSolver::Options options,
                                                std::string* error);
  ~CudaCgnrSolver() override;

  Summary SolveImpl(CompressedRowSparseMatrix* A,
                    const double* b,
                    const LinearSolver::PerSolveOptions& per_solve_options,
                    double* x) final;

 private:
  void CpuToGpuTransfer(const CompressedRowSparseMatrix& A,
                        const double* b,
                        const double* D);

  LinearSolver::Options options_;
  std::unique_ptr<CudaSparseMatrix> A_;
  std::unique_ptr<CudaVector> b_;
  std::unique_ptr<CudaVector> x_;
  std::unique_ptr<CudaVector> Atb_;
  std::unique_ptr<CudaVector> Ax_;
  std::unique_ptr<CudaVector> D_;
  std::unique_ptr<CudaPreconditioner> preconditioner_;
  CudaVector* scratch_[4] = {nullptr, nullptr, nullptr, nullptr};
};
#endif  // CERES_NO_CUDA

}  // namespace ceres::internal

#endif  // CERES_INTERNAL_CGNR_SOLVER_H_
