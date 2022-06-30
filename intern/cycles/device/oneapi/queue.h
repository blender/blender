/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Intel Corporation */

#pragma once

#ifdef WITH_ONEAPI

#  include "device/kernel.h"
#  include "device/memory.h"
#  include "device/queue.h"

#  include "device/oneapi/device.h"
#  include "device/oneapi/dll_interface.h"

CCL_NAMESPACE_BEGIN

class OneapiDevice;
class device_memory;

/* Base class for OneAPI queues. */
class OneapiDeviceQueue : public DeviceQueue {
 public:
  explicit OneapiDeviceQueue(OneapiDevice *device);
  ~OneapiDeviceQueue();

  virtual int num_concurrent_states(const size_t state_size) const override;

  virtual int num_concurrent_busy_states() const override;

  virtual void init_execution() override;

  virtual bool enqueue(DeviceKernel kernel,
                       const int kernel_work_size,
                       DeviceKernelArguments const &args) override;

  virtual bool synchronize() override;

  virtual void zero_to_device(device_memory &mem) override;
  virtual void copy_to_device(device_memory &mem) override;
  virtual void copy_from_device(device_memory &mem) override;

 protected:
  OneapiDevice *oneapi_device_;
  OneAPIDLLInterface oneapi_dll_;
  KernelContext *kernel_context_;
  bool with_kernel_statistics_;
};

CCL_NAMESPACE_END

#endif /* WITH_ONEAPI */
