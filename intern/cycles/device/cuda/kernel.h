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
