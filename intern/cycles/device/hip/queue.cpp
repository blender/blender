/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

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

int HIPDeviceQueue::num_concurrent_states(const size_t state_size) const
{
  const int max_num_threads = hip_device_->get_num_multiprocessors() *
                              hip_device_->get_max_num_threads_per_multiprocessor();
  int num_states = ((max_num_threads == 0) ? 65536 : max_num_threads) * 16;

  const char *factor_str = getenv("CYCLES_CONCURRENT_STATES_FACTOR");
  if (factor_str) {
    const float factor = (float)atof(factor_str);
    if (factor != 0.0f) {
      num_states = max((int)(num_states * factor), 1024);
    }
    else {
      VLOG_DEVICE_STATS << "CYCLES_CONCURRENT_STATES_FACTOR evaluated to 0";
    }
  }

  VLOG_DEVICE_STATS << "GPU queue concurrent states: " << num_states << ", using up to "
                    << string_human_readable_size(num_states * state_size);

  return num_states;
}

int HIPDeviceQueue::num_concurrent_busy_states(const size_t /*state_size*/) const
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

bool HIPDeviceQueue::enqueue(DeviceKernel kernel,
                             const int work_size,
                             DeviceKernelArguments const &args)
{
  if (hip_device_->have_error()) {
    return false;
  }

  debug_enqueue_begin(kernel, work_size);

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
    case DEVICE_KERNEL_INTEGRATOR_TERMINATED_SHADOW_PATHS_ARRAY:
    case DEVICE_KERNEL_INTEGRATOR_COMPACT_SHADOW_PATHS_ARRAY:
      /* See parall_active_index.h for why this amount of shared memory is needed. */
      shared_mem_bytes = (num_threads_per_block + 1) * sizeof(int);
      break;
    default:
      break;
  }

  /* Launch kernel. */
  assert_success(hipModuleLaunchKernel(hip_kernel.function,
                                       num_blocks,
                                       1,
                                       1,
                                       num_threads_per_block,
                                       1,
                                       1,
                                       shared_mem_bytes,
                                       hip_stream_,
                                       const_cast<void **>(args.values),
                                       0),
                 "enqueue");

  debug_enqueue_end();

  return !(hip_device_->have_error());
}

bool HIPDeviceQueue::synchronize()
{
  if (hip_device_->have_error()) {
    return false;
  }

  const HIPContextScope scope(hip_device_);
  assert_success(hipStreamSynchronize(hip_stream_), "synchronize");
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
  assert_success(
      hipMemsetD8Async((hipDeviceptr_t)mem.device_pointer, 0, mem.memory_size(), hip_stream_),
      "zero_to_device");
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
  assert_success(
      hipMemcpyHtoDAsync(
          (hipDeviceptr_t)mem.device_pointer, mem.host_pointer, mem.memory_size(), hip_stream_),
      "copy_to_device");
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
  assert_success(
      hipMemcpyDtoHAsync(
          mem.host_pointer, (hipDeviceptr_t)mem.device_pointer, mem.memory_size(), hip_stream_),
      "copy_from_device");
}

void HIPDeviceQueue::assert_success(hipError_t result, const char *operation)
{
  if (result != hipSuccess) {
    const char *name = hipewErrorString(result);
    hip_device_->set_error(
        string_printf("%s in HIP queue %s (%s)", name, operation, debug_active_kernels().c_str()));
  }
}

unique_ptr<DeviceGraphicsInterop> HIPDeviceQueue::graphics_interop_create()
{
  return make_unique<HIPDeviceGraphicsInterop>(this);
}

CCL_NAMESPACE_END

#endif /* WITH_HIP */
