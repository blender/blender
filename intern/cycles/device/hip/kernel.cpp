/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_HIP
#  include "kernel/types.h"

#  include "device/hip/device_impl.h"
#  include "device/hip/kernel.h"

CCL_NAMESPACE_BEGIN

bool HIPDeviceKernels::load_kernel(HIPDevice *device,
                                   hipModule_t hip_module,
                                   const DeviceKernel kernel)
{
  if (available(kernel)) {
    return true;
  }

  /* No mega-kernel used for GPU. */
  if (kernel == DEVICE_KERNEL_INTEGRATOR_MEGAKERNEL) {
    return false;
  }

  HIPDeviceKernel &hip_kernel = kernels_[int(kernel)];

  const std::string function_name = std::string("kernel_gpu_") + device_kernel_as_string(kernel);
  hip_device_assert(device,
                    hipModuleGetFunction(&hip_kernel.function, hip_module, function_name.c_str()));
  if (!hip_kernel.function) {
    LOG_ERROR << "Unable to load kernel " << function_name;
    return false;
  }

  hip_device_assert(device, hipFuncSetCacheConfig(hip_kernel.function, hipFuncCachePreferL1));
  hip_device_assert(
      device,
      hipModuleOccupancyMaxPotentialBlockSize(
          &hip_kernel.min_blocks, &hip_kernel.num_threads_per_block, hip_kernel.function, 0, 0));

  LOG_DEBUG << "Loaded kernel: " << function_name;

  return true;
}

void HIPDeviceKernels::load_all(HIPDevice *device, hipModule_t hip_module)
{
  LOG_DEBUG << "Loading all HIP kernels";

  for (int i = 0; i < (int)DEVICE_KERNEL_NUM; i++) {
    load_kernel(device, hip_module, DeviceKernel(i));
  }
}

void HIPDeviceKernels::load_raytrace(HIPDevice *device, hipModule_t hip_module)
{
  LOG_DEBUG << "Loading ray-tracing HIP-RT kernels";

  load_kernel(device, hip_module, DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST);
  load_kernel(device, hip_module, DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW);
  load_kernel(device, hip_module, DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE);
  load_kernel(device, hip_module, DEVICE_KERNEL_INTEGRATOR_INTERSECT_VOLUME_STACK);
  load_kernel(device, hip_module, DEVICE_KERNEL_INTEGRATOR_INTERSECT_DEDICATED_LIGHT);

  load_kernel(device, hip_module, DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE);
  load_kernel(device, hip_module, DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_MNEE);
}

const HIPDeviceKernel &HIPDeviceKernels::get(DeviceKernel kernel) const
{
  return kernels_[(int)kernel];
}

bool HIPDeviceKernels::available(DeviceKernel kernel) const
{
  return kernels_[(int)kernel].function != nullptr;
}

CCL_NAMESPACE_END

#endif /* WITH_HIP */
