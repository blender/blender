/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_CUDA

#  include "device/kernel.h"

#  ifdef WITH_CUDA_DYNLOAD
#    include "cuew.h"
#  else
#    include <cuda.h>
#  endif

CCL_NAMESPACE_BEGIN

class CUDADevice;

/* CUDA kernel and associate occupancy information. */
class CUDADeviceKernel {
 public:
  CUfunction function = nullptr;

  int num_threads_per_block = 0;
  int min_blocks = 0;
};

/* Cache of CUDA kernels for each DeviceKernel. */
class CUDADeviceKernels {
 public:
  void load(CUDADevice *device);
  const CUDADeviceKernel &get(DeviceKernel kernel) const;
  bool available(DeviceKernel kernel) const;

 protected:
  CUDADeviceKernel kernels_[DEVICE_KERNEL_NUM];
  bool loaded = false;
};

CCL_NAMESPACE_END

#endif /* WITH_CUDA */
