/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#ifdef WITH_HIPRT

#  include "device/kernel.h"
#  include "device/memory.h"
#  include "device/queue.h"

#  include "device/hip/queue.h"
#  include "device/hip/util.h"

CCL_NAMESPACE_BEGIN

class HIPRTDevice;

class HIPRTDeviceQueue : public HIPDeviceQueue {
 public:
  HIPRTDeviceQueue(HIPRTDevice *device);
  ~HIPRTDeviceQueue() {}
  virtual bool enqueue(DeviceKernel kernel,
                       const int work_size,
                       DeviceKernelArguments const &args) override;

 protected:
  HIPRTDevice *hiprt_device_;
};

CCL_NAMESPACE_END

#endif /* WITH_HIPRT */
