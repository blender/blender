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

#include "ceres/dense_cholesky.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ceres/internal/config.h"
#include "ceres/iterative_refiner.h"

#ifndef CERES_NO_CUDA
#include "ceres/context_impl.h"
#include "ceres/cuda_kernels_vector_ops.h"
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

extern "C" void spotrf_(
    const char* uplo, const int* n, float* a, const int* lda, int* info);

extern "C" void spotrs_(const char* uplo,
                        const int* n,
                        const int* nrhs,
                        const float* a,
                        const int* lda,
                        float* b,
                        const int* ldb,
                        int* info);
#endif

namespace ceres::internal {

DenseCholesky::~DenseCholesky() = default;

std::unique_ptr<DenseCholesky> DenseCholesky::Create(
    const LinearSolver::Options& options) {
  std::unique_ptr<DenseCholesky> dense_cholesky;

  switch (options.dense_linear_algebra_library_type) {
    case EIGEN:
      // Eigen mixed precision solver not yet implemented.
      if (options.use_mixed_precision_solves) {
        dense_cholesky = std::make_unique<FloatEigenDenseCholesky>();
      } else {
        dense_cholesky = std::make_unique<EigenDenseCholesky>();
      }
      break;

    case LAPACK:
#ifndef CERES_NO_LAPACK
      // LAPACK mixed precision solver not yet implemented.
      if (options.use_mixed_precision_solves) {
        dense_cholesky = std::make_unique<FloatLAPACKDenseCholesky>();
      } else {
        dense_cholesky = std::make_unique<LAPACKDenseCholesky>();
      }
      break;
#else
      LOG(FATAL) << "Ceres was compiled without support for LAPACK.";
#endif

    case CUDA:
#ifndef CERES_NO_CUDA
      if (options.use_mixed_precision_solves) {
        dense_cholesky = CUDADenseCholeskyMixedPrecision::Create(options);
      } else {
        dense_cholesky = CUDADenseCholesky::Create(options);
      }
      break;
#else
      LOG(FATAL) << "Ceres was compiled without support for CUDA.";
#endif

    default:
      LOG(FATAL) << "Unknown dense linear algebra library type : "
                 << DenseLinearAlgebraLibraryTypeToString(
                        options.dense_linear_algebra_library_type);
  }

  if (options.max_num_refinement_iterations > 0) {
    auto refiner = std::make_unique<DenseIterativeRefiner>(
        options.max_num_refinement_iterations);
    dense_cholesky = std::make_unique<RefinedDenseCholesky>(
        std::move(dense_cholesky), std::move(refiner));
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
  if (termination_type == LinearSolverTerminationType::SUCCESS) {
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
    return LinearSolverTerminationType::FAILURE;
  }

  *message = "Success.";
  return LinearSolverTerminationType::SUCCESS;
}

LinearSolverTerminationType EigenDenseCholesky::Solve(const double* rhs,
                                                      double* solution,
                                                      std::string* message) {
  if (llt_->info() != Eigen::Success) {
    *message = "Eigen failure. Unable to perform dense Cholesky factorization.";
    return LinearSolverTerminationType::FAILURE;
  }

  VectorRef(solution, llt_->cols()) =
      llt_->solve(ConstVectorRef(rhs, llt_->cols()));
  *message = "Success.";
  return LinearSolverTerminationType::SUCCESS;
}

LinearSolverTerminationType FloatEigenDenseCholesky::Factorize(
    int num_cols, double* lhs, std::string* message) {
  // TODO(sameeragarwal): Check if this causes a double allocation.
  lhs_ = Eigen::Map<Eigen::MatrixXd>(lhs, num_cols, num_cols).cast<float>();
  llt_ = std::make_unique<LLTType>(lhs_);
  if (llt_->info() != Eigen::Success) {
    *message = "Eigen failure. Unable to perform dense Cholesky factorization.";
    return LinearSolverTerminationType::FAILURE;
  }

  *message = "Success.";
  return LinearSolverTerminationType::SUCCESS;
}

LinearSolverTerminationType FloatEigenDenseCholesky::Solve(
    const double* rhs, double* solution, std::string* message) {
  if (llt_->info() != Eigen::Success) {
    *message = "Eigen failure. Unable to perform dense Cholesky factorization.";
    return LinearSolverTerminationType::FAILURE;
  }

  rhs_ = ConstVectorRef(rhs, llt_->cols()).cast<float>();
  solution_ = llt_->solve(rhs_);
  VectorRef(solution, llt_->cols()) = solution_.cast<double>();
  *message = "Success.";
  return LinearSolverTerminationType::SUCCESS;
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
    termination_type_ = LinearSolverTerminationType::FATAL_ERROR;
    LOG(FATAL) << "Congratulations, you found a bug in Ceres. "
               << "Please report it. "
               << "LAPACK::dpotrf fatal error. "
               << "Argument: " << -info << " is invalid.";
  } else if (info > 0) {
    termination_type_ = LinearSolverTerminationType::FAILURE;
    *message = StringPrintf(
        "LAPACK::dpotrf numerical failure. "
        "The leading minor of order %d is not positive definite.",
        info);
  } else {
    termination_type_ = LinearSolverTerminationType::SUCCESS;
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

  VectorRef(solution, num_cols_) = ConstVectorRef(rhs, num_cols_);
  dpotrs_(
      &uplo, &num_cols_, &nrhs, lhs_, &num_cols_, solution, &num_cols_, &info);

  if (info < 0) {
    termination_type_ = LinearSolverTerminationType::FATAL_ERROR;
    LOG(FATAL) << "Congratulations, you found a bug in Ceres. "
               << "Please report it. "
               << "LAPACK::dpotrs fatal error. "
               << "Argument: " << -info << " is invalid.";
  }

  *message = "Success";
  termination_type_ = LinearSolverTerminationType::SUCCESS;

  return termination_type_;
}

LinearSolverTerminationType FloatLAPACKDenseCholesky::Factorize(
    int num_cols, double* lhs, std::string* message) {
  num_cols_ = num_cols;
  lhs_ = Eigen::Map<Eigen::MatrixXd>(lhs, num_cols, num_cols).cast<float>();

  const char uplo = 'L';
  int info = 0;
  spotrf_(&uplo, &num_cols_, lhs_.data(), &num_cols_, &info);

  if (info < 0) {
    termination_type_ = LinearSolverTerminationType::FATAL_ERROR;
    LOG(FATAL) << "Congratulations, you found a bug in Ceres. "
               << "Please report it. "
               << "LAPACK::spotrf fatal error. "
               << "Argument: " << -info << " is invalid.";
  } else if (info > 0) {
    termination_type_ = LinearSolverTerminationType::FAILURE;
    *message = StringPrintf(
        "LAPACK::spotrf numerical failure. "
        "The leading minor of order %d is not positive definite.",
        info);
  } else {
    termination_type_ = LinearSolverTerminationType::SUCCESS;
    *message = "Success.";
  }
  return termination_type_;
}

LinearSolverTerminationType FloatLAPACKDenseCholesky::Solve(
    const double* rhs, double* solution, std::string* message) {
  const char uplo = 'L';
  const int nrhs = 1;
  int info = 0;
  rhs_and_solution_ = ConstVectorRef(rhs, num_cols_).cast<float>();
  spotrs_(&uplo,
          &num_cols_,
          &nrhs,
          lhs_.data(),
          &num_cols_,
          rhs_and_solution_.data(),
          &num_cols_,
          &info);

  if (info < 0) {
    termination_type_ = LinearSolverTerminationType::FATAL_ERROR;
    LOG(FATAL) << "Congratulations, you found a bug in Ceres. "
               << "Please report it. "
               << "LAPACK::dpotrs fatal error. "
               << "Argument: " << -info << " is invalid.";
  }

  *message = "Success";
  termination_type_ = LinearSolverTerminationType::SUCCESS;
  VectorRef(solution, num_cols_) =
      rhs_and_solution_.head(num_cols_).cast<double>();
  return termination_type_;
}

#endif  // CERES_NO_LAPACK

RefinedDenseCholesky::RefinedDenseCholesky(
    std::unique_ptr<DenseCholesky> dense_cholesky,
    std::unique_ptr<DenseIterativeRefiner> iterative_refiner)
    : dense_cholesky_(std::move(dense_cholesky)),
      iterative_refiner_(std::move(iterative_refiner)) {}

RefinedDenseCholesky::~RefinedDenseCholesky() = default;

LinearSolverTerminationType RefinedDenseCholesky::Factorize(
    const int num_cols, double* lhs, std::string* message) {
  lhs_ = lhs;
  num_cols_ = num_cols;
  return dense_cholesky_->Factorize(num_cols, lhs, message);
}

LinearSolverTerminationType RefinedDenseCholesky::Solve(const double* rhs,
                                                        double* solution,
                                                        std::string* message) {
  CHECK(lhs_ != nullptr);
  auto termination_type = dense_cholesky_->Solve(rhs, solution, message);
  if (termination_type != LinearSolverTerminationType::SUCCESS) {
    return termination_type;
  }

  iterative_refiner_->Refine(
      num_cols_, lhs_, rhs, dense_cholesky_.get(), solution);
  return LinearSolverTerminationType::SUCCESS;
}

#ifndef CERES_NO_CUDA

CUDADenseCholesky::CUDADenseCholesky(ContextImpl* context)
    : context_(context),
      lhs_{context},
      rhs_{context},
      device_workspace_{context},
      error_(context, 1) {}

LinearSolverTerminationType CUDADenseCholesky::Factorize(int num_cols,
                                                         double* lhs,
                                                         std::string* message) {
  factorize_result_ = LinearSolverTerminationType::FATAL_ERROR;
  lhs_.Reserve(num_cols * num_cols);
  num_cols_ = num_cols;
  lhs_.CopyFromCpu(lhs, num_cols * num_cols);
  int device_workspace_size = 0;
  if (cusolverDnDpotrf_bufferSize(context_->cusolver_handle_,
                                  CUBLAS_FILL_MODE_LOWER,
                                  num_cols,
                                  lhs_.data(),
                                  num_cols,
                                  &device_workspace_size) !=
      CUSOLVER_STATUS_SUCCESS) {
    *message = "cuSolverDN::cusolverDnDpotrf_bufferSize failed.";
    return LinearSolverTerminationType::FATAL_ERROR;
  }
  device_workspace_.Reserve(device_workspace_size);
  if (cusolverDnDpotrf(context_->cusolver_handle_,
                       CUBLAS_FILL_MODE_LOWER,
                       num_cols,
                       lhs_.data(),
                       num_cols,
                       reinterpret_cast<double*>(device_workspace_.data()),
                       device_workspace_.size(),
                       error_.data()) != CUSOLVER_STATUS_SUCCESS) {
    *message = "cuSolverDN::cusolverDnDpotrf failed.";
    return LinearSolverTerminationType::FATAL_ERROR;
  }
  int error = 0;
  error_.CopyToCpu(&error, 1);
  if (error < 0) {
    LOG(FATAL) << "Congratulations, you found a bug in Ceres - "
               << "please report it. "
               << "cuSolverDN::cusolverDnXpotrf fatal error. "
               << "Argument: " << -error << " is invalid.";
    // The following line is unreachable, but return failure just to be
    // pedantic, since the compiler does not know that.
    return LinearSolverTerminationType::FATAL_ERROR;
  } else if (error > 0) {
    *message = StringPrintf(
        "cuSolverDN::cusolverDnDpotrf numerical failure. "
        "The leading minor of order %d is not positive definite.",
        error);
    factorize_result_ = LinearSolverTerminationType::FAILURE;
    return LinearSolverTerminationType::FAILURE;
  }
  *message = "Success";
  factorize_result_ = LinearSolverTerminationType::SUCCESS;
  return LinearSolverTerminationType::SUCCESS;
}

LinearSolverTerminationType CUDADenseCholesky::Solve(const double* rhs,
                                                     double* solution,
                                                     std::string* message) {
  if (factorize_result_ != LinearSolverTerminationType::SUCCESS) {
    *message = "Factorize did not complete successfully previously.";
    return factorize_result_;
  }
  rhs_.CopyFromCpu(rhs, num_cols_);
  if (cusolverDnDpotrs(context_->cusolver_handle_,
                       CUBLAS_FILL_MODE_LOWER,
                       num_cols_,
                       1,
                       lhs_.data(),
                       num_cols_,
                       rhs_.data(),
                       num_cols_,
                       error_.data()) != CUSOLVER_STATUS_SUCCESS) {
    *message = "cuSolverDN::cusolverDnDpotrs failed.";
    return LinearSolverTerminationType::FATAL_ERROR;
  }
  int error = 0;
  error_.CopyToCpu(&error, 1);
  if (error != 0) {
    LOG(FATAL) << "Congratulations, you found a bug in Ceres. "
               << "Please report it."
               << "cuSolverDN::cusolverDnDpotrs fatal error. "
               << "Argument: " << -error << " is invalid.";
  }
  rhs_.CopyToCpu(solution, num_cols_);
  *message = "Success";
  return LinearSolverTerminationType::SUCCESS;
}

std::unique_ptr<CUDADenseCholesky> CUDADenseCholesky::Create(
    const LinearSolver::Options& options) {
  if (options.dense_linear_algebra_library_type != CUDA ||
      options.context == nullptr || !options.context->IsCudaInitialized()) {
    return nullptr;
  }
  return std::unique_ptr<CUDADenseCholesky>(
      new CUDADenseCholesky(options.context));
}

std::unique_ptr<CUDADenseCholeskyMixedPrecision>
CUDADenseCholeskyMixedPrecision::Create(const LinearSolver::Options& options) {
  if (options.dense_linear_algebra_library_type != CUDA ||
      !options.use_mixed_precision_solves || options.context == nullptr ||
      !options.context->IsCudaInitialized()) {
    return nullptr;
  }
  return std::unique_ptr<CUDADenseCholeskyMixedPrecision>(
      new CUDADenseCholeskyMixedPrecision(
          options.context, options.max_num_refinement_iterations));
}

LinearSolverTerminationType
CUDADenseCholeskyMixedPrecision::CudaCholeskyFactorize(std::string* message) {
  int device_workspace_size = 0;
  if (cusolverDnSpotrf_bufferSize(context_->cusolver_handle_,
                                  CUBLAS_FILL_MODE_LOWER,
                                  num_cols_,
                                  lhs_fp32_.data(),
                                  num_cols_,
                                  &device_workspace_size) !=
      CUSOLVER_STATUS_SUCCESS) {
    *message = "cuSolverDN::cusolverDnSpotrf_bufferSize failed.";
    return LinearSolverTerminationType::FATAL_ERROR;
  }
  device_workspace_.Reserve(device_workspace_size);
  if (cusolverDnSpotrf(context_->cusolver_handle_,
                       CUBLAS_FILL_MODE_LOWER,
                       num_cols_,
                       lhs_fp32_.data(),
                       num_cols_,
                       device_workspace_.data(),
                       device_workspace_.size(),
                       error_.data()) != CUSOLVER_STATUS_SUCCESS) {
    *message = "cuSolverDN::cusolverDnSpotrf failed.";
    return LinearSolverTerminationType::FATAL_ERROR;
  }
  int error = 0;
  error_.CopyToCpu(&error, 1);
  if (error < 0) {
    LOG(FATAL) << "Congratulations, you found a bug in Ceres - "
               << "please report it. "
               << "cuSolverDN::cusolverDnSpotrf fatal error. "
               << "Argument: " << -error << " is invalid.";
    // The following line is unreachable, but return failure just to be
    // pedantic, since the compiler does not know that.
    return LinearSolverTerminationType::FATAL_ERROR;
  }
  if (error > 0) {
    *message = StringPrintf(
        "cuSolverDN::cusolverDnSpotrf numerical failure. "
        "The leading minor of order %d is not positive definite.",
        error);
    factorize_result_ = LinearSolverTerminationType::FAILURE;
    return LinearSolverTerminationType::FAILURE;
  }
  *message = "Success";
  return LinearSolverTerminationType::SUCCESS;
}

LinearSolverTerminationType CUDADenseCholeskyMixedPrecision::CudaCholeskySolve(
    std::string* message) {
  CHECK_EQ(cudaMemcpyAsync(correction_fp32_.data(),
                           residual_fp32_.data(),
                           num_cols_ * sizeof(float),
                           cudaMemcpyDeviceToDevice,
                           context_->DefaultStream()),
           cudaSuccess);
  if (cusolverDnSpotrs(context_->cusolver_handle_,
                       CUBLAS_FILL_MODE_LOWER,
                       num_cols_,
                       1,
                       lhs_fp32_.data(),
                       num_cols_,
                       correction_fp32_.data(),
                       num_cols_,
                       error_.data()) != CUSOLVER_STATUS_SUCCESS) {
    *message = "cuSolverDN::cusolverDnDpotrs failed.";
    return LinearSolverTerminationType::FATAL_ERROR;
  }
  int error = 0;
  error_.CopyToCpu(&error, 1);
  if (error != 0) {
    LOG(FATAL) << "Congratulations, you found a bug in Ceres. "
               << "Please report it."
               << "cuSolverDN::cusolverDnDpotrs fatal error. "
               << "Argument: " << -error << " is invalid.";
  }
  *message = "Success";
  return LinearSolverTerminationType::SUCCESS;
}

CUDADenseCholeskyMixedPrecision::CUDADenseCholeskyMixedPrecision(
    ContextImpl* context, int max_num_refinement_iterations)
    : context_(context),
      lhs_fp64_{context},
      rhs_fp64_{context},
      lhs_fp32_{context},
      device_workspace_{context},
      error_(context, 1),
      x_fp64_{context},
      correction_fp32_{context},
      residual_fp32_{context},
      residual_fp64_{context},
      max_num_refinement_iterations_(max_num_refinement_iterations) {}

LinearSolverTerminationType CUDADenseCholeskyMixedPrecision::Factorize(
    int num_cols, double* lhs, std::string* message) {
  num_cols_ = num_cols;

  // Copy fp64 version of lhs to GPU.
  lhs_fp64_.Reserve(num_cols * num_cols);
  lhs_fp64_.CopyFromCpu(lhs, num_cols * num_cols);

  // Create an fp32 copy of lhs, lhs_fp32.
  lhs_fp32_.Reserve(num_cols * num_cols);
  CudaFP64ToFP32(lhs_fp64_.data(),
                 lhs_fp32_.data(),
                 num_cols * num_cols,
                 context_->DefaultStream());

  // Factorize lhs_fp32.
  factorize_result_ = CudaCholeskyFactorize(message);
  return factorize_result_;
}

LinearSolverTerminationType CUDADenseCholeskyMixedPrecision::Solve(
    const double* rhs, double* solution, std::string* message) {
  // If factorization failed, return failure.
  if (factorize_result_ != LinearSolverTerminationType::SUCCESS) {
    *message = "Factorize did not complete successfully previously.";
    return factorize_result_;
  }

  // Reserve memory for all arrays.
  rhs_fp64_.Reserve(num_cols_);
  x_fp64_.Reserve(num_cols_);
  correction_fp32_.Reserve(num_cols_);
  residual_fp32_.Reserve(num_cols_);
  residual_fp64_.Reserve(num_cols_);

  // Initialize x = 0.
  CudaSetZeroFP64(x_fp64_.data(), num_cols_, context_->DefaultStream());

  // Initialize residual = rhs.
  rhs_fp64_.CopyFromCpu(rhs, num_cols_);
  residual_fp64_.CopyFromGPUArray(rhs_fp64_.data(), num_cols_);

  for (int i = 0; i <= max_num_refinement_iterations_; ++i) {
    // Cast residual from fp64 to fp32.
    CudaFP64ToFP32(residual_fp64_.data(),
                   residual_fp32_.data(),
                   num_cols_,
                   context_->DefaultStream());
    // [fp32] c = lhs^-1 * residual.
    auto result = CudaCholeskySolve(message);
    if (result != LinearSolverTerminationType::SUCCESS) {
      return result;
    }
    // [fp64] x += c.
    CudaDsxpy(x_fp64_.data(),
              correction_fp32_.data(),
              num_cols_,
              context_->DefaultStream());
    if (i < max_num_refinement_iterations_) {
      // [fp64] residual = rhs - lhs * x
      // This is done in two steps:
      // 1. [fp64] residual = rhs
      residual_fp64_.CopyFromGPUArray(rhs_fp64_.data(), num_cols_);
      // 2. [fp64] residual = residual - lhs * x
      double alpha = -1.0;
      double beta = 1.0;
      cublasDsymv(context_->cublas_handle_,
                  CUBLAS_FILL_MODE_LOWER,
                  num_cols_,
                  &alpha,
                  lhs_fp64_.data(),
                  num_cols_,
                  x_fp64_.data(),
                  1,
                  &beta,
                  residual_fp64_.data(),
                  1);
    }
  }
  x_fp64_.CopyToCpu(solution, num_cols_);
  *message = "Success.";
  return LinearSolverTerminationType::SUCCESS;
}

#endif  // CERES_NO_CUDA

}  // namespace ceres::internal
