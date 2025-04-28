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
// Authors: dmitriy.korchemkin@gmail.com (Dmitriy Korchemkin)

#include "ceres/internal/config.h"

#ifndef CERES_NO_CUDA

#include <glog/logging.h>
#include <gtest/gtest.h>

#include <numeric>

#include "ceres/cuda_streamed_buffer.h"

namespace ceres::internal {

TEST(CudaStreamedBufferTest, IntegerCopy) {
  // Offsets and sizes of batches supplied to callback
  std::vector<std::pair<int, int>> batches;
  const int kMaxTemporaryArraySize = 16;
  const int kInputSize = kMaxTemporaryArraySize * 7 + 3;
  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;

  std::vector<int> inputs(kInputSize);
  std::vector<int> outputs(kInputSize, -1);
  std::iota(inputs.begin(), inputs.end(), 0);

  CudaStreamedBuffer<int> streamed_buffer(&context, kMaxTemporaryArraySize);
  streamed_buffer.CopyToGpu(inputs.data(),
                            kInputSize,
                            [&outputs, &batches](const int* device_pointer,
                                                 int size,
                                                 int offset,
                                                 cudaStream_t stream) {
                              batches.emplace_back(offset, size);
                              CHECK_EQ(cudaSuccess,
                                       cudaMemcpyAsync(outputs.data() + offset,
                                                       device_pointer,
                                                       sizeof(int) * size,
                                                       cudaMemcpyDeviceToHost,
                                                       stream));
                            });
  // All operations in all streams should be completed when CopyToGpu returns
  // control to the callee
  for (int i = 0; i < ContextImpl::kNumCudaStreams; ++i) {
    CHECK_EQ(cudaSuccess, cudaStreamQuery(context.streams_[i]));
  }

  // Check if every element was visited
  for (int i = 0; i < kInputSize; ++i) {
    CHECK_EQ(outputs[i], i);
  }

  // Check if there is no overlap between batches
  std::sort(batches.begin(), batches.end());
  const int num_batches = batches.size();
  for (int i = 0; i < num_batches; ++i) {
    const auto [begin, size] = batches[i];
    const int end = begin + size;
    CHECK_GE(begin, 0);
    CHECK_LT(begin, kInputSize);

    CHECK_GT(size, 0);
    CHECK_LE(end, kInputSize);

    if (i + 1 == num_batches) continue;
    CHECK_EQ(end, batches[i + 1].first);
  }
}

TEST(CudaStreamedBufferTest, IntegerNoCopy) {
  // Offsets and sizes of batches supplied to callback
  std::vector<std::pair<int, int>> batches;
  const int kMaxTemporaryArraySize = 16;
  const int kInputSize = kMaxTemporaryArraySize * 7 + 3;
  ContextImpl context;
  std::string message;
  CHECK(context.InitCuda(&message)) << "InitCuda() failed because: " << message;

  int* inputs;
  int* outputs;
  CHECK_EQ(cudaSuccess,
           cudaHostAlloc(
               &inputs, sizeof(int) * kInputSize, cudaHostAllocWriteCombined));
  CHECK_EQ(
      cudaSuccess,
      cudaHostAlloc(&outputs, sizeof(int) * kInputSize, cudaHostAllocDefault));

  std::fill(outputs, outputs + kInputSize, -1);
  std::iota(inputs, inputs + kInputSize, 0);

  CudaStreamedBuffer<int> streamed_buffer(&context, kMaxTemporaryArraySize);
  streamed_buffer.CopyToGpu(inputs,
                            kInputSize,
                            [outputs, &batches](const int* device_pointer,
                                                int size,
                                                int offset,
                                                cudaStream_t stream) {
                              batches.emplace_back(offset, size);
                              CHECK_EQ(cudaSuccess,
                                       cudaMemcpyAsync(outputs + offset,
                                                       device_pointer,
                                                       sizeof(int) * size,
                                                       cudaMemcpyDeviceToHost,
                                                       stream));
                            });
  // All operations in all streams should be completed when CopyToGpu returns
  // control to the callee
  for (int i = 0; i < ContextImpl::kNumCudaStreams; ++i) {
    CHECK_EQ(cudaSuccess, cudaStreamQuery(context.streams_[i]));
  }

  // Check if every element was visited
  for (int i = 0; i < kInputSize; ++i) {
    CHECK_EQ(outputs[i], i);
  }

  // Check if there is no overlap between batches
  std::sort(batches.begin(), batches.end());
  const int num_batches = batches.size();
  for (int i = 0; i < num_batches; ++i) {
    const auto [begin, size] = batches[i];
    const int end = begin + size;
    CHECK_GE(begin, 0);
    CHECK_LT(begin, kInputSize);

    CHECK_GT(size, 0);
    CHECK_LE(end, kInputSize);

    if (i + 1 == num_batches) continue;
    CHECK_EQ(end, batches[i + 1].first);
  }

  CHECK_EQ(cudaSuccess, cudaFreeHost(inputs));
  CHECK_EQ(cudaSuccess, cudaFreeHost(outputs));
}

}  // namespace ceres::internal

#endif  // CERES_NO_CUDA
