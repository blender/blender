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

#ifndef CERES_INTERNAL_DENSE_QR_H_
#define CERES_INTERNAL_DENSE_QR_H_

// This include must come before any #ifndef check on Ceres compile options.
// clang-format off
#include "ceres/internal/config.h"
// clang-format on

#include <memory>
#include <vector>

#include "Eigen/Dense"
#include "ceres/context_impl.h"
#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/export.h"
#include "ceres/linear_solver.h"
#include "glog/logging.h"

#ifndef CERES_NO_CUDA
#include "ceres/context_impl.h"
#include "ceres/cuda_buffer.h"
#include "cublas_v2.h"
#include "cuda_runtime.h"
#include "cusolverDn.h"
#endif  // CERES_NO_CUDA

namespace ceres::internal {

// An interface that abstracts away the internal details of various dense linear
// algebra libraries and offers a simple API for solving dense linear systems
// using a QR factorization.
class CERES_NO_EXPORT DenseQR {
 public:
  static std::unique_ptr<DenseQR> Create(const LinearSolver::Options& options);

  virtual ~DenseQR();

  // Computes the QR factorization of the given matrix.
  //
  // The input matrix lhs is assumed to be a column-major num_rows x num_cols
  // matrix.
  //
  // The input matrix lhs may be modified by the implementation to store the
  // factorization, irrespective of whether the factorization succeeds or not.
  // As a result it is the user's responsibility to ensure that lhs is valid
  // when Solve is called.
  virtual LinearSolverTerminationType Factorize(int num_rows,
                                                int num_cols,
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
  LinearSolverTerminationType FactorAndSolve(int num_rows,
                                             int num_cols,
                                             double* lhs,
                                             const double* rhs,
                                             double* solution,
                                             std::string* message);
};

class CERES_NO_EXPORT EigenDenseQR final : public DenseQR {
 public:
  LinearSolverTerminationType Factorize(int num_rows,
                                        int num_cols,
                                        double* lhs,
                                        std::string* message) override;
  LinearSolverTerminationType Solve(const double* rhs,
                                    double* solution,
                                    std::string* message) override;

 private:
  using QRType = Eigen::HouseholderQR<Eigen::Ref<ColMajorMatrix>>;
  std::unique_ptr<QRType> qr_;
};

#ifndef CERES_NO_LAPACK
class CERES_NO_EXPORT LAPACKDenseQR final : public DenseQR {
 public:
  LinearSolverTerminationType Factorize(int num_rows,
                                        int num_cols,
                                        double* lhs,
                                        std::string* message) override;
  LinearSolverTerminationType Solve(const double* rhs,
                                    double* solution,
                                    std::string* message) override;

 private:
  double* lhs_ = nullptr;
  int num_rows_;
  int num_cols_;
  LinearSolverTerminationType termination_type_ =
      LinearSolverTerminationType::FATAL_ERROR;
  Vector work_;
  Vector tau_;
  Vector q_transpose_rhs_;
};
#endif  // CERES_NO_LAPACK

#ifndef CERES_NO_CUDA
// Implementation of DenseQR using the 32-bit cuSolverDn interface. A
// requirement for using this solver is that the lhs must not be rank deficient.
// This is because cuSolverDn does not implement the singularity-checking
// wrapper trtrs, hence this solver directly uses trsv from CUBLAS for the
// backsubstitution.
class CERES_NO_EXPORT CUDADenseQR final : public DenseQR {
 public:
  static std::unique_ptr<CUDADenseQR> Create(
      const LinearSolver::Options& options);
  CUDADenseQR(const CUDADenseQR&) = delete;
  CUDADenseQR& operator=(const CUDADenseQR&) = delete;
  LinearSolverTerminationType Factorize(int num_rows,
                                        int num_cols,
                                        double* lhs,
                                        std::string* message) override;
  LinearSolverTerminationType Solve(const double* rhs,
                                    double* solution,
                                    std::string* message) override;

 private:
  explicit CUDADenseQR(ContextImpl* context);

  ContextImpl* context_ = nullptr;
  // Number of rowns in the A matrix, to be cached between calls to *Factorize
  // and *Solve.
  size_t num_rows_ = 0;
  // Number of columns in the A matrix, to be cached between calls to *Factorize
  // and *Solve.
  size_t num_cols_ = 0;
  // GPU memory allocated for the A matrix (lhs matrix).
  CudaBuffer<double> lhs_;
  // GPU memory allocated for the B matrix (rhs vector).
  CudaBuffer<double> rhs_;
  // GPU memory allocated for the TAU matrix (scaling of householder vectors).
  CudaBuffer<double> tau_;
  // Scratch space for cuSOLVER on the GPU.
  CudaBuffer<double> device_workspace_;
  // Required for error handling with cuSOLVER.
  CudaBuffer<int> error_;
  // Cache the result of Factorize to ensure that when Solve is called, the
  // factiorization of lhs is valid.
  LinearSolverTerminationType factorize_result_ =
      LinearSolverTerminationType::FATAL_ERROR;
};

#endif  // CERES_NO_CUDA

}  // namespace ceres::internal

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_INTERNAL_DENSE_QR_H_
