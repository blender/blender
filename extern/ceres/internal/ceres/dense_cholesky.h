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
#include "ceres/context_impl.h"
#include "ceres/cuda_buffer.h"
#include "ceres/linear_solver.h"
#include "glog/logging.h"
#ifndef CERES_NO_CUDA
#include "ceres/context_impl.h"
#include "cuda_runtime.h"
#include "cusolverDn.h"
#endif  // CERES_NO_CUDA

namespace ceres::internal {

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
  // only called if Factorize returns LinearSolverTerminationType::SUCCESS.
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

class CERES_NO_EXPORT FloatEigenDenseCholesky final : public DenseCholesky {
 public:
  LinearSolverTerminationType Factorize(int num_cols,
                                        double* lhs,
                                        std::string* message) override;
  LinearSolverTerminationType Solve(const double* rhs,
                                    double* solution,
                                    std::string* message) override;

 private:
  Eigen::MatrixXf lhs_;
  Eigen::VectorXf rhs_;
  Eigen::VectorXf solution_;
  using LLTType = Eigen::LLT<Eigen::MatrixXf, Eigen::Lower>;
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
  LinearSolverTerminationType termination_type_ =
      LinearSolverTerminationType::FATAL_ERROR;
};

class CERES_NO_EXPORT FloatLAPACKDenseCholesky final : public DenseCholesky {
 public:
  LinearSolverTerminationType Factorize(int num_cols,
                                        double* lhs,
                                        std::string* message) override;
  LinearSolverTerminationType Solve(const double* rhs,
                                    double* solution,
                                    std::string* message) override;

 private:
  Eigen::MatrixXf lhs_;
  Eigen::VectorXf rhs_and_solution_;
  int num_cols_ = -1;
  LinearSolverTerminationType termination_type_ =
      LinearSolverTerminationType::FATAL_ERROR;
};
#endif  // CERES_NO_LAPACK

class DenseIterativeRefiner;

// Computes an initial solution using the given instance of
// DenseCholesky, and then refines it using the DenseIterativeRefiner.
class CERES_NO_EXPORT RefinedDenseCholesky final : public DenseCholesky {
 public:
  RefinedDenseCholesky(
      std::unique_ptr<DenseCholesky> dense_cholesky,
      std::unique_ptr<DenseIterativeRefiner> iterative_refiner);
  ~RefinedDenseCholesky() override;

  LinearSolverTerminationType Factorize(int num_cols,
                                        double* lhs,
                                        std::string* message) override;
  LinearSolverTerminationType Solve(const double* rhs,
                                    double* solution,
                                    std::string* message) override;

 private:
  std::unique_ptr<DenseCholesky> dense_cholesky_;
  std::unique_ptr<DenseIterativeRefiner> iterative_refiner_;
  double* lhs_ = nullptr;
  int num_cols_;
};

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
  explicit CUDADenseCholesky(ContextImpl* context);

  ContextImpl* context_ = nullptr;
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
  // factorization of lhs is valid.
  LinearSolverTerminationType factorize_result_ =
      LinearSolverTerminationType::FATAL_ERROR;
};

// A mixed-precision iterative refinement dense Cholesky solver using FP32 CUDA
// Dense Cholesky for inner iterations, and FP64 outer refinements.
// This class implements a modified version of the  "Classical iterative
// refinement" (Algorithm 4.1) from the following paper:
// Haidar, Azzam, Harun Bayraktar, Stanimire Tomov, Jack Dongarra, and Nicholas
// J. Higham. "Mixed-precision iterative refinement using tensor cores on GPUs
// to accelerate solution of linear systems." Proceedings of the Royal Society A
// 476, no. 2243 (2020): 20200110.
//
// The three key modifications from Algorithm 4.1 in the paper are:
// 1. We use Cholesky factorization instead of LU factorization since our A is
//    symmetric positive definite.
// 2. During the solution update, the up-cast and accumulation is performed in
//    one step with a custom kernel.
class CERES_NO_EXPORT CUDADenseCholeskyMixedPrecision final
    : public DenseCholesky {
 public:
  static std::unique_ptr<CUDADenseCholeskyMixedPrecision> Create(
      const LinearSolver::Options& options);
  CUDADenseCholeskyMixedPrecision(const CUDADenseCholeskyMixedPrecision&) =
      delete;
  CUDADenseCholeskyMixedPrecision& operator=(
      const CUDADenseCholeskyMixedPrecision&) = delete;
  LinearSolverTerminationType Factorize(int num_cols,
                                        double* lhs,
                                        std::string* message) override;
  LinearSolverTerminationType Solve(const double* rhs,
                                    double* solution,
                                    std::string* message) override;

 private:
  CUDADenseCholeskyMixedPrecision(ContextImpl* context,
                                  int max_num_refinement_iterations);

  // Helper function to wrap Cuda boilerplate needed to call Spotrf.
  LinearSolverTerminationType CudaCholeskyFactorize(std::string* message);
  // Helper function to wrap Cuda boilerplate needed to call Spotrs.
  LinearSolverTerminationType CudaCholeskySolve(std::string* message);
  // Picks up the cuSolverDN and cuStream handles from the context in the
  // options, and the number of refinement iterations from the options. If
  // the context is unable to initialize CUDA, returns false with a
  // human-readable message indicating the reason.
  bool Init(const LinearSolver::Options& options, std::string* message);

  ContextImpl* context_ = nullptr;
  // Number of columns in the A matrix, to be cached between calls to *Factorize
  // and *Solve.
  size_t num_cols_ = 0;
  CudaBuffer<double> lhs_fp64_;
  CudaBuffer<double> rhs_fp64_;
  CudaBuffer<float> lhs_fp32_;
  // Scratch space for cuSOLVER on the GPU.
  CudaBuffer<float> device_workspace_;
  // Required for error handling with cuSOLVER.
  CudaBuffer<int> error_;

  // Solution to lhs * x = rhs.
  CudaBuffer<double> x_fp64_;
  // Incremental correction to x.
  CudaBuffer<float> correction_fp32_;
  // Residual to iterative refinement.
  CudaBuffer<float> residual_fp32_;
  CudaBuffer<double> residual_fp64_;

  // Number of inner refinement iterations to perform.
  int max_num_refinement_iterations_ = 0;
  // Cache the result of Factorize to ensure that when Solve is called, the
  // factorization of lhs is valid.
  LinearSolverTerminationType factorize_result_ =
      LinearSolverTerminationType::FATAL_ERROR;
};

#endif  // CERES_NO_CUDA

}  // namespace ceres::internal

#endif  // CERES_INTERNAL_DENSE_CHOLESKY_H_
