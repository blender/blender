/* SPDX-FileCopyrightText: 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_ONEAPI

#  include "device/memory.h"
#  include "device/queue.h"

#  include "kernel/device/oneapi/kernel.h"

#  include "util/unique_ptr.h"

CCL_NAMESPACE_BEGIN

class OneapiDevice;
class device_memory;

/* Base class for OneAPI queues. */
class OneapiDeviceQueue : public DeviceQueue {
 public:
  explicit OneapiDeviceQueue(OneapiDevice *device);

  int num_concurrent_states(const size_t state_size) const override;

  int num_concurrent_busy_states(const size_t state_size) const override;

  int num_sort_partitions(int max_num_paths, uint max_scene_shaders) const override;

  void init_execution() override;

  bool enqueue(DeviceKernel kernel,
               const int kernel_work_size,
               const DeviceKernelArguments &args) override;

  bool synchronize() override;

  void zero_to_device(device_memory &mem) override;
  void copy_to_device(device_memory &mem) override;
  void copy_from_device(device_memory &mem) override;

  bool supports_local_atomic_sort() const override
  {
    return true;
  }

#  ifdef SYCL_LINEAR_MEMORY_INTEROP_AVAILABLE
  unique_ptr<DeviceGraphicsInterop> graphics_interop_create() override;
#  endif

 protected:
  OneapiDevice *oneapi_device_;
  unique_ptr<KernelContext> kernel_context_;
};

CCL_NAMESPACE_END

#endif /* WITH_ONEAPI */
