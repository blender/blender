/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

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
