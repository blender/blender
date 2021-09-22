/*
 * Copyright 2011-2013 Blender Foundation
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

#ifdef WITH_CUDA

#  include "device/cuda/kernel.h"
#  include "device/cuda/device_impl.h"

CCL_NAMESPACE_BEGIN

void CUDADeviceKernels::load(CUDADevice *device)
{
  CUmodule cuModule = device->cuModule;

  for (int i = 0; i < (int)DEVICE_KERNEL_NUM; i++) {
    CUDADeviceKernel &kernel = kernels_[i];

    /* No mega-kernel used for GPU. */
    if (i == DEVICE_KERNEL_INTEGRATOR_MEGAKERNEL) {
      continue;
    }

    const std::string function_name = std::string("kernel_gpu_") +
                                      device_kernel_as_string((DeviceKernel)i);
    cuda_device_assert(device,
                       cuModuleGetFunction(&kernel.function, cuModule, function_name.c_str()));

    if (kernel.function) {
      cuda_device_assert(device, cuFuncSetCacheConfig(kernel.function, CU_FUNC_CACHE_PREFER_L1));

      cuda_device_assert(
          device,
          cuOccupancyMaxPotentialBlockSize(
              &kernel.min_blocks, &kernel.num_threads_per_block, kernel.function, NULL, 0, 0));
    }
    else {
      LOG(ERROR) << "Unable to load kernel " << function_name;
    }
  }

  loaded = true;
}

const CUDADeviceKernel &CUDADeviceKernels::get(DeviceKernel kernel) const
{
  return kernels_[(int)kernel];
}

bool CUDADeviceKernels::available(DeviceKernel kernel) const
{
  return kernels_[(int)kernel].function != nullptr;
}

CCL_NAMESPACE_END

#endif /* WITH_CUDA*/
