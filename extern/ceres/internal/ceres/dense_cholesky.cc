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

#include "ceres/dense_cholesky.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ceres/internal/config.h"

#ifndef CERES_NO_CUDA
#include "ceres/context_impl.h"
#include "cuda_runtime.h"
#include "cusolverDn.h"
#endif  // CERES_NO_CUDA

#ifndef CERES_NO_LAPACK

// C interface to the LAPACK Cholesky factorization and triangular solve.
extern "C" void dpotrf_(
    const char* uplo, const int* n, double* a, const int* lda, int* info);

extern "C" void dpotrs_(const char* uplo,
                        const int* n,
                        const int* nrhs,
                        const double* a,
                        const int* lda,
                        double* b,
                        const int* ldb,
                        int* info);
#endif

namespace ceres {
namespace internal {

DenseCholesky::~DenseCholesky() = default;

std::unique_ptr<DenseCholesky> DenseCholesky::Create(
    const LinearSolver::Options& options) {
  std::unique_ptr<DenseCholesky> dense_cholesky;

  switch (options.dense_linear_algebra_library_type) {
    case EIGEN:
      dense_cholesky = std::make_unique<EigenDenseCholesky>();
      break;

    case LAPACK:
#ifndef CERES_NO_LAPACK
      dense_cholesky = std::make_unique<LAPACKDenseCholesky>();
      break;
#else
      LOG(FATAL) << "Ceres was compiled without support for LAPACK.";
#endif

    case CUDA:
#ifndef CERES_NO_CUDA
      dense_cholesky = CUDADenseCholesky::Create(options);
      break;
#else
      LOG(FATAL) << "Ceres was compiled without support for CUDA.";
#endif

    default:
      LOG(FATAL) << "Unknown dense linear algebra library type : "
                 << DenseLinearAlgebraLibraryTypeToString(
                        options.dense_linear_algebra_library_type);
  }
  return dense_cholesky;
}

LinearSolverTerminationType DenseCholesky::FactorAndSolve(
    int num_cols,
    double* lhs,
    const double* rhs,
    double* solution,
    std::string* message) {
  LinearSolverTerminationType termination_type =
      Factorize(num_cols, lhs, message);
  if (termination_type == LINEAR_SOLVER_SUCCESS) {
    termination_type = Solve(rhs, solution, message);
  }
  return termination_type;
}

LinearSolverTerminationType EigenDenseCholesky::Factorize(
    int num_cols, double* lhs, std::string* message) {
  Eigen::Map<Eigen::MatrixXd> m(lhs, num_cols, num_cols);
  llt_ = std::make_unique<LLTType>(m);
  if (llt_->info() != Eigen::Success) {
    *message = "Eigen failure. Unable to perform dense Cholesky factorization.";
    return LINEAR_SOLVER_FAILURE;
  }

  *message = "Success.";
  return LINEAR_SOLVER_SUCCESS;
}

LinearSolverTerminationType EigenDenseCholesky::Solve(const double* rhs,
                                                      double* solution,
                                                      std::string* message) {
  if (llt_->info() != Eigen::Success) {
    *message = "Eigen failure. Unable to perform dense Cholesky factorization.";
    return LINEAR_SOLVER_FAILURE;
  }

  VectorRef(solution, llt_->cols()) =
      llt_->solve(ConstVectorRef(rhs, llt_->cols()));
  *message = "Success.";
  return LINEAR_SOLVER_SUCCESS;
}

#ifndef CERES_NO_LAPACK
LinearSolverTerminationType LAPACKDenseCholesky::Factorize(
    int num_cols, double* lhs, std::string* message) {
  lhs_ = lhs;
  num_cols_ = num_cols;

  const char uplo = 'L';
  int info = 0;
  dpotrf_(&uplo, &num_cols_, lhs_, &num_cols_, &info);

  if (info < 0) {
    termination_type_ = LINEAR_SOLVER_FATAL_ERROR;
    LOG(FATAL) << "Congratulations, you found a bug in Ceres. "
               << "Please report it. "
               << "LAPACK::dpotrf fatal error. "
               << "Argument: " << -info << " is invalid.";
  } else if (info > 0) {
    termination_type_ = LINEAR_SOLVER_FAILURE;
    *message = StringPrintf(
        "LAPACK::dpotrf numerical failure. "
        "The leading minor of order %d is not positive definite.",
        info);
  } else {
    termination_type_ = LINEAR_SOLVER_SUCCESS;
    *message = "Success.";
  }
  return termination_type_;
}

LinearSolverTerminationType LAPACKDenseCholesky::Solve(const double* rhs,
                                                       double* solution,
                                                       std::string* message) {
  const char uplo = 'L';
  const int nrhs = 1;
  int info = 0;

  std::copy_n(rhs, num_cols_, solution);
  dpotrs_(
      &uplo, &num_cols_, &nrhs, lhs_, &num_cols_, solution, &num_cols_, &info);

  if (info < 0) {
    termination_type_ = LINEAR_SOLVER_FATAL_ERROR;
    LOG(FATAL) << "Congratulations, you found a bug in Ceres. "
               << "Please report it. "
               << "LAPACK::dpotrs fatal error. "
               << "Argument: " << -info << " is invalid.";
  }

  *message = "Success";
  termination_type_ = LINEAR_SOLVER_SUCCESS;

  return termination_type_;
}

#endif  // CERES_NO_LAPACK

#ifndef CERES_NO_CUDA

bool CUDADenseCholesky::Init(ContextImpl* context, std::string* message) {
  if (!context->InitCUDA(message)) {
    return false;
  }
  cusolver_handle_ = context->cusolver_handle_;
  stream_ = context->stream_;
  error_.Reserve(1);
  *message = "CUDADenseCholesky::Init Success.";
  return true;
}

LinearSolverTerminationType CUDADenseCholesky::Factorize(int num_cols,
                                                         double* lhs,
                                                         std::string* message) {
  factorize_result_ = LinearSolverTerminationType::LINEAR_SOLVER_FATAL_ERROR;
  lhs_.Reserve(num_cols * num_cols);
  num_cols_ = num_cols;
  lhs_.CopyToGpuAsync(lhs, num_cols * num_cols, stream_);
  int device_workspace_size = 0;
  if (cusolverDnDpotrf_bufferSize(cusolver_handle_,
                                  CUBLAS_FILL_MODE_LOWER,
                                  num_cols,
                                  lhs_.data(),
                                  num_cols,
                                  &device_workspace_size) !=
      CUSOLVER_STATUS_SUCCESS) {
    *message = "cuSolverDN::cusolverDnDpotrf_bufferSize failed.";
    return LinearSolverTerminationType::LINEAR_SOLVER_FATAL_ERROR;
  }
  device_workspace_.Reserve(device_workspace_size);
  if (cusolverDnDpotrf(cusolver_handle_,
                       CUBLAS_FILL_MODE_LOWER,
                       num_cols,
                       lhs_.data(),
                       num_cols,
                       reinterpret_cast<double*>(device_workspace_.data()),
                       device_workspace_.size(),
                       error_.data()) != CUSOLVER_STATUS_SUCCESS) {
    *message = "cuSolverDN::cusolverDnDpotrf failed.";
    return LinearSolverTerminationType::LINEAR_SOLVER_FATAL_ERROR;
  }
  if (cudaDeviceSynchronize() != cudaSuccess ||
      cudaStreamSynchronize(stream_) != cudaSuccess) {
    *message = "Cuda device synchronization failed.";
    return LinearSolverTerminationType::LINEAR_SOLVER_FATAL_ERROR;
  }
  int error = 0;
  error_.CopyToHost(&error, 1);
  if (error < 0) {
    LOG(FATAL) << "Congratulations, you found a bug in Ceres - "
               << "please report it. "
               << "cuSolverDN::cusolverDnXpotrf fatal error. "
               << "Argument: " << -error << " is invalid.";
    // The following line is unreachable, but return failure just to be
    // pedantic, since the compiler does not know that.
    return LinearSolverTerminationType::LINEAR_SOLVER_FATAL_ERROR;
  } else if (error > 0) {
    *message = StringPrintf(
        "cuSolverDN::cusolverDnDpotrf numerical failure. "
        "The leading minor of order %d is not positive definite.",
        error);
    factorize_result_ = LinearSolverTerminationType::LINEAR_SOLVER_FAILURE;
    return LinearSolverTerminationType::LINEAR_SOLVER_FAILURE;
  }
  *message = "Success";
  factorize_result_ = LinearSolverTerminationType::LINEAR_SOLVER_SUCCESS;
  return LinearSolverTerminationType::LINEAR_SOLVER_SUCCESS;
}

LinearSolverTerminationType CUDADenseCholesky::Solve(const double* rhs,
                                                     double* solution,
                                                     std::string* message) {
  if (factorize_result_ != LinearSolverTerminationType::LINEAR_SOLVER_SUCCESS) {
    *message = "Factorize did not complete succesfully previously.";
    return factorize_result_;
  }
  rhs_.CopyToGpuAsync(rhs, num_cols_, stream_);
  if (cusolverDnDpotrs(cusolver_handle_,
                       CUBLAS_FILL_MODE_LOWER,
                       num_cols_,
                       1,
                       lhs_.data(),
                       num_cols_,
                       rhs_.data(),
                       num_cols_,
                       error_.data()) != CUSOLVER_STATUS_SUCCESS) {
    *message = "cuSolverDN::cusolverDnDpotrs failed.";
    return LinearSolverTerminationType::LINEAR_SOLVER_FATAL_ERROR;
  }
  if (cudaDeviceSynchronize() != cudaSuccess ||
      cudaStreamSynchronize(stream_) != cudaSuccess) {
    *message = "Cuda device synchronization failed.";
    return LinearSolverTerminationType::LINEAR_SOLVER_FATAL_ERROR;
  }
  int error = 0;
  error_.CopyToHost(&error, 1);
  if (error != 0) {
    LOG(FATAL) << "Congratulations, you found a bug in Ceres. "
               << "Please report it."
               << "cuSolverDN::cusolverDnDpotrs fatal error. "
               << "Argument: " << -error << " is invalid.";
  }
  rhs_.CopyToHost(solution, num_cols_);
  *message = "Success";
  return LinearSolverTerminationType::LINEAR_SOLVER_SUCCESS;
}

std::unique_ptr<CUDADenseCholesky> CUDADenseCholesky::Create(
    const LinearSolver::Options& options) {
  if (options.dense_linear_algebra_library_type != CUDA) {
    // The user called the wrong factory method.
    return nullptr;
  }
  auto cuda_dense_cholesky =
      std::unique_ptr<CUDADenseCholesky>(new CUDADenseCholesky());
  std::string cuda_error;
  if (cuda_dense_cholesky->Init(options.context, &cuda_error)) {
    return cuda_dense_cholesky;
  }
  // Initialization failed, destroy the object (done automatically) and return a
  // nullptr.
  LOG(ERROR) << "CUDADenseCholesky::Init failed: " << cuda_error;
  return nullptr;
}

#endif  // CERES_NO_CUDA

}  // namespace internal
}  // namespace ceres
