/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_CUDA

#  include "device/cuda/queue.h"

#  include "device/cuda/device_impl.h"
#  include "device/cuda/graphics_interop.h"
#  include "device/cuda/kernel.h"

#  include "kernel/device/gpu/block_sizes.h"
#  include "util/debug.h"

CCL_NAMESPACE_BEGIN

/* CUDADeviceQueue */

CUDADeviceQueue::CUDADeviceQueue(CUDADevice *device)
    : DeviceQueue(device), cuda_device_(device), cuda_stream_(nullptr)
{
  const CUDAContextScope scope(cuda_device_);
  cuda_device_assert(cuda_device_, cuStreamCreate(&cuda_stream_, CU_STREAM_NON_BLOCKING));
}

CUDADeviceQueue::~CUDADeviceQueue()
{
  const CUDAContextScope scope(cuda_device_);
  cuStreamDestroy(cuda_stream_);
}

ConcurrentStatesParams CUDADeviceQueue::concurrent_states_params() const
{
  const int max_num_threads = cuda_device_->get_num_multiprocessors() *
                              cuda_device_->get_max_num_threads_per_multiprocessor();

  /* Benefit stops being measurable at around 10x the baseline, but we are a bit
   * more conservative and only grow up to 4x and shrink up to 2x. */
  ConcurrentStatesParams params;
  params.baseline = max(max_num_threads, 65536) * 16;
  params.min = params.baseline / 2;
  params.max = params.baseline * 4;
  return params;
}

void CUDADeviceQueue::get_memory_info(size_t &total, size_t &free) const
{
  cuda_device_->get_device_memory_info(total, free);
}

int CUDADeviceQueue::num_sort_partitions(int max_num_paths, uint max_scene_shaders) const
{
  /* Sort partitioning becomes less effective when more shaders are in the wavefront. Also need to
   * ensure the amount of shared memory required is within the limit. */
  if (max_scene_shaders < min(300, cuda_device_->max_shared_mem_bytes / (int)sizeof(int))) {
    return max(max_num_paths / 65536, 1);
  }
  else {
    return 1;
  }
}

bool CUDADeviceQueue::supports_local_atomic_sort() const
{
  return DebugFlags().cuda.use_local_atomic_sort;
}

void CUDADeviceQueue::init_execution()
{
  /* Synchronize all textures and memory copies before executing task.
   * Use default stream (nullptr) since that's what we will synchronize
   * here to ensure all scene data is copied. */
  CUDAContextScope scope(cuda_device_);
  cuda_device_->load_image_info(nullptr);
  cuda_device_assert(cuda_device_, cuCtxSynchronize());

  debug_init_execution();
}

void CUDADeviceQueue::load_image_info()
{
  CUDAContextScope scope(cuda_device_);
  cuda_device_->load_image_info(this);
}

bool CUDADeviceQueue::enqueue(DeviceKernel kernel,
                              const int work_size,
                              const DeviceKernelArguments &args)
{
  if (cuda_device_->have_error()) {
    return false;
  }

  debug_enqueue_begin(kernel, work_size);

  const CUDAContextScope scope(cuda_device_);

  /* Update image info in case integrator memory alloc caused texture to move to host. */
  if (cuda_device_->load_image_info(nullptr)) {
    cuda_device_assert(cuda_device_, cuCtxSynchronize());
    if (cuda_device_->have_error()) {
      return false;
    }
  }

  /* Compute kernel launch parameters. */
  const CUDADeviceKernel &cuda_kernel = cuda_device_->kernels.get(kernel);

  int num_threads_per_block = cuda_kernel.num_threads_per_block;
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

    case DEVICE_KERNEL_INTEGRATOR_SORT_BUCKET_PASS:
    case DEVICE_KERNEL_INTEGRATOR_SORT_WRITE_PASS:
      num_threads_per_block = GPU_PARALLEL_SORT_BLOCK_SIZE;
      shared_mem_bytes = cuda_device_->scene_max_shaders_ * sizeof(int);
      break;

    default:
      break;
  }

  const int num_blocks = divide_up(work_size, num_threads_per_block);

  /* Launch kernel. */
  assert_success(cuLaunchKernel(cuda_kernel.function,
                                num_blocks,
                                1,
                                1,
                                num_threads_per_block,
                                1,
                                1,
                                shared_mem_bytes,
                                cuda_stream_,
                                const_cast<void **>(args.values),
                                nullptr),
                 "enqueue");

  debug_enqueue_end();

  return !(cuda_device_->have_error());
}

bool CUDADeviceQueue::synchronize()
{
  if (cuda_device_->have_error()) {
    return false;
  }

  const CUDAContextScope scope(cuda_device_);
  assert_success(cuStreamSynchronize(cuda_stream_), "synchronize");

  debug_synchronize();

  return !(cuda_device_->have_error());
}

void CUDADeviceQueue::zero_to_device(device_memory &mem)
{
  assert(mem.type != MEM_IMAGE_TEXTURE);

  if (mem.memory_size() == 0) {
    return;
  }

  /* Allocate on demand. */
  if (mem.device_pointer == 0) {
    if (mem.type == MEM_GLOBAL) {
      cuda_device_->global_alloc(mem);
    }
    else {
      cuda_device_->mem_alloc(mem);
    }
  }

  /* Zero memory on device. */
  device_ptr d_ptr = mem.device->mem_device_ptr(mem, cuda_device_);
  assert(d_ptr != 0);

  const CUDAContextScope scope(cuda_device_);
  assert_success(cuMemsetD8Async((CUdeviceptr)d_ptr, 0, mem.memory_size(), cuda_stream_),
                 "zero_to_device");
}

void CUDADeviceQueue::copy_to_device(device_memory &mem)
{
  assert(mem.type != MEM_IMAGE_TEXTURE);

  if (mem.memory_size() == 0) {
    return;
  }

  /* Allocate on demand. */
  if (mem.device_pointer == 0) {
    if (mem.type == MEM_GLOBAL) {
      cuda_device_->global_alloc(mem);
    }
    else {
      cuda_device_->mem_alloc(mem);
    }
  }

  device_ptr d_ptr = mem.device->mem_device_ptr(mem, cuda_device_);
  assert(d_ptr != 0);
  assert(mem.host_pointer != nullptr);

  /* Copy memory to device. */
  const CUDAContextScope scope(cuda_device_);
  assert_success(
      cuMemcpyHtoDAsync((CUdeviceptr)d_ptr, mem.host_pointer, mem.memory_size(), cuda_stream_),
      "copy_to_device");
}

void CUDADeviceQueue::copy_from_device(device_memory &mem)
{
  assert(mem.type != MEM_GLOBAL && mem.type != MEM_IMAGE_TEXTURE);

  if (mem.memory_size() == 0) {
    return;
  }

  assert(mem.device_pointer != 0);
  assert(mem.host_pointer != nullptr);

  /* Copy memory from device. */
  const CUDAContextScope scope(cuda_device_);
  assert_success(
      cuMemcpyDtoHAsync(
          mem.host_pointer, (CUdeviceptr)mem.device_pointer, mem.memory_size(), cuda_stream_),
      "copy_from_device");
}

void *CUDADeviceQueue::copy_from_device_synchronized(device_memory &mem, vector<uint8_t> &storage)
{
  if (mem.memory_size() == 0) {
    return nullptr;
  }

  storage.resize(mem.memory_size());

  device_ptr d_ptr = mem.device->mem_device_ptr(mem, cuda_device_);
  assert(d_ptr != 0);

  const CUDAContextScope scope(cuda_device_);
  assert_success(
      cuMemcpyDtoHAsync(storage.data(), (CUdeviceptr)d_ptr, mem.memory_size(), cuda_stream_),
      "copy_from_device_synchronized");

  synchronize();
  return storage.data();
}

void CUDADeviceQueue::assert_success(CUresult result, const char *operation)
{
  if (result != CUDA_SUCCESS) {
    const char *name = cuewErrorString(result);
    cuda_device_->set_error(string_printf(
        "%s in CUDA queue %s (%s)", name, operation, debug_active_kernels().c_str()));
  }
}

unique_ptr<DeviceGraphicsInterop> CUDADeviceQueue::graphics_interop_create()
{
  return make_unique<CUDADeviceGraphicsInterop>(this);
}

CCL_NAMESPACE_END

#endif /* WITH_CUDA */
