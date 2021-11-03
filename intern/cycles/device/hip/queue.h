/*
 * Copyright 2011-2021 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#ifdef WITH_HIP

#  include "device/kernel.h"
#  include "device/memory.h"
#  include "device/queue.h"

#  include "device/hip/util.h"

CCL_NAMESPACE_BEGIN

class HIPDevice;
class device_memory;

/* Base class for HIP queues. */
class HIPDeviceQueue : public DeviceQueue {
 public:
  HIPDeviceQueue(HIPDevice *device);
  ~HIPDeviceQueue();

  virtual int num_concurrent_states(const size_t state_size) const override;
  virtual int num_concurrent_busy_states() const override;

  virtual void init_execution() override;

  virtual bool kernel_available(DeviceKernel kernel) const override;

  virtual bool enqueue(DeviceKernel kernel, const int work_size, void *args[]) override;

  virtual bool synchronize() override;

  virtual void zero_to_device(device_memory &mem) override;
  virtual void copy_to_device(device_memory &mem) override;
  virtual void copy_from_device(device_memory &mem) override;

  virtual hipStream_t stream()
  {
    return hip_stream_;
  }

  virtual unique_ptr<DeviceGraphicsInterop> graphics_interop_create() override;

 protected:
  HIPDevice *hip_device_;
  hipStream_t hip_stream_;

  void assert_success(hipError_t result, const char *operation);
};

CCL_NAMESPACE_END

#endif /* WITH_HIP */
