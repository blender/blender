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

#  ifdef WITH_HIP_DYNLOAD
#    include "hipew.h"
#  endif

CCL_NAMESPACE_BEGIN

class HIPDevice;

/* HIP kernel and associate occupancy information. */
class HIPDeviceKernel {
 public:
  hipFunction_t function = nullptr;

  int num_threads_per_block = 0;
  int min_blocks = 0;
};

/* Cache of HIP kernels for each DeviceKernel. */
class HIPDeviceKernels {
 public:
  void load(HIPDevice *device);
  const HIPDeviceKernel &get(DeviceKernel kernel) const;
  bool available(DeviceKernel kernel) const;

 protected:
  HIPDeviceKernel kernels_[DEVICE_KERNEL_NUM];
  bool loaded = false;
};

CCL_NAMESPACE_END

#endif /* WITH_HIP */
