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

#ifdef WITH_HIP

#  include "device/hip/queue.h"

#  include "device/hip/device_impl.h"
#  include "device/hip/graphics_interop.h"
#  include "device/hip/kernel.h"

CCL_NAMESPACE_BEGIN

/* HIPDeviceQueue */

HIPDeviceQueue::HIPDeviceQueue(HIPDevice *device)
    : DeviceQueue(device), hip_device_(device), hip_stream_(nullptr)
{
  const HIPContextScope scope(hip_device_);
  hip_device_assert(hip_device_, hipStreamCreateWithFlags(&hip_stream_, hipStreamNonBlocking));
}

HIPDeviceQueue::~HIPDeviceQueue()
{
  const HIPContextScope scope(hip_device_);
  hipStreamDestroy(hip_stream_);
}

int HIPDeviceQueue::num_concurrent_states(const size_t /*state_size*/) const
{
  /* TODO: compute automatically. */
  /* TODO: must have at least num_threads_per_block. */
  return 14416128;
}

int HIPDeviceQueue::num_concurrent_busy_states() const
{
  const int max_num_threads = hip_device_->get_num_multiprocessors() *
                              hip_device_->get_max_num_threads_per_multiprocessor();

  if (max_num_threads == 0) {
    return 65536;
  }

  return 4 * max_num_threads;
}

void HIPDeviceQueue::init_execution()
{
  /* Synchronize all textures and memory copies before executing task. */
  HIPContextScope scope(hip_device_);
  hip_device_->load_texture_info();
  hip_device_assert(hip_device_, hipDeviceSynchronize());

  debug_init_execution();
}

bool HIPDeviceQueue::kernel_available(DeviceKernel kernel) const
{
  return hip_device_->kernels.available(kernel);
}

bool HIPDeviceQueue::enqueue(DeviceKernel kernel, const int work_size, void *args[])
{
  if (hip_device_->have_error()) {
    return false;
  }

  debug_enqueue(kernel, work_size);

  const HIPContextScope scope(hip_device_);
  const HIPDeviceKernel &hip_kernel = hip_device_->kernels.get(kernel);

  /* Compute kernel launch parameters. */
  const int num_threads_per_block = hip_kernel.num_threads_per_block;
  const int num_blocks = divide_up(work_size, num_threads_per_block);

  int shared_mem_bytes = 0;

  switch (kernel) {
    case DEVICE_KERNEL_INTEGRATOR_QUEUED_PATHS_ARRAY:
    case DEVICE_KERNEL_INTEGRATOR_QUEUED_SHADOW_PATHS_ARRAY:
    case DEVICE_KERNEL_INTEGRATOR_ACTIVE_PATHS_ARRAY:
    case DEVICE_KERNEL_INTEGRATOR_TERMINATED_PATHS_ARRAY:
    case DEVICE_KERNEL_INTEGRATOR_SORTED_PATHS_ARRAY:
    case DEVICE_KERNEL_INTEGRATOR_COMPACT_PATHS_ARRAY:
      /* See parall_active_index.h for why this amount of shared memory is needed. */
      shared_mem_bytes = (num_threads_per_block + 1) * sizeof(int);
      break;
    default:
      break;
  }

  /* Launch kernel. */
  hip_device_assert(hip_device_,
                    hipModuleLaunchKernel(hip_kernel.function,
                                          num_blocks,
                                          1,
                                          1,
                                          num_threads_per_block,
                                          1,
                                          1,
                                          shared_mem_bytes,
                                          hip_stream_,
                                          args,
                                          0));
  return !(hip_device_->have_error());
}

bool HIPDeviceQueue::synchronize()
{
  if (hip_device_->have_error()) {
    return false;
  }

  const HIPContextScope scope(hip_device_);
  hip_device_assert(hip_device_, hipStreamSynchronize(hip_stream_));
  debug_synchronize();

  return !(hip_device_->have_error());
}

void HIPDeviceQueue::zero_to_device(device_memory &mem)
{
  assert(mem.type != MEM_GLOBAL && mem.type != MEM_TEXTURE);

  if (mem.memory_size() == 0) {
    return;
  }

  /* Allocate on demand. */
  if (mem.device_pointer == 0) {
    hip_device_->mem_alloc(mem);
  }

  /* Zero memory on device. */
  assert(mem.device_pointer != 0);

  const HIPContextScope scope(hip_device_);
  hip_device_assert(
      hip_device_,
      hipMemsetD8Async((hipDeviceptr_t)mem.device_pointer, 0, mem.memory_size(), hip_stream_));
}

void HIPDeviceQueue::copy_to_device(device_memory &mem)
{
  assert(mem.type != MEM_GLOBAL && mem.type != MEM_TEXTURE);

  if (mem.memory_size() == 0) {
    return;
  }

  /* Allocate on demand. */
  if (mem.device_pointer == 0) {
    hip_device_->mem_alloc(mem);
  }

  assert(mem.device_pointer != 0);
  assert(mem.host_pointer != nullptr);

  /* Copy memory to device. */
  const HIPContextScope scope(hip_device_);
  hip_device_assert(
      hip_device_,
      hipMemcpyHtoDAsync(
          (hipDeviceptr_t)mem.device_pointer, mem.host_pointer, mem.memory_size(), hip_stream_));
}

void HIPDeviceQueue::copy_from_device(device_memory &mem)
{
  assert(mem.type != MEM_GLOBAL && mem.type != MEM_TEXTURE);

  if (mem.memory_size() == 0) {
    return;
  }

  assert(mem.device_pointer != 0);
  assert(mem.host_pointer != nullptr);

  /* Copy memory from device. */
  const HIPContextScope scope(hip_device_);
  hip_device_assert(
      hip_device_,
      hipMemcpyDtoHAsync(
          mem.host_pointer, (hipDeviceptr_t)mem.device_pointer, mem.memory_size(), hip_stream_));
}

// TODO : (Arya) Enable this after stabilizing dev branch
unique_ptr<DeviceGraphicsInterop> HIPDeviceQueue::graphics_interop_create()
{
  return make_unique<HIPDeviceGraphicsInterop>(this);
}

CCL_NAMESPACE_END

#endif /* WITH_HIP */
