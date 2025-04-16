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

#include "ceres/cuda_vector.h"

#include <string>

#include "ceres/internal/config.h"
#include "ceres/internal/eigen.h"
#include "glog/logging.h"
#include "gtest/gtest.h"

namespace ceres {
namespace internal {

#ifndef CERES_NO_CUDA

TEST(CudaVector, Creation) {
  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  CudaVector x(&context, 1000);
  EXPECT_EQ(x.num_rows(), 1000);
  EXPECT_NE(x.data(), nullptr);
}

TEST(CudaVector, CopyVector) {
  Vector x(3);
  x << 1, 2, 3;
  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  CudaVector y(&context, 10);
  y.CopyFromCpu(x);
  EXPECT_EQ(y.num_rows(), 3);

  Vector z(3);
  z << 0, 0, 0;
  y.CopyTo(&z);
  EXPECT_EQ(x, z);
}

TEST(CudaVector, Move) {
  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  CudaVector y(&context, 10);
  const auto y_data = y.data();
  const auto y_descr = y.descr();
  EXPECT_EQ(y.num_rows(), 10);
  CudaVector z(std::move(y));
  EXPECT_EQ(y.data(), nullptr);
  EXPECT_EQ(y.descr(), nullptr);
  EXPECT_EQ(y.num_rows(), 0);

  EXPECT_EQ(z.data(), y_data);
  EXPECT_EQ(z.descr(), y_descr);
}

TEST(CudaVector, DeepCopy) {
  Vector x(3);
  x << 1, 2, 3;
  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  CudaVector x_gpu(&context, 3);
  x_gpu.CopyFromCpu(x);

  CudaVector y_gpu(&context, 3);
  y_gpu.SetZero();
  EXPECT_EQ(y_gpu.Norm(), 0.0);

  y_gpu = x_gpu;
  Vector y(3);
  y << 0, 0, 0;
  y_gpu.CopyTo(&y);
  EXPECT_EQ(x, y);
}

TEST(CudaVector, Dot) {
  Vector x(3);
  Vector y(3);
  x << 1, 2, 3;
  y << 100, 10, 1;
  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  CudaVector x_gpu(&context, 10);
  CudaVector y_gpu(&context, 10);
  x_gpu.CopyFromCpu(x);
  y_gpu.CopyFromCpu(y);

  EXPECT_EQ(x_gpu.Dot(y_gpu), 123.0);
  EXPECT_EQ(Dot(x_gpu, y_gpu), 123.0);
}

TEST(CudaVector, Norm) {
  Vector x(3);
  x << 1, 2, 3;
  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  CudaVector x_gpu(&context, 10);
  x_gpu.CopyFromCpu(x);

  EXPECT_NEAR(x_gpu.Norm(),
              sqrt(1.0 + 4.0 + 9.0),
              std::numeric_limits<double>::epsilon());

  EXPECT_NEAR(Norm(x_gpu),
              sqrt(1.0 + 4.0 + 9.0),
              std::numeric_limits<double>::epsilon());
}

TEST(CudaVector, SetZero) {
  Vector x(4);
  x << 1, 1, 1, 1;
  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  CudaVector x_gpu(&context, 10);
  x_gpu.CopyFromCpu(x);

  EXPECT_NEAR(x_gpu.Norm(), 2.0, std::numeric_limits<double>::epsilon());

  x_gpu.SetZero();
  EXPECT_NEAR(x_gpu.Norm(), 0.0, std::numeric_limits<double>::epsilon());

  x_gpu.CopyFromCpu(x);
  EXPECT_NEAR(x_gpu.Norm(), 2.0, std::numeric_limits<double>::epsilon());
  SetZero(x_gpu);
  EXPECT_NEAR(x_gpu.Norm(), 0.0, std::numeric_limits<double>::epsilon());
}

TEST(CudaVector, Resize) {
  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  CudaVector x_gpu(&context, 10);
  EXPECT_EQ(x_gpu.num_rows(), 10);
  x_gpu.Resize(4);
  EXPECT_EQ(x_gpu.num_rows(), 4);
}

TEST(CudaVector, Axpy) {
  Vector x(4);
  Vector y(4);
  x << 1, 1, 1, 1;
  y << 100, 10, 1, 0;
  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  CudaVector x_gpu(&context, 4);
  CudaVector y_gpu(&context, 4);
  x_gpu.CopyFromCpu(x);
  y_gpu.CopyFromCpu(y);

  x_gpu.Axpby(2.0, y_gpu, 1.0);
  Vector result;
  Vector expected(4);
  expected << 201, 21, 3, 1;
  x_gpu.CopyTo(&result);
  EXPECT_EQ(result, expected);
}

TEST(CudaVector, AxpbyBEquals1) {
  Vector x(4);
  Vector y(4);
  x << 1, 1, 1, 1;
  y << 100, 10, 1, 0;
  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  CudaVector x_gpu(&context, 4);
  CudaVector y_gpu(&context, 4);
  x_gpu.CopyFromCpu(x);
  y_gpu.CopyFromCpu(y);

  x_gpu.Axpby(2.0, y_gpu, 1.0);
  Vector result;
  Vector expected(4);
  expected << 201, 21, 3, 1;
  x_gpu.CopyTo(&result);
  EXPECT_EQ(result, expected);
}

TEST(CudaVector, AxpbyMemberFunctionBNotEqual1) {
  Vector x(4);
  Vector y(4);
  x << 1, 1, 1, 1;
  y << 100, 10, 1, 0;
  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  CudaVector x_gpu(&context, 4);
  CudaVector y_gpu(&context, 4);
  x_gpu.CopyFromCpu(x);
  y_gpu.CopyFromCpu(y);

  x_gpu.Axpby(2.0, y_gpu, 3.0);
  Vector result;
  Vector expected(4);
  expected << 203, 23, 5, 3;
  x_gpu.CopyTo(&result);
  EXPECT_EQ(result, expected);
}

TEST(CudaVector, AxpbyMemberFunctionBEqual1) {
  Vector x(4);
  Vector y(4);
  x << 1, 1, 1, 1;
  y << 100, 10, 1, 0;
  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  CudaVector x_gpu(&context, 4);
  CudaVector y_gpu(&context, 4);
  x_gpu.CopyFromCpu(x);
  y_gpu.CopyFromCpu(y);

  x_gpu.Axpby(2.0, y_gpu, 1.0);
  Vector result;
  Vector expected(4);
  expected << 201, 21, 3, 1;
  x_gpu.CopyTo(&result);
  EXPECT_EQ(result, expected);
}

TEST(CudaVector, AxpbyMemberXAliasesY) {
  Vector x(4);
  x << 100, 10, 1, 0;
  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  CudaVector x_gpu(&context, 4);
  CudaVector y_gpu(&context, 4);
  x_gpu.CopyFromCpu(x);
  y_gpu.SetZero();

  x_gpu.Axpby(2.0, x_gpu, 1.0);
  Vector result;
  Vector expected(4);
  expected << 300, 30, 3, 0;
  x_gpu.CopyTo(&result);
  EXPECT_EQ(result, expected);
}

TEST(CudaVector, AxpbyNonMemberMethodNoAliases) {
  Vector x(4);
  Vector y(4);
  x << 1, 1, 1, 1;
  y << 100, 10, 1, 0;
  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  CudaVector x_gpu(&context, 4);
  CudaVector y_gpu(&context, 4);
  CudaVector z_gpu(&context, 4);
  x_gpu.CopyFromCpu(x);
  y_gpu.CopyFromCpu(y);
  z_gpu.Resize(4);
  z_gpu.SetZero();

  Axpby(2.0, x_gpu, 3.0, y_gpu, z_gpu);
  Vector result;
  Vector expected(4);
  expected << 302, 32, 5, 2;
  z_gpu.CopyTo(&result);
  EXPECT_EQ(result, expected);
}

TEST(CudaVector, AxpbyNonMemberMethodXAliasesY) {
  Vector x(4);
  x << 100, 10, 1, 0;
  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  CudaVector x_gpu(&context, 4);
  CudaVector z_gpu(&context, 4);
  x_gpu.CopyFromCpu(x);
  z_gpu.SetZero();

  Axpby(2.0, x_gpu, 3.0, x_gpu, z_gpu);
  Vector result;
  Vector expected(4);
  expected << 500, 50, 5, 0;
  z_gpu.CopyTo(&result);
  EXPECT_EQ(result, expected);
}

TEST(CudaVector, AxpbyNonMemberMethodXAliasesZ) {
  Vector x(4);
  Vector y(4);
  x << 1, 1, 1, 1;
  y << 100, 10, 1, 0;
  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  CudaVector x_gpu(&context, 10);
  CudaVector y_gpu(&context, 10);
  x_gpu.CopyFromCpu(x);
  y_gpu.CopyFromCpu(y);

  Axpby(2.0, x_gpu, 3.0, y_gpu, x_gpu);
  Vector result;
  Vector expected(4);
  expected << 302, 32, 5, 2;
  x_gpu.CopyTo(&result);
  EXPECT_EQ(result, expected);
}

TEST(CudaVector, AxpbyNonMemberMethodYAliasesZ) {
  Vector x(4);
  Vector y(4);
  x << 1, 1, 1, 1;
  y << 100, 10, 1, 0;
  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  CudaVector x_gpu(&context, 4);
  CudaVector y_gpu(&context, 4);
  x_gpu.CopyFromCpu(x);
  y_gpu.CopyFromCpu(y);

  Axpby(2.0, x_gpu, 3.0, y_gpu, y_gpu);
  Vector result;
  Vector expected(4);
  expected << 302, 32, 5, 2;
  y_gpu.CopyTo(&result);
  EXPECT_EQ(result, expected);
}

TEST(CudaVector, AxpbyNonMemberMethodXAliasesYAliasesZ) {
  Vector x(4);
  x << 100, 10, 1, 0;
  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  CudaVector x_gpu(&context, 10);
  x_gpu.CopyFromCpu(x);

  Axpby(2.0, x_gpu, 3.0, x_gpu, x_gpu);
  Vector result;
  Vector expected(4);
  expected << 500, 50, 5, 0;
  x_gpu.CopyTo(&result);
  EXPECT_EQ(result, expected);
}

TEST(CudaVector, DtDxpy) {
  Vector x(4);
  Vector y(4);
  Vector D(4);
  x << 1, 2, 3, 4;
  y << 100, 10, 1, 0;
  D << 4, 3, 2, 1;
  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  CudaVector x_gpu(&context, 4);
  CudaVector y_gpu(&context, 4);
  CudaVector D_gpu(&context, 4);
  x_gpu.CopyFromCpu(x);
  y_gpu.CopyFromCpu(y);
  D_gpu.CopyFromCpu(D);

  y_gpu.DtDxpy(D_gpu, x_gpu);
  Vector result;
  Vector expected(4);
  expected << 116, 28, 13, 4;
  y_gpu.CopyTo(&result);
  EXPECT_EQ(result, expected);
}

TEST(CudaVector, Scale) {
  Vector x(4);
  x << 1, 2, 3, 4;
  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;
  CudaVector x_gpu(&context, 4);
  x_gpu.CopyFromCpu(x);

  x_gpu.Scale(-3.0);

  Vector result;
  Vector expected(4);
  expected << -3.0, -6.0, -9.0, -12.0;
  x_gpu.CopyTo(&result);
  EXPECT_EQ(result, expected);
}

#endif  // CERES_NO_CUDA

}  // namespace internal
}  // namespace ceres
