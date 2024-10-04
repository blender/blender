/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_HIPRT

#  include "device/hiprt/queue.h"

#  include "device/hip/graphics_interop.h"
#  include "device/hip/kernel.h"
#  include "device/hiprt/device_impl.h"

CCL_NAMESPACE_BEGIN

HIPRTDeviceQueue::HIPRTDeviceQueue(HIPRTDevice *device)
    : HIPDeviceQueue((HIPDevice *)device), hiprt_device_(device)
{
}

bool HIPRTDeviceQueue::enqueue(DeviceKernel kernel,
                               const int work_size,
                               DeviceKernelArguments const &args)
{
  if (hiprt_device_->have_error()) {
    return false;
  }

  if (!device_kernel_has_intersection(kernel)) {
    return HIPDeviceQueue::enqueue(kernel, work_size, args);
  }

  debug_enqueue_begin(kernel, work_size);

  const HIPContextScope scope(hiprt_device_);
  const HIPDeviceKernel &hip_kernel = hiprt_device_->kernels.get(kernel);

  if (!hiprt_device_->global_stack_buffer.stackData) {
    uint32_t max_path = num_concurrent_states(0);
    hiprtGlobalStackBufferInput stack_buffer_input{
        hiprtStackTypeGlobal, hiprtStackEntryTypeInteger, HIPRT_THREAD_STACK_SIZE, max_path};

    hiprtError rt_result = hiprtCreateGlobalStackBuffer(hiprt_device_->get_hiprt_context(),
                                                        stack_buffer_input,
                                                        hiprt_device_->global_stack_buffer);

    if (rt_result != hiprtSuccess) {
      LOG(ERROR) << "Failed to create hiprt Global Stack Buffer";
      return false;
    }
  }

  DeviceKernelArguments args_copy = args;
  args_copy.add(DeviceKernelArguments::HIPRT_GLOBAL_STACK,
                (void *)(&hiprt_device_->global_stack_buffer),
                sizeof(hiprtGlobalStackBuffer));

  /* Compute kernel launch parameters. */
  const int num_threads_per_block = HIPRT_THREAD_GROUP_SIZE;
  const int num_blocks = divide_up(work_size, num_threads_per_block);
  int shared_mem_bytes = 0;

  assert_success(hipModuleLaunchKernel(hip_kernel.function,
                                       num_blocks,
                                       1,
                                       1,
                                       num_threads_per_block,
                                       1,
                                       1,
                                       shared_mem_bytes,
                                       hip_stream_,
                                       const_cast<void **>(args_copy.values),
                                       0),
                 "enqueue");

  debug_enqueue_end();

  return !(hiprt_device_->have_error());
}

CCL_NAMESPACE_END

#endif /* WITH_HIPRT */
