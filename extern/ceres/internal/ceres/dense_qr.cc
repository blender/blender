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

#include "ceres/dense_qr.h"

#include <algorithm>
#include <memory>
#include <string>

#ifndef CERES_NO_CUDA
#include "ceres/context_impl.h"
#include "cublas_v2.h"
#include "cusolverDn.h"
#endif  // CERES_NO_CUDA

#ifndef CERES_NO_LAPACK

// LAPACK routines for solving a linear least squares problem using QR
// factorization. This is done in three stages:
//
// A * x     = b
// Q * R * x = b               (dgeqrf)
//     R * x = Q' * b          (dormqr)
//         x = R^{-1} * Q'* b  (dtrtrs)

// clang-format off

// Compute the QR factorization of a.
//
// a is an m x n column major matrix (Denoted by "A" in the above description)
// lda is the leading dimension of a. lda >= max(1, num_rows)
// tau is an array of size min(m,n). It contains the scalar factors of the
// elementary reflectors.
// work is an array of size max(1,lwork). On exit, if info=0, work[0] contains
// the optimal size of work.
//
// if lwork >= 1 it is the size of work. If lwork = -1, then a workspace query is assumed.
// dgeqrf computes the optimal size of the work array and returns it as work[0].
//
// info = 0, successful exit.
// info < 0, if info = -i, then the i^th argument had illegal value.
extern "C" void dgeqrf_(const int* m, const int* n, double* a, const int* lda,
                        double* tau, double* work, const int* lwork, int* info);

// Apply Q or Q' to b.
//
// b is a m times n column major matrix.
// size = 'L' applies Q or Q' on the left, size = 'R' applies Q or Q' on the right.
// trans = 'N', applies Q, trans = 'T', applies Q'.
// k is the number of elementary reflectors whose product defines the matrix Q.
// If size = 'L', m >= k >= 0 and if side = 'R', n >= k >= 0.
// a is an lda x k column major matrix containing the reflectors as returned by dgeqrf.
// ldb is the leading dimension of b.
// work is an array of size max(1, lwork)
// lwork if positive is the size of work. If lwork = -1, then a
// workspace query is assumed.
//
// info = 0, successful exit.
// info < 0, if info = -i, then the i^th argument had illegal value.
extern "C" void dormqr_(const char* side, const char* trans, const int* m,
                        const int* n ,const int* k, double* a, const int* lda,
                        double* tau, double* b, const int* ldb, double* work,
                        const int* lwork, int* info);

// Solve a triangular system of the form A * x = b
//
// uplo = 'U', A is upper triangular. uplo = 'L' is lower triangular.
// trans = 'N', 'T', 'C' specifies the form  - A, A^T, A^H.
// DIAG = 'N', A is not unit triangular. 'U' is unit triangular.
// n is the order of the matrix A.
// nrhs number of columns of b.
// a is a column major lda x n.
// b is a column major matrix of ldb x nrhs
//
// info = 0 successful.
//      = -i < 0 i^th argument is an illegal value.
//      = i > 0, i^th diagonal element of A is zero.
extern "C" void dtrtrs_(const char* uplo, const char* trans, const char* diag,
                        const int* n, const int* nrhs, double* a, const int* lda,
                        double* b, const int* ldb, int* info);
// clang-format on

#endif

namespace ceres::internal {

DenseQR::~DenseQR() = default;

std::unique_ptr<DenseQR> DenseQR::Create(const LinearSolver::Options& options) {
  std::unique_ptr<DenseQR> dense_qr;

  switch (options.dense_linear_algebra_library_type) {
    case EIGEN:
      dense_qr = std::make_unique<EigenDenseQR>();
      break;

    case LAPACK:
#ifndef CERES_NO_LAPACK
      dense_qr = std::make_unique<LAPACKDenseQR>();
      break;
#else
      LOG(FATAL) << "Ceres was compiled without support for LAPACK.";
#endif

    case CUDA:
#ifndef CERES_NO_CUDA
      dense_qr = CUDADenseQR::Create(options);
      break;
#else
      LOG(FATAL) << "Ceres was compiled without support for CUDA.";
#endif

    default:
      LOG(FATAL) << "Unknown dense linear algebra library type : "
                 << DenseLinearAlgebraLibraryTypeToString(
                        options.dense_linear_algebra_library_type);
  }
  return dense_qr;
}

LinearSolverTerminationType DenseQR::FactorAndSolve(int num_rows,
                                                    int num_cols,
                                                    double* lhs,
                                                    const double* rhs,
                                                    double* solution,
                                                    std::string* message) {
  LinearSolverTerminationType termination_type =
      Factorize(num_rows, num_cols, lhs, message);
  if (termination_type == LinearSolverTerminationType::SUCCESS) {
    termination_type = Solve(rhs, solution, message);
  }
  return termination_type;
}

LinearSolverTerminationType EigenDenseQR::Factorize(int num_rows,
                                                    int num_cols,
                                                    double* lhs,
                                                    std::string* message) {
  Eigen::Map<ColMajorMatrix> m(lhs, num_rows, num_cols);
  qr_ = std::make_unique<QRType>(m);
  *message = "Success.";
  return LinearSolverTerminationType::SUCCESS;
}

LinearSolverTerminationType EigenDenseQR::Solve(const double* rhs,
                                                double* solution,
                                                std::string* message) {
  VectorRef(solution, qr_->cols()) =
      qr_->solve(ConstVectorRef(rhs, qr_->rows()));
  *message = "Success.";
  return LinearSolverTerminationType::SUCCESS;
}

#ifndef CERES_NO_LAPACK
LinearSolverTerminationType LAPACKDenseQR::Factorize(int num_rows,
                                                     int num_cols,
                                                     double* lhs,
                                                     std::string* message) {
  int lwork = -1;
  double work_size;
  int info = 0;

  // Compute the size of the temporary workspace needed to compute the QR
  // factorization in the dgeqrf call below.
  dgeqrf_(&num_rows,
          &num_cols,
          lhs_,
          &num_rows,
          tau_.data(),
          &work_size,
          &lwork,
          &info);
  if (info < 0) {
    LOG(FATAL) << "Congratulations, you found a bug in Ceres."
               << "Please report it."
               << "LAPACK::dgels fatal error."
               << "Argument: " << -info << " is invalid.";
  }

  lhs_ = lhs;
  num_rows_ = num_rows;
  num_cols_ = num_cols;

  lwork = static_cast<int>(work_size);

  if (work_.size() < lwork) {
    work_.resize(lwork);
  }
  if (tau_.size() < num_cols) {
    tau_.resize(num_cols);
  }

  if (q_transpose_rhs_.size() < num_rows) {
    q_transpose_rhs_.resize(num_rows);
  }

  // Factorize the lhs_ using the workspace that we just constructed above.
  dgeqrf_(&num_rows,
          &num_cols,
          lhs_,
          &num_rows,
          tau_.data(),
          work_.data(),
          &lwork,
          &info);

  if (info < 0) {
    LOG(FATAL) << "Congratulations, you found a bug in Ceres."
               << "Please report it. dgeqrf fatal error."
               << "Argument: " << -info << " is invalid.";
  }

  termination_type_ = LinearSolverTerminationType::SUCCESS;
  *message = "Success.";
  return termination_type_;
}

LinearSolverTerminationType LAPACKDenseQR::Solve(const double* rhs,
                                                 double* solution,
                                                 std::string* message) {
  if (termination_type_ != LinearSolverTerminationType::SUCCESS) {
    *message = "QR factorization failed and solve called.";
    return termination_type_;
  }

  std::copy_n(rhs, num_rows_, q_transpose_rhs_.data());

  const char side = 'L';
  char trans = 'T';
  const int num_c_cols = 1;
  const int lwork = work_.size();
  int info = 0;
  dormqr_(&side,
          &trans,
          &num_rows_,
          &num_c_cols,
          &num_cols_,
          lhs_,
          &num_rows_,
          tau_.data(),
          q_transpose_rhs_.data(),
          &num_rows_,
          work_.data(),
          &lwork,
          &info);
  if (info < 0) {
    LOG(FATAL) << "Congratulations, you found a bug in Ceres."
               << "Please report it. dormr fatal error."
               << "Argument: " << -info << " is invalid.";
  }

  const char uplo = 'U';
  trans = 'N';
  const char diag = 'N';
  dtrtrs_(&uplo,
          &trans,
          &diag,
          &num_cols_,
          &num_c_cols,
          lhs_,
          &num_rows_,
          q_transpose_rhs_.data(),
          &num_rows_,
          &info);

  if (info < 0) {
    LOG(FATAL) << "Congratulations, you found a bug in Ceres."
               << "Please report it. dormr fatal error."
               << "Argument: " << -info << " is invalid.";
  } else if (info > 0) {
    *message =
        "QR factorization failure. The factorization is not full rank. R has "
        "zeros on the diagonal.";
    termination_type_ = LinearSolverTerminationType::FAILURE;
  } else {
    std::copy_n(q_transpose_rhs_.data(), num_cols_, solution);
    termination_type_ = LinearSolverTerminationType::SUCCESS;
  }

  return termination_type_;
}

#endif  // CERES_NO_LAPACK

#ifndef CERES_NO_CUDA

CUDADenseQR::CUDADenseQR(ContextImpl* context)
    : context_(context),
      lhs_{context},
      rhs_{context},
      tau_{context},
      device_workspace_{context},
      error_(context, 1) {}

LinearSolverTerminationType CUDADenseQR::Factorize(int num_rows,
                                                   int num_cols,
                                                   double* lhs,
                                                   std::string* message) {
  factorize_result_ = LinearSolverTerminationType::FATAL_ERROR;
  lhs_.Reserve(num_rows * num_cols);
  tau_.Reserve(std::min(num_rows, num_cols));
  num_rows_ = num_rows;
  num_cols_ = num_cols;
  lhs_.CopyFromCpu(lhs, num_rows * num_cols);
  int device_workspace_size = 0;
  if (cusolverDnDgeqrf_bufferSize(context_->cusolver_handle_,
                                  num_rows,
                                  num_cols,
                                  lhs_.data(),
                                  num_rows,
                                  &device_workspace_size) !=
      CUSOLVER_STATUS_SUCCESS) {
    *message = "cuSolverDN::cusolverDnDgeqrf_bufferSize failed.";
    return LinearSolverTerminationType::FATAL_ERROR;
  }
  device_workspace_.Reserve(device_workspace_size);
  if (cusolverDnDgeqrf(context_->cusolver_handle_,
                       num_rows,
                       num_cols,
                       lhs_.data(),
                       num_rows,
                       tau_.data(),
                       reinterpret_cast<double*>(device_workspace_.data()),
                       device_workspace_.size(),
                       error_.data()) != CUSOLVER_STATUS_SUCCESS) {
    *message = "cuSolverDN::cusolverDnDgeqrf failed.";
    return LinearSolverTerminationType::FATAL_ERROR;
  }
  int error = 0;
  error_.CopyToCpu(&error, 1);
  if (error < 0) {
    LOG(FATAL) << "Congratulations, you found a bug in Ceres - "
               << "please report it. "
               << "cuSolverDN::cusolverDnDgeqrf fatal error. "
               << "Argument: " << -error << " is invalid.";
    // The following line is unreachable, but return failure just to be
    // pedantic, since the compiler does not know that.
    return LinearSolverTerminationType::FATAL_ERROR;
  }

  *message = "Success";
  factorize_result_ = LinearSolverTerminationType::SUCCESS;
  return LinearSolverTerminationType::SUCCESS;
}

LinearSolverTerminationType CUDADenseQR::Solve(const double* rhs,
                                               double* solution,
                                               std::string* message) {
  if (factorize_result_ != LinearSolverTerminationType::SUCCESS) {
    *message = "Factorize did not complete successfully previously.";
    return factorize_result_;
  }
  rhs_.CopyFromCpu(rhs, num_rows_);
  int device_workspace_size = 0;
  if (cusolverDnDormqr_bufferSize(context_->cusolver_handle_,
                                  CUBLAS_SIDE_LEFT,
                                  CUBLAS_OP_T,
                                  num_rows_,
                                  1,
                                  num_cols_,
                                  lhs_.data(),
                                  num_rows_,
                                  tau_.data(),
                                  rhs_.data(),
                                  num_rows_,
                                  &device_workspace_size) !=
      CUSOLVER_STATUS_SUCCESS) {
    *message = "cuSolverDN::cusolverDnDormqr_bufferSize failed.";
    return LinearSolverTerminationType::FATAL_ERROR;
  }
  device_workspace_.Reserve(device_workspace_size);
  // Compute rhs = Q^T * rhs, assuming that lhs has already been factorized.
  // The result of factorization would have stored Q in a packed form in lhs_.
  if (cusolverDnDormqr(context_->cusolver_handle_,
                       CUBLAS_SIDE_LEFT,
                       CUBLAS_OP_T,
                       num_rows_,
                       1,
                       num_cols_,
                       lhs_.data(),
                       num_rows_,
                       tau_.data(),
                       rhs_.data(),
                       num_rows_,
                       reinterpret_cast<double*>(device_workspace_.data()),
                       device_workspace_.size(),
                       error_.data()) != CUSOLVER_STATUS_SUCCESS) {
    *message = "cuSolverDN::cusolverDnDormqr failed.";
    return LinearSolverTerminationType::FATAL_ERROR;
  }
  int error = 0;
  error_.CopyToCpu(&error, 1);
  if (error < 0) {
    LOG(FATAL) << "Congratulations, you found a bug in Ceres. "
               << "Please report it."
               << "cuSolverDN::cusolverDnDormqr fatal error. "
               << "Argument: " << -error << " is invalid.";
  }
  // Compute the solution vector as x = R \ (Q^T * rhs). Since the previous step
  // replaced rhs by (Q^T * rhs), this is just x = R \ rhs.
  if (cublasDtrsv(context_->cublas_handle_,
                  CUBLAS_FILL_MODE_UPPER,
                  CUBLAS_OP_N,
                  CUBLAS_DIAG_NON_UNIT,
                  num_cols_,
                  lhs_.data(),
                  num_rows_,
                  rhs_.data(),
                  1) != CUBLAS_STATUS_SUCCESS) {
    *message = "cuBLAS::cublasDtrsv failed.";
    return LinearSolverTerminationType::FATAL_ERROR;
  }
  rhs_.CopyToCpu(solution, num_cols_);
  *message = "Success";
  return LinearSolverTerminationType::SUCCESS;
}

std::unique_ptr<CUDADenseQR> CUDADenseQR::Create(
    const LinearSolver::Options& options) {
  if (options.dense_linear_algebra_library_type != CUDA ||
      options.context == nullptr || !options.context->IsCudaInitialized()) {
    return nullptr;
  }
  return std::unique_ptr<CUDADenseQR>(new CUDADenseQR(options.context));
}

#endif  // CERES_NO_CUDA

}  // namespace ceres::internal
