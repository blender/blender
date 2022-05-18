// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2022 Google Inc. All rights reserved.
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

#ifndef CERES_INTERNAL_DENSE_CHOLESKY_H_
#define CERES_INTERNAL_DENSE_CHOLESKY_H_

// This include must come before any #ifndef check on Ceres compile options.
// clang-format off
#include "ceres/internal/config.h"
// clang-format on

#include <memory>
#include <vector>

#include "Eigen/Dense"
#include "ceres/cuda_buffer.h"
#include "ceres/linear_solver.h"
#include "glog/logging.h"
#ifndef CERES_NO_CUDA
#include "ceres/context_impl.h"
#include "cuda_runtime.h"
#include "cusolverDn.h"
#endif  // CERES_NO_CUDA

namespace ceres {
namespace internal {

// An interface that abstracts away the internal details of various dense linear
// algebra libraries and offers a simple API for solving dense symmetric
// positive definite linear systems using a Cholesky factorization.
class CERES_NO_EXPORT DenseCholesky {
 public:
  static std::unique_ptr<DenseCholesky> Create(
      const LinearSolver::Options& options);

  virtual ~DenseCholesky();

  // Computes the Cholesky factorization of the given matrix.
  //
  // The input matrix lhs is assumed to be a column-major num_cols x num_cols
  // matrix, that is symmetric positive definite with its lower triangular part
  // containing the left hand side of the linear system being solved.
  //
  // The input matrix lhs may be modified by the implementation to store the
  // factorization, irrespective of whether the factorization succeeds or not.
  // As a result it is the user's responsibility to ensure that lhs is valid
  // when Solve is called.
  virtual LinearSolverTerminationType Factorize(int num_cols,
                                                double* lhs,
                                                std::string* message) = 0;

  // Computes the solution to the equation
  //
  // lhs * solution = rhs
  //
  // Calling Solve without calling Factorize is undefined behaviour. It is the
  // user's responsibility to ensure that the input matrix lhs passed to
  // Factorize has not been freed/modified when Solve is called.
  virtual LinearSolverTerminationType Solve(const double* rhs,
                                            double* solution,
                                            std::string* message) = 0;

  // Convenience method which combines a call to Factorize and Solve. Solve is
  // only called if Factorize returns LINEAR_SOLVER_SUCCESS.
  //
  // The input matrix lhs may be modified by the implementation to store the
  // factorization, irrespective of whether the method succeeds or not. It is
  // the user's responsibility to ensure that lhs is valid if and when Solve is
  // called again after this call.
  LinearSolverTerminationType FactorAndSolve(int num_cols,
                                             double* lhs,
                                             const double* rhs,
                                             double* solution,
                                             std::string* message);
};

class CERES_NO_EXPORT EigenDenseCholesky final : public DenseCholesky {
 public:
  LinearSolverTerminationType Factorize(int num_cols,
                                        double* lhs,
                                        std::string* message) override;
  LinearSolverTerminationType Solve(const double* rhs,
                                    double* solution,
                                    std::string* message) override;

 private:
  using LLTType = Eigen::LLT<Eigen::Ref<Eigen::MatrixXd>, Eigen::Lower>;
  std::unique_ptr<LLTType> llt_;
};

#ifndef CERES_NO_LAPACK
class CERES_NO_EXPORT LAPACKDenseCholesky final : public DenseCholesky {
 public:
  LinearSolverTerminationType Factorize(int num_cols,
                                        double* lhs,
                                        std::string* message) override;
  LinearSolverTerminationType Solve(const double* rhs,
                                    double* solution,
                                    std::string* message) override;

 private:
  double* lhs_ = nullptr;
  int num_cols_ = -1;
  LinearSolverTerminationType termination_type_ = LINEAR_SOLVER_FATAL_ERROR;
};
#endif  // CERES_NO_LAPACK

#ifndef CERES_NO_CUDA
// CUDA implementation of DenseCholesky using the cuSolverDN library using the
// 32-bit legacy interface for maximum compatibility.
class CERES_NO_EXPORT CUDADenseCholesky final : public DenseCholesky {
 public:
  static std::unique_ptr<CUDADenseCholesky> Create(
      const LinearSolver::Options& options);
  CUDADenseCholesky(const CUDADenseCholesky&) = delete;
  CUDADenseCholesky& operator=(const CUDADenseCholesky&) = delete;
  LinearSolverTerminationType Factorize(int num_cols,
                                        double* lhs,
                                        std::string* message) override;
  LinearSolverTerminationType Solve(const double* rhs,
                                    double* solution,
                                    std::string* message) override;

 private:
  CUDADenseCholesky() = default;
  // Picks up the cuSolverDN and cuStream handles from the context. If
  // the context is unable to initialize CUDA, returns false with a
  // human-readable message indicating the reason.
  bool Init(ContextImpl* context, std::string* message);

  // Handle to the cuSOLVER context.
  cusolverDnHandle_t cusolver_handle_ = nullptr;
  // CUDA device stream.
  cudaStream_t stream_ = nullptr;
  // Number of columns in the A matrix, to be cached between calls to *Factorize
  // and *Solve.
  size_t num_cols_ = 0;
  // GPU memory allocated for the A matrix (lhs matrix).
  CudaBuffer<double> lhs_;
  // GPU memory allocated for the B matrix (rhs vector).
  CudaBuffer<double> rhs_;
  // Scratch space for cuSOLVER on the GPU.
  CudaBuffer<double> device_workspace_;
  // Required for error handling with cuSOLVER.
  CudaBuffer<int> error_;
  // Cache the result of Factorize to ensure that when Solve is called, the
  // factiorization of lhs is valid.
  LinearSolverTerminationType factorize_result_ = LINEAR_SOLVER_FATAL_ERROR;
};

#endif  // CERES_NO_CUDA

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_DENSE_CHOLESKY_H_
