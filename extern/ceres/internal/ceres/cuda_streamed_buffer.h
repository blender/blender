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

#ifndef CERES_INTERNAL_CUDA_STREAMED_BUFFER_H_
#define CERES_INTERNAL_CUDA_STREAMED_BUFFER_H_

#include "ceres/internal/config.h"

#ifndef CERES_NO_CUDA
#include "ceres/cuda_buffer.h"

namespace ceres::internal {

// Most contemporary CUDA devices are capable of simultaneous code execution and
// host-to-device transfer. This class copies batches of data to GPU memory and
// executes processing of copied data in parallel (asynchronously).
// Data is copied to a fixed-size buffer on GPU (containing at most
// max_buffer_size values), and this memory is re-used when the previous
// batch of values is processed by user-provided callback
// Host-to-device copy uses a temporary buffer if required. Each batch of values
// has size of kValuesPerBatch, except the last one.
template <typename T>
class CERES_NO_EXPORT CudaStreamedBuffer {
 public:
  // If hardware supports only one host-to-device copy or one host-to-device
  // copy is able to reach peak bandwidth, two streams are sufficient to reach
  // maximum efficiency:
  //  - If transferring batch of values takes more time, than processing it on
  //  gpu, then at every moment of time one of the streams will be transferring
  //  data and other stream will be either processing data or idle; the whole
  //  process will be bounded by host-to-device copy.
  //  - If transferring batch of values takes less time, than processing it on
  //  gpu, then at every moment of time one of the streams will be processing
  //  data and other stream will be either performing computations or
  //  transferring data, and the whole process will be bounded by computations.
  static constexpr int kNumBatches = 2;
  // max_buffer_size is the maximal size (in elements of type T) of array
  // to be pre-allocated in gpu memory. The size of array determines size of
  // batch of values for simultaneous copying and processing. It should be large
  // enough to allow highly-parallel execution of user kernels; making it too
  // large increases latency.
  CudaStreamedBuffer(ContextImpl* context, const int max_buffer_size)
      : kValuesPerBatch(max_buffer_size / kNumBatches),
        context_(context),
        values_gpu_(context, kValuesPerBatch * kNumBatches) {
    static_assert(ContextImpl::kNumCudaStreams >= kNumBatches);
    CHECK_GE(max_buffer_size, kNumBatches);
    // Pre-allocate a buffer of page-locked memory for transfers from a regular
    // cpu memory. Because we will be only writing into that buffer from cpu,
    // memory is allocated with cudaHostAllocWriteCombined flag.
    CHECK_EQ(cudaSuccess,
             cudaHostAlloc(&values_cpu_pinned_,
                           sizeof(T) * kValuesPerBatch * kNumBatches,
                           cudaHostAllocWriteCombined));
    for (auto& e : copy_finished_) {
      CHECK_EQ(cudaSuccess,
               cudaEventCreateWithFlags(&e, cudaEventDisableTiming));
    }
  }

  CudaStreamedBuffer(const CudaStreamedBuffer&) = delete;

  ~CudaStreamedBuffer() {
    CHECK_EQ(cudaSuccess, cudaFreeHost(values_cpu_pinned_));
    for (auto& e : copy_finished_) {
      CHECK_EQ(cudaSuccess, cudaEventDestroy(e));
    }
  }

  // Transfer num_values at host-memory pointer from, calling
  // callback(device_pointer, size_of_batch, offset_of_batch, stream_to_use)
  // after scheduling transfer of each batch of data. User-provided callback
  // should perform processing of data at device_pointer only in
  // stream_to_use stream (device_pointer will be re-used in the next
  // callback invocation with the same stream).
  //
  // Two diagrams below describe operation in two possible scenarios, depending
  // on input data being stored in page-locked memory. In this example we will
  // have max_buffer_size = 2 * K, num_values = N * K and callback
  // scheduling a single asynchronous launch of
  // Kernel<<..., stream_to_use>>(device_pointer,
  //                              size_of_batch,
  //                              offset_of_batch)
  //
  // a. Copying from page-locked memory
  // In this case no copy on the host-side is necessary, and this method just
  // schedules a bunch of interleaved memory copies and callback invocations:
  //
  //  cudaStreamSynchronize(context->DefaultStream());
  //  - Iteration #0:
  //    - cudaMemcpyAsync(values_gpu_, from, K * sizeof(T), H->D, stream_0)
  //    - callback(values_gpu_, K, 0, stream_0)
  //  - Iteration #1:
  //    - cudaMemcpyAsync(values_gpu_ + K, from + K, K * sizeof(T), H->D,
  //    stream_1)
  //    - callback(values_gpu_ + K, K, K, stream_1)
  //  - Iteration #2:
  //    - cudaMemcpyAsync(values_gpu_, from + 2 * K, K * sizeof(T), H->D,
  //    stream_0)
  //    - callback(values_gpu_, K, 2 * K, stream_0)
  //  - Iteration #3:
  //     - cudaMemcpyAsync(values_gpu_ + K, from + 3 * K, K * sizeof(T), H->D,
  //     stream_1)
  //     - callback(values_gpu_ + K, K, 3 * K, stream_1)
  //  ...
  //  - Iteration #i:
  //     - cudaMemcpyAsync(values_gpu_ + (i % 2) * K, from + i * K, K *
  //     sizeof(T), H->D, stream_(i % 2))
  //     - callback(values_gpu_ + (i % 2) * K, K, i * K, stream_(i % 2)
  //  ...
  //  cudaStreamSynchronize(stream_0)
  //  cudaStreamSynchronize(stream_1)
  //
  //  This sequence of calls results in following activity on gpu (assuming that
  //  kernel invoked by callback takes less time than host-to-device copy):
  //  +-------------------+-------------------+
  //  | Stream #0         | Stream #1         |
  //  +-------------------+-------------------+
  //  | Copy host->device |                   |
  //  |                   |                   |
  //  |                   |                   |
  //  +-------------------+-------------------+
  //  | Kernel            | Copy host->device |
  //  +-------------------+                   |
  //  |                   |                   |
  //  +-------------------+-------------------+
  //  | Copy host->device | Kernel            |
  //  |                   +-------------------+
  //  |                   |                   |
  //  +-------------------+-------------------+
  //  | Kernel            | Copy host->device |
  //  |                  ...                  |
  //  +---------------------------------------+
  //
  // b. Copying from regular memory
  // In this case a copy from regular memory to page-locked memory is required
  // in order to get asynchrnonous operation. Because pinned memory on host-side
  // is reused, additional synchronization is required. On each iteration method
  // the following actions are performed:
  //  - Wait till previous copy operation in stream is completed
  //  - Copy batch of values from input array into pinned memory
  //  - Asynchronously launch host-to-device copy
  //  - Setup event for synchronization on copy completion
  //  - Invoke callback (that launches kernel asynchronously)
  //
  //  Invocations are performed with the following arguments
  //  cudaStreamSynchronize(context->DefaultStream());
  //  - Iteration #0:
  //    - cudaEventSynchronize(copy_finished_0)
  //    - std::copy_n(from, K, values_cpu_pinned_)
  //    - cudaMemcpyAsync(values_gpu_, values_cpu_pinned_, K * sizeof(T), H->D,
  //    stream_0)
  //    - cudaEventRecord(copy_finished_0, stream_0)
  //    - callback(values_gpu_, K, 0, stream_0)
  //  - Iteration #1:
  //    - cudaEventSynchronize(copy_finished_1)
  //    - std::copy_n(from + K, K, values_cpu_pinned_ + K)
  //    - cudaMemcpyAsync(values_gpu_ + K, values_cpu_pinned_ + K, K *
  //    sizeof(T), H->D, stream_1)
  //    - cudaEventRecord(copy_finished_1, stream_1)
  //    - callback(values_gpu_ + K, K, K, stream_1)
  //  - Iteration #2:
  //    - cudaEventSynchronize(copy_finished_0)
  //    - std::copy_n(from + 2 * K, K, values_cpu_pinned_)
  //    - cudaMemcpyAsync(values_gpu_, values_cpu_pinned_, K * sizeof(T), H->D,
  //    stream_0)
  //    - cudaEventRecord(copy_finished_0, stream_0)
  //    - callback(values_gpu_, K, 2 * K, stream_0)
  //  - Iteration #3:
  //    - cudaEventSynchronize(copy_finished_1)
  //    - std::copy_n(from + 3 * K, K, values_cpu_pinned_ + K)
  //    - cudaMemcpyAsync(values_gpu_ + K, values_cpu_pinned_ + K, K *
  //    sizeof(T), H->D, stream_1)
  //    - cudaEventRecord(copy_finished_1, stream_1)
  //    - callback(values_gpu_ + K, K, 3 * K, stream_1)
  //  ...
  //  - Iteration #i:
  //    - cudaEventSynchronize(copy_finished_(i % 2))
  //    - std::copy_n(from + i * K, K, values_cpu_pinned_ + (i % 2) * K)
  //    - cudaMemcpyAsync(values_gpu_ + (i % 2) * K, values_cpu_pinned_ + (i %
  //    2) * K, K * sizeof(T), H->D, stream_(i % 2))
  //    - cudaEventRecord(copy_finished_(i % 2), stream_(i % 2))
  //    - callback(values_gpu_ + (i % 2) * K, K, i * K, stream_(i % 2))
  //  ...
  //  cudaStreamSynchronize(stream_0)
  //  cudaStreamSynchronize(stream_1)
  //
  //  This sequence of calls results in following activity on cpu and gpu
  //  (assuming that kernel invoked by callback takes less time than
  //  host-to-device copy and copy in cpu memory, and copy in cpu memory is
  //  faster than host-to-device copy):
  //  +----------------------------+-------------------+-------------------+
  //  | Stream #0                  | Stream #0         | Stream #1         |
  //  +----------------------------+-------------------+-------------------+
  //  | Copy to pinned memory      |                   |                   |
  //  |                            |                   |                   |
  //  +----------------------------+-------------------|                   |
  //  | Copy to pinned memory      | Copy host->device |                   |
  //  |                            |                   |                   |
  //  +----------------------------+                   |                   |
  //  | Waiting previous h->d copy |                   |                   |
  //  +----------------------------+-------------------+-------------------+
  //  | Copy to pinned memory      | Kernel            | Copy host->device |
  //  |                            +-------------------+                   |
  //  +----------------------------+                   |                   |
  //  | Waiting previous h->d copy |                   |                   |
  //  +----------------------------+-------------------+-------------------+
  //  | Copy to pinned memory      | Copy host->device | Kernel            |
  //  |                            |                   +-------------------+
  //  |                           ...                 ...                  |
  //  +----------------------------+---------------------------------------+
  //
  template <typename Fun>
  void CopyToGpu(const T* from, const int num_values, Fun&& callback) {
    // This synchronization is not required in some cases, but we perform it in
    // order to avoid situation when user callback depends on data that is
    // still to be computed in default stream
    CHECK_EQ(cudaSuccess, cudaStreamSynchronize(context_->DefaultStream()));

    // If pointer to input data does not correspond to page-locked memory,
    // host-to-device memory copy might be executed synchrnonously (with a copy
    // to pinned memory happening inside the driver). In that case we perform
    // copy to a pre-allocated array of page-locked memory.
    const bool copy_to_pinned_memory = MemoryTypeResultsInSynchronousCopy(from);
    T* batch_values_gpu[kNumBatches];
    T* batch_values_cpu[kNumBatches];
    auto streams = context_->streams_;
    for (int i = 0; i < kNumBatches; ++i) {
      batch_values_gpu[i] = values_gpu_.data() + kValuesPerBatch * i;
      batch_values_cpu[i] = values_cpu_pinned_ + kValuesPerBatch * i;
    }
    int batch_id = 0;
    for (int offset = 0; offset < num_values; offset += kValuesPerBatch) {
      const int num_values_batch =
          std::min(num_values - offset, kValuesPerBatch);
      const T* batch_from = from + offset;
      T* batch_to = batch_values_gpu[batch_id];
      auto stream = streams[batch_id];
      auto copy_finished = copy_finished_[batch_id];

      if (copy_to_pinned_memory) {
        // Copying values to a temporary buffer should be started only after the
        // previous copy from temporary buffer to device is completed.
        CHECK_EQ(cudaSuccess, cudaEventSynchronize(copy_finished));
        std::copy_n(batch_from, num_values_batch, batch_values_cpu[batch_id]);
        batch_from = batch_values_cpu[batch_id];
      }
      CHECK_EQ(cudaSuccess,
               cudaMemcpyAsync(batch_to,
                               batch_from,
                               sizeof(T) * num_values_batch,
                               cudaMemcpyHostToDevice,
                               stream));
      if (copy_to_pinned_memory) {
        // Next copy to a temporary buffer can start straight after asynchronous
        // copy is completed (and might be started before kernels asynchronously
        // executed in stream by user-supplied callback are completed).
        // No explicit synchronization is required when copying data from
        // page-locked memory, because memory copy and user kernel execution
        // with corresponding part of values_gpu_ array is serialized using
        // stream
        CHECK_EQ(cudaSuccess, cudaEventRecord(copy_finished, stream));
      }
      callback(batch_to, num_values_batch, offset, stream);
      batch_id = (batch_id + 1) % kNumBatches;
    }
    // Explicitly synchronize on all CUDA streams that were utilized.
    for (int i = 0; i < kNumBatches; ++i) {
      CHECK_EQ(cudaSuccess, cudaStreamSynchronize(streams[i]));
    }
  }

 private:
  // It is necessary to have all host-to-device copies to be completely
  // asynchronous. This requires source memory to be allocated in page-locked
  // memory.
  static bool MemoryTypeResultsInSynchronousCopy(const void* ptr) {
    cudaPointerAttributes attributes;
    auto status = cudaPointerGetAttributes(&attributes, ptr);
#if CUDART_VERSION < 11000
    // In CUDA versions prior 11 call to cudaPointerGetAttributes with host
    // pointer will return  cudaErrorInvalidValue
    if (status == cudaErrorInvalidValue) {
      return true;
    }
#endif
    CHECK_EQ(status, cudaSuccess);
    // This class only supports cpu memory as a source
    CHECK_NE(attributes.type, cudaMemoryTypeDevice);
    // If host memory was allocated (or registered) with CUDA API, or is a
    // managed memory, then call to cudaMemcpyAsync will be asynchrnous. In case
    // of managed memory it might be slightly better to perform a single call of
    // user-provided call-back (and hope that page migration will provide a
    // similar throughput with zero efforts from our side).
    return attributes.type == cudaMemoryTypeUnregistered;
  }

  const int kValuesPerBatch;
  ContextImpl* context_ = nullptr;
  CudaBuffer<T> values_gpu_;
  T* values_cpu_pinned_ = nullptr;
  cudaEvent_t copy_finished_[kNumBatches] = {nullptr};
};

}  // namespace ceres::internal

#endif  // CERES_NO_CUDA
#endif  // CERES_INTERNAL_CUDA_STREAMED_BUFFER_H_
