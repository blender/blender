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
// Author: joydeepb@cs.utexas.edu (Joydeep Biswas)

#include "ceres/cuda_sparse_matrix.h"

#include <string>

#include "ceres/block_sparse_matrix.h"
#include "ceres/casts.h"
#include "ceres/cuda_vector.h"
#include "ceres/internal/config.h"
#include "ceres/internal/eigen.h"
#include "ceres/linear_least_squares_problems.h"
#include "ceres/triplet_sparse_matrix.h"
#include "glog/logging.h"
#include "gtest/gtest.h"

namespace ceres {
namespace internal {

#ifndef CERES_NO_CUDA

class CudaSparseMatrixTest : public ::testing::Test {
 protected:
  void SetUp() final {
    std::string message;
    CHECK(context_.InitCuda(&message))
        << "InitCuda() failed because: " << message;
    std::unique_ptr<LinearLeastSquaresProblem> problem =
        CreateLinearLeastSquaresProblemFromId(2);
    CHECK(problem != nullptr);
    A_.reset(down_cast<BlockSparseMatrix*>(problem->A.release()));
    CHECK(A_ != nullptr);
    CHECK(problem->b != nullptr);
    CHECK(problem->x != nullptr);
    b_.resize(A_->num_rows());
    for (int i = 0; i < A_->num_rows(); ++i) {
      b_[i] = problem->b[i];
    }
    x_.resize(A_->num_cols());
    for (int i = 0; i < A_->num_cols(); ++i) {
      x_[i] = problem->x[i];
    }
    CHECK_EQ(A_->num_rows(), b_.rows());
    CHECK_EQ(A_->num_cols(), x_.rows());
  }

  std::unique_ptr<BlockSparseMatrix> A_;
  Vector x_;
  Vector b_;
  ContextImpl context_;
};

TEST_F(CudaSparseMatrixTest, RightMultiplyAndAccumulate) {
  std::string message;
  auto A_crs = A_->ToCompressedRowSparseMatrix();
  CudaSparseMatrix A_gpu(&context_, *A_crs);
  CudaVector x_gpu(&context_, A_gpu.num_cols());
  CudaVector res_gpu(&context_, A_gpu.num_rows());
  x_gpu.CopyFromCpu(x_);

  const Vector minus_b = -b_;
  // res = -b
  res_gpu.CopyFromCpu(minus_b);
  // res += A * x
  A_gpu.RightMultiplyAndAccumulate(x_gpu, &res_gpu);

  Vector res;
  res_gpu.CopyTo(&res);

  Vector res_expected = minus_b;
  A_->RightMultiplyAndAccumulate(x_.data(), res_expected.data());

  EXPECT_LE((res - res_expected).norm(),
            std::numeric_limits<double>::epsilon() * 1e3);
}

TEST(CudaSparseMatrix, CopyValuesFromCpu) {
  // A1:
  // [ 1 1 0 0
  //   0 1 1 0]
  // A2:
  // [ 1 2 0 0
  //   0 3 4 0]
  // b: [1 2 3 4]'
  // A1 * b = [3 5]'
  // A2 * b = [5 18]'
  TripletSparseMatrix A1(2, 4, {0, 0, 1, 1}, {0, 1, 1, 2}, {1, 1, 1, 1});
  TripletSparseMatrix A2(2, 4, {0, 0, 1, 1}, {0, 1, 1, 2}, {1, 2, 3, 4});
  Vector b(4);
  b << 1, 2, 3, 4;

  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  auto A1_crs = CompressedRowSparseMatrix::FromTripletSparseMatrix(A1);
  CudaSparseMatrix A_gpu(&context, *A1_crs);
  CudaVector b_gpu(&context, A1.num_cols());
  CudaVector x_gpu(&context, A1.num_rows());
  b_gpu.CopyFromCpu(b);
  x_gpu.SetZero();

  Vector x_expected(2);
  x_expected << 3, 5;
  A_gpu.RightMultiplyAndAccumulate(b_gpu, &x_gpu);
  Vector x_computed;
  x_gpu.CopyTo(&x_computed);
  EXPECT_EQ(x_computed, x_expected);

  auto A2_crs = CompressedRowSparseMatrix::FromTripletSparseMatrix(A2);
  A_gpu.CopyValuesFromCpu(*A2_crs);
  x_gpu.SetZero();
  x_expected << 5, 18;
  A_gpu.RightMultiplyAndAccumulate(b_gpu, &x_gpu);
  x_gpu.CopyTo(&x_computed);
  EXPECT_EQ(x_computed, x_expected);
}

TEST(CudaSparseMatrix, RightMultiplyAndAccumulate) {
  // A:
  // [ 1 2 0 0
  //   0 3 4 0]
  // b: [1 2 3 4]'
  // A * b = [5 18]'
  TripletSparseMatrix A(2, 4, {0, 0, 1, 1}, {0, 1, 1, 2}, {1, 2, 3, 4});
  Vector b(4);
  b << 1, 2, 3, 4;
  Vector x_expected(2);
  x_expected << 5, 18;

  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  auto A_crs = CompressedRowSparseMatrix::FromTripletSparseMatrix(A);
  CudaSparseMatrix A_gpu(&context, *A_crs);
  CudaVector b_gpu(&context, A.num_cols());
  CudaVector x_gpu(&context, A.num_rows());
  b_gpu.CopyFromCpu(b);
  x_gpu.SetZero();

  A_gpu.RightMultiplyAndAccumulate(b_gpu, &x_gpu);

  Vector x_computed;
  x_gpu.CopyTo(&x_computed);

  EXPECT_EQ(x_computed, x_expected);
}

TEST(CudaSparseMatrix, LeftMultiplyAndAccumulate) {
  // A:
  // [ 1 2 0 0
  //   0 3 4 0]
  // b: [1 2]'
  // A'* b = [1 8 8 0]'
  TripletSparseMatrix A(2, 4, {0, 0, 1, 1}, {0, 1, 1, 2}, {1, 2, 3, 4});
  Vector b(2);
  b << 1, 2;
  Vector x_expected(4);
  x_expected << 1, 8, 8, 0;

  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  auto A_crs = CompressedRowSparseMatrix::FromTripletSparseMatrix(A);
  CudaSparseMatrix A_gpu(&context, *A_crs);
  CudaVector b_gpu(&context, A.num_rows());
  CudaVector x_gpu(&context, A.num_cols());
  b_gpu.CopyFromCpu(b);
  x_gpu.SetZero();

  A_gpu.LeftMultiplyAndAccumulate(b_gpu, &x_gpu);

  Vector x_computed;
  x_gpu.CopyTo(&x_computed);

  EXPECT_EQ(x_computed, x_expected);
}

// If there are numerical errors due to synchronization issues, they will show
// up when testing with large matrices, since each operation will take
// significant time, thus hopefully revealing any potential synchronization
// issues.
TEST(CudaSparseMatrix, LargeMultiplyAndAccumulate) {
  // Create a large NxN matrix A that has the following structure:
  // In row i, only columns i and i+1 are non-zero.
  // A_{i, i} = A_{i, i+1} = 1.
  // There will be 2 * N - 1 non-zero elements in A.
  // X = [1:N]
  // Right multiply test:
  // b = A * X
  // Left multiply test:
  // b = A' * X

  const int N = 10 * 1000 * 1000;
  const int num_non_zeros = 2 * N - 1;
  std::vector<int> row_indices(num_non_zeros);
  std::vector<int> col_indices(num_non_zeros);
  std::vector<double> values(num_non_zeros);

  for (int i = 0; i < N; ++i) {
    row_indices[2 * i] = i;
    col_indices[2 * i] = i;
    values[2 * i] = 1.0;
    if (i + 1 < N) {
      col_indices[2 * i + 1] = i + 1;
      row_indices[2 * i + 1] = i;
      values[2 * i + 1] = 1;
    }
  }
  TripletSparseMatrix A(N, N, row_indices, col_indices, values);
  Vector x(N);
  for (int i = 0; i < N; ++i) {
    x[i] = i + 1;
  }

  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  auto A_crs = CompressedRowSparseMatrix::FromTripletSparseMatrix(A);
  CudaSparseMatrix A_gpu(&context, *A_crs);
  CudaVector b_gpu(&context, N);
  CudaVector x_gpu(&context, N);
  x_gpu.CopyFromCpu(x);

  // First check RightMultiply.
  {
    b_gpu.SetZero();
    A_gpu.RightMultiplyAndAccumulate(x_gpu, &b_gpu);
    Vector b_computed;
    b_gpu.CopyTo(&b_computed);
    for (int i = 0; i < N; ++i) {
      if (i + 1 < N) {
        EXPECT_EQ(b_computed[i], 2 * (i + 1) + 1);
      } else {
        EXPECT_EQ(b_computed[i], i + 1);
      }
    }
  }

  // Next check LeftMultiply.
  {
    b_gpu.SetZero();
    A_gpu.LeftMultiplyAndAccumulate(x_gpu, &b_gpu);
    Vector b_computed;
    b_gpu.CopyTo(&b_computed);
    for (int i = 0; i < N; ++i) {
      if (i > 0) {
        EXPECT_EQ(b_computed[i], 2 * (i + 1) - 1);
      } else {
        EXPECT_EQ(b_computed[i], i + 1);
      }
    }
  }
}

#endif  // CERES_NO_CUDA

}  // namespace internal
}  // namespace ceres
