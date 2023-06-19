/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

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
  virtual int num_concurrent_busy_states(const size_t state_size) const override;

  virtual void init_execution() override;

  virtual bool enqueue(DeviceKernel kernel,
                       const int work_size,
                       DeviceKernelArguments const &args) override;

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
