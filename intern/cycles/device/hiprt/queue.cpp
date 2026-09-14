/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_HIPRT

#  include "device/hiprt/queue.h"

#  include <hiprt/hiprt.h>

#  include "device/hip/graphics_interop.h"
#  include "device/hip/kernel.h"
#  include "device/hiprt/device_impl.h"

#  include "kernel/device/hiprt/globals.h"

CCL_NAMESPACE_BEGIN

HIPRTDeviceQueue::HIPRTDeviceQueue(HIPRTDevice *device)
    : HIPDeviceQueue((HIPDevice *)device), hiprt_device_(device)
{
}

int HIPRTDeviceQueue::num_concurrent_states(const size_t state_size) const
{
  /* Add global traversal stack of each path. */
  return HIPDeviceQueue::num_concurrent_states(state_size + HIPRT_THREAD_STACK_SIZE * sizeof(int));
}

bool HIPRTDeviceQueue::enqueue(DeviceKernel kernel,
                               const int work_size,
                               const DeviceKernelArguments &args)
{
  if (hiprt_device_->have_error()) {
    return false;
  }

  if (!device_kernel_has_intersection(kernel)) {
    return HIPDeviceQueue::enqueue(kernel, work_size, args);
  }

  const HIPContextScope scope(hiprt_device_);
  const HIPDeviceKernel &hip_kernel = hiprt_device_->kernels.get(kernel);

  /* Compute kernel launch parameters. */
  const int num_threads_per_block = HIPRT_THREAD_GROUP_SIZE;
  const int num_blocks = divide_up(work_size, num_threads_per_block);

  /* Allocate and grow stack buffer just in time, its size is only known here. */
  const int num_stacks = num_blocks * num_threads_per_block;
  hiprtGlobalStackBuffer &stack_buffer = hiprt_device_->global_stack_buffer;
  if (!stack_buffer.stackData || stack_buffer.stackCount < num_stacks) {
    const hiprtContext hiprt_context = hiprt_device_->get_hiprt_context();

    if (stack_buffer.stackData) {
      /* Wait for kernels still using the current buffer. */
      if (!synchronize()) {
        return false;
      }
      hiprtDestroyGlobalStackBuffer(hiprt_context, stack_buffer);
      stack_buffer = {0};
    }

    hiprtGlobalStackBufferInput stack_buffer_input{hiprtStackTypeGlobal,
                                                   hiprtStackEntryTypeInteger,
                                                   uint32_t(HIPRT_THREAD_STACK_SIZE),
                                                   uint32_t(num_stacks)};

    hiprtError rt_result = hiprtCreateGlobalStackBuffer(
        hiprt_context, stack_buffer_input, stack_buffer);

    if (rt_result != hiprtSuccess) {
      LOG_ERROR << "Failed to create hiprt Global Stack Buffer";
      return false;
    }
  }

  debug_enqueue_begin(kernel, work_size);

  DeviceKernelArguments args_copy = args;
  args_copy.add(DeviceKernelArguments::HIPRT_GLOBAL_STACK,
                (void *)(&stack_buffer),
                sizeof(hiprtGlobalStackBuffer));

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
                                       nullptr),
                 "enqueue");

  debug_enqueue_end();

  return !(hiprt_device_->have_error());
}

CCL_NAMESPACE_END

#endif /* WITH_HIPRT */
