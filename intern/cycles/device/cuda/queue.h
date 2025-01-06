/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_CUDA

#  include "device/memory.h"
#  include "device/queue.h"

#  include "device/cuda/util.h"

CCL_NAMESPACE_BEGIN

class CUDADevice;
class device_memory;

/* Base class for CUDA queues. */
class CUDADeviceQueue : public DeviceQueue {
 public:
  CUDADeviceQueue(CUDADevice *device);
  ~CUDADeviceQueue() override;

  int num_concurrent_states(const size_t state_size) const override;
  int num_concurrent_busy_states(const size_t state_size) const override;

  void init_execution() override;

  bool enqueue(DeviceKernel kernel,
               const int work_size,
               const DeviceKernelArguments &args) override;

  bool synchronize() override;

  void zero_to_device(device_memory &mem) override;
  void copy_to_device(device_memory &mem) override;
  void copy_from_device(device_memory &mem) override;

  virtual CUstream stream()
  {
    return cuda_stream_;
  }

  unique_ptr<DeviceGraphicsInterop> graphics_interop_create() override;

 protected:
  CUDADevice *cuda_device_;
  CUstream cuda_stream_;

  void assert_success(CUresult result, const char *operation);
};

CCL_NAMESPACE_END

#endif /* WITH_CUDA */
