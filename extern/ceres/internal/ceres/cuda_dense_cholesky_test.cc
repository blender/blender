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

#include <string>

#include "ceres/dense_cholesky.h"
#include "ceres/internal/config.h"
#include "ceres/internal/eigen.h"
#include "glog/logging.h"
#include "gtest/gtest.h"

namespace ceres::internal {

#ifndef CERES_NO_CUDA

TEST(CUDADenseCholesky, InvalidOptionOnCreate) {
  LinearSolver::Options options;
  ContextImpl context;
  options.context = &context;
  std::string error;
  EXPECT_TRUE(context.InitCuda(&error)) << error;
  auto dense_cuda_solver = CUDADenseCholesky::Create(options);
  EXPECT_EQ(dense_cuda_solver, nullptr);
}

// Tests the CUDA Cholesky solver with a simple 4x4 matrix.
TEST(CUDADenseCholesky, Cholesky4x4Matrix) {
  Eigen::Matrix4d A;
  // clang-format off
  A <<  4,  12, -16, 0,
       12,  37, -43, 0,
      -16, -43,  98, 0,
        0,   0,   0, 1;
  // clang-format on

  Vector b = Eigen::Vector4d::Ones();
  LinearSolver::Options options;
  ContextImpl context;
  options.context = &context;
  std::string error;
  EXPECT_TRUE(context.InitCuda(&error)) << error;
  options.dense_linear_algebra_library_type = CUDA;
  auto dense_cuda_solver = CUDADenseCholesky::Create(options);
  ASSERT_NE(dense_cuda_solver, nullptr);
  std::string error_string;
  ASSERT_EQ(dense_cuda_solver->Factorize(A.cols(), A.data(), &error_string),
            LinearSolverTerminationType::SUCCESS);
  Eigen::Vector4d x = Eigen::Vector4d::Zero();
  ASSERT_EQ(dense_cuda_solver->Solve(b.data(), x.data(), &error_string),
            LinearSolverTerminationType::SUCCESS);
  static const double kEpsilon = std::numeric_limits<double>::epsilon() * 10;
  const Eigen::Vector4d x_expected(113.75 / 3.0, -31.0 / 3.0, 5.0 / 3.0, 1.0);
  EXPECT_NEAR((x[0] - x_expected[0]) / x_expected[0], 0.0, kEpsilon);
  EXPECT_NEAR((x[1] - x_expected[1]) / x_expected[1], 0.0, kEpsilon);
  EXPECT_NEAR((x[2] - x_expected[2]) / x_expected[2], 0.0, kEpsilon);
  EXPECT_NEAR((x[3] - x_expected[3]) / x_expected[3], 0.0, kEpsilon);
}

TEST(CUDADenseCholesky, SingularMatrix) {
  Eigen::Matrix3d A;
  // clang-format off
  A <<  1, 0, 0,
        0, 1, 0,
        0, 0, 0;
  // clang-format on

  LinearSolver::Options options;
  ContextImpl context;
  options.context = &context;
  std::string error;
  EXPECT_TRUE(context.InitCuda(&error)) << error;
  options.dense_linear_algebra_library_type = CUDA;
  auto dense_cuda_solver = CUDADenseCholesky::Create(options);
  ASSERT_NE(dense_cuda_solver, nullptr);
  std::string error_string;
  ASSERT_EQ(dense_cuda_solver->Factorize(A.cols(), A.data(), &error_string),
            LinearSolverTerminationType::FAILURE);
}

TEST(CUDADenseCholesky, NegativeMatrix) {
  Eigen::Matrix3d A;
  // clang-format off
  A <<  1, 0, 0,
        0, 1, 0,
        0, 0, -1;
  // clang-format on

  LinearSolver::Options options;
  ContextImpl context;
  options.context = &context;
  std::string error;
  EXPECT_TRUE(context.InitCuda(&error)) << error;
  options.dense_linear_algebra_library_type = CUDA;
  auto dense_cuda_solver = CUDADenseCholesky::Create(options);
  ASSERT_NE(dense_cuda_solver, nullptr);
  std::string error_string;
  ASSERT_EQ(dense_cuda_solver->Factorize(A.cols(), A.data(), &error_string),
            LinearSolverTerminationType::FAILURE);
}

TEST(CUDADenseCholesky, MustFactorizeBeforeSolve) {
  const Eigen::Vector3d b = Eigen::Vector3d::Ones();
  LinearSolver::Options options;
  ContextImpl context;
  options.context = &context;
  std::string error;
  EXPECT_TRUE(context.InitCuda(&error)) << error;
  options.dense_linear_algebra_library_type = CUDA;
  auto dense_cuda_solver = CUDADenseCholesky::Create(options);
  ASSERT_NE(dense_cuda_solver, nullptr);
  std::string error_string;
  ASSERT_EQ(dense_cuda_solver->Solve(b.data(), nullptr, &error_string),
            LinearSolverTerminationType::FATAL_ERROR);
}

TEST(CUDADenseCholesky, Randomized1600x1600Tests) {
  const int kNumCols = 1600;
  using LhsType = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic>;
  using RhsType = Eigen::Matrix<double, Eigen::Dynamic, 1>;
  using SolutionType = Eigen::Matrix<double, Eigen::Dynamic, 1>;

  LinearSolver::Options options;
  ContextImpl context;
  options.context = &context;
  std::string error;
  EXPECT_TRUE(context.InitCuda(&error)) << error;
  options.dense_linear_algebra_library_type = ceres::CUDA;
  std::unique_ptr<DenseCholesky> dense_cholesky =
      CUDADenseCholesky::Create(options);

  const int kNumTrials = 20;
  for (int i = 0; i < kNumTrials; ++i) {
    LhsType lhs = LhsType::Random(kNumCols, kNumCols);
    lhs = lhs.transpose() * lhs;
    lhs += 1e-3 * LhsType::Identity(kNumCols, kNumCols);
    SolutionType x_expected = SolutionType::Random(kNumCols);
    RhsType rhs = lhs * x_expected;
    SolutionType x_computed = SolutionType::Zero(kNumCols);
    // Sanity check the random matrix sizes.
    EXPECT_EQ(lhs.rows(), kNumCols);
    EXPECT_EQ(lhs.cols(), kNumCols);
    EXPECT_EQ(rhs.rows(), kNumCols);
    EXPECT_EQ(rhs.cols(), 1);
    EXPECT_EQ(x_expected.rows(), kNumCols);
    EXPECT_EQ(x_expected.cols(), 1);
    EXPECT_EQ(x_computed.rows(), kNumCols);
    EXPECT_EQ(x_computed.cols(), 1);
    LinearSolver::Summary summary;
    summary.termination_type = dense_cholesky->FactorAndSolve(
        kNumCols, lhs.data(), rhs.data(), x_computed.data(), &summary.message);
    ASSERT_EQ(summary.termination_type, LinearSolverTerminationType::SUCCESS);
    static const double kEpsilon = std::numeric_limits<double>::epsilon() * 3e5;
    ASSERT_NEAR(
        (x_computed - x_expected).norm() / x_expected.norm(), 0.0, kEpsilon);
  }
}

TEST(CUDADenseCholeskyMixedPrecision, InvalidOptionsOnCreate) {
  {
    // Did not ask for CUDA, and did not ask for mixed precision.
    LinearSolver::Options options;
    ContextImpl context;
    options.context = &context;
    std::string error;
    EXPECT_TRUE(context.InitCuda(&error)) << error;
    auto solver = CUDADenseCholeskyMixedPrecision::Create(options);
    ASSERT_EQ(solver, nullptr);
  }
  {
    // Asked for CUDA, but did not ask for mixed precision.
    LinearSolver::Options options;
    ContextImpl context;
    options.context = &context;
    std::string error;
    EXPECT_TRUE(context.InitCuda(&error)) << error;
    options.dense_linear_algebra_library_type = ceres::CUDA;
    auto solver = CUDADenseCholeskyMixedPrecision::Create(options);
    ASSERT_EQ(solver, nullptr);
  }
}

// Tests the CUDA Cholesky solver with a simple 4x4 matrix.
TEST(CUDADenseCholeskyMixedPrecision, Cholesky4x4Matrix1Step) {
  Eigen::Matrix4d A;
  // clang-format off
  // A common test Cholesky decomposition test matrix, see :
  // https://en.wikipedia.org/w/index.php?title=Cholesky_decomposition&oldid=1080607368#Example
  A <<  4,  12, -16, 0,
       12,  37, -43, 0,
      -16, -43,  98, 0,
        0,   0,   0, 1;
  // clang-format on

  const Eigen::Vector4d b = Eigen::Vector4d::Ones();
  LinearSolver::Options options;
  options.max_num_refinement_iterations = 0;
  ContextImpl context;
  options.context = &context;
  std::string error;
  EXPECT_TRUE(context.InitCuda(&error)) << error;
  options.dense_linear_algebra_library_type = CUDA;
  options.use_mixed_precision_solves = true;
  auto solver = CUDADenseCholeskyMixedPrecision::Create(options);
  ASSERT_NE(solver, nullptr);
  std::string error_string;
  ASSERT_EQ(solver->Factorize(A.cols(), A.data(), &error_string),
            LinearSolverTerminationType::SUCCESS);
  Eigen::Vector4d x = Eigen::Vector4d::Zero();
  ASSERT_EQ(solver->Solve(b.data(), x.data(), &error_string),
            LinearSolverTerminationType::SUCCESS);
  // A single step of the mixed precision solver will be equivalent to solving
  // in low precision (FP32). Hence the tolerance is defined w.r.t. FP32 epsilon
  // instead of FP64 epsilon.
  static const double kEpsilon = std::numeric_limits<float>::epsilon() * 10;
  const Eigen::Vector4d x_expected(113.75 / 3.0, -31.0 / 3.0, 5.0 / 3.0, 1.0);
  EXPECT_NEAR((x[0] - x_expected[0]) / x_expected[0], 0.0, kEpsilon);
  EXPECT_NEAR((x[1] - x_expected[1]) / x_expected[1], 0.0, kEpsilon);
  EXPECT_NEAR((x[2] - x_expected[2]) / x_expected[2], 0.0, kEpsilon);
  EXPECT_NEAR((x[3] - x_expected[3]) / x_expected[3], 0.0, kEpsilon);
}

// Tests the CUDA Cholesky solver with a simple 4x4 matrix.
TEST(CUDADenseCholeskyMixedPrecision, Cholesky4x4Matrix4Steps) {
  Eigen::Matrix4d A;
  // clang-format off
  A <<  4,  12, -16, 0,
       12,  37, -43, 0,
      -16, -43,  98, 0,
        0,   0,   0, 1;
  // clang-format on

  const Eigen::Vector4d b = Eigen::Vector4d::Ones();
  LinearSolver::Options options;
  options.max_num_refinement_iterations = 3;
  ContextImpl context;
  options.context = &context;
  std::string error;
  EXPECT_TRUE(context.InitCuda(&error)) << error;
  options.dense_linear_algebra_library_type = CUDA;
  options.use_mixed_precision_solves = true;
  auto solver = CUDADenseCholeskyMixedPrecision::Create(options);
  ASSERT_NE(solver, nullptr);
  std::string error_string;
  ASSERT_EQ(solver->Factorize(A.cols(), A.data(), &error_string),
            LinearSolverTerminationType::SUCCESS);
  Eigen::Vector4d x = Eigen::Vector4d::Zero();
  ASSERT_EQ(solver->Solve(b.data(), x.data(), &error_string),
            LinearSolverTerminationType::SUCCESS);
  // The error does not reduce beyond four iterations, and stagnates at this
  // level of precision.
  static const double kEpsilon = std::numeric_limits<double>::epsilon() * 100;
  const Eigen::Vector4d x_expected(113.75 / 3.0, -31.0 / 3.0, 5.0 / 3.0, 1.0);
  EXPECT_NEAR((x[0] - x_expected[0]) / x_expected[0], 0.0, kEpsilon);
  EXPECT_NEAR((x[1] - x_expected[1]) / x_expected[1], 0.0, kEpsilon);
  EXPECT_NEAR((x[2] - x_expected[2]) / x_expected[2], 0.0, kEpsilon);
  EXPECT_NEAR((x[3] - x_expected[3]) / x_expected[3], 0.0, kEpsilon);
}

TEST(CUDADenseCholeskyMixedPrecision, Randomized1600x1600Tests) {
  const int kNumCols = 1600;
  using LhsType = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic>;
  using RhsType = Eigen::Matrix<double, Eigen::Dynamic, 1>;
  using SolutionType = Eigen::Matrix<double, Eigen::Dynamic, 1>;

  LinearSolver::Options options;
  ContextImpl context;
  options.context = &context;
  std::string error;
  EXPECT_TRUE(context.InitCuda(&error)) << error;
  options.dense_linear_algebra_library_type = ceres::CUDA;
  options.use_mixed_precision_solves = true;
  options.max_num_refinement_iterations = 20;
  std::unique_ptr<CUDADenseCholeskyMixedPrecision> dense_cholesky =
      CUDADenseCholeskyMixedPrecision::Create(options);

  const int kNumTrials = 20;
  for (int i = 0; i < kNumTrials; ++i) {
    LhsType lhs = LhsType::Random(kNumCols, kNumCols);
    lhs = lhs.transpose() * lhs;
    lhs += 1e-3 * LhsType::Identity(kNumCols, kNumCols);
    SolutionType x_expected = SolutionType::Random(kNumCols);
    RhsType rhs = lhs * x_expected;
    SolutionType x_computed = SolutionType::Zero(kNumCols);
    // Sanity check the random matrix sizes.
    EXPECT_EQ(lhs.rows(), kNumCols);
    EXPECT_EQ(lhs.cols(), kNumCols);
    EXPECT_EQ(rhs.rows(), kNumCols);
    EXPECT_EQ(rhs.cols(), 1);
    EXPECT_EQ(x_expected.rows(), kNumCols);
    EXPECT_EQ(x_expected.cols(), 1);
    EXPECT_EQ(x_computed.rows(), kNumCols);
    EXPECT_EQ(x_computed.cols(), 1);
    LinearSolver::Summary summary;
    summary.termination_type = dense_cholesky->FactorAndSolve(
        kNumCols, lhs.data(), rhs.data(), x_computed.data(), &summary.message);
    ASSERT_EQ(summary.termination_type, LinearSolverTerminationType::SUCCESS);
    static const double kEpsilon = std::numeric_limits<double>::epsilon() * 1e6;
    ASSERT_NEAR(
        (x_computed - x_expected).norm() / x_expected.norm(), 0.0, kEpsilon);
  }
}

#endif  // CERES_NO_CUDA

}  // namespace ceres::internal
