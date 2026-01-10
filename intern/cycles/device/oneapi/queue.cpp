/* SPDX-FileCopyrightText: 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_ONEAPI

#  include "device/oneapi/queue.h"
#  include "device/oneapi/device_impl.h"
#  include "device/oneapi/graphics_interop.h"
#  include "util/log.h"

#  include "kernel/device/oneapi/kernel.h"

CCL_NAMESPACE_BEGIN

struct KernelExecutionInfo {
  double elapsed_summary = 0.0;
  int enqueue_count = 0;
};

/* OneapiDeviceQueue */

OneapiDeviceQueue::OneapiDeviceQueue(OneapiDevice *device)
    : DeviceQueue(device), oneapi_device_(device)
{
}

int OneapiDeviceQueue::num_concurrent_states(const size_t state_size) const
{
  int num_states = 4 * num_concurrent_busy_states(state_size);

  LOG_TRACE << "GPU queue concurrent states: " << num_states << ", using up to "
            << string_human_readable_size(num_states * state_size);

  return num_states;
}

int OneapiDeviceQueue::num_concurrent_busy_states(const size_t /*state_size*/) const
{
  const int max_num_threads = oneapi_device_->get_num_multiprocessors() *
                              oneapi_device_->get_max_num_threads_per_multiprocessor();

  return 4 * max(8 * max_num_threads, 65536);
}

int OneapiDeviceQueue::num_sort_partitions(int max_num_paths, uint /*max_scene_shaders*/) const
{
  int sort_partition_elements = (oneapi_device_->get_max_num_threads_per_multiprocessor() >= 128) ?
                                    65536 :
                                    8192;
  /* Sort partitioning with local sorting on Intel GPUs is currently the most effective solution no
   * matter the number of shaders. */
  return max(max_num_paths / sort_partition_elements, 1);
}

void OneapiDeviceQueue::init_execution()
{
  oneapi_device_->load_image_info();

  SyclQueue *device_queue = oneapi_device_->sycl_queue();
  void *kg_dptr = oneapi_device_->kernel_globals_device_pointer();
  assert(device_queue);
  assert(kg_dptr);
  kernel_context_ = make_unique<KernelContext>();
  kernel_context_->queue = device_queue;
  kernel_context_->kernel_globals = kg_dptr;

  debug_init_execution();
}

bool OneapiDeviceQueue::enqueue(DeviceKernel kernel,
                                const int signed_kernel_work_size,
                                const DeviceKernelArguments &_args)
{
  if (oneapi_device_->have_error()) {
    return false;
  }

  /* Update image info in case memory moved to host. */
  if (oneapi_device_->load_image_info()) {
    if (!synchronize()) {
      return false;
    }
  }

  void **args = const_cast<void **>(_args.values);

  debug_enqueue_begin(kernel, signed_kernel_work_size);
  assert(signed_kernel_work_size >= 0);
  size_t kernel_global_size = (size_t)signed_kernel_work_size;
  size_t kernel_local_size;

  assert(kernel_context_);
  kernel_context_->scene_max_shaders = oneapi_device_->scene_max_shaders();

  oneapi_device_->get_adjusted_global_and_local_sizes(
      kernel_context_->queue, kernel, kernel_global_size, kernel_local_size);

  /* Call the oneAPI kernel DLL to launch the requested kernel. */
  bool is_finished_ok = oneapi_device_->enqueue_kernel(
      kernel_context_.get(), kernel, kernel_global_size, kernel_local_size, args);

  if (is_finished_ok == false) {
    oneapi_device_->set_error("oneAPI kernel \"" + std::string(device_kernel_as_string(kernel)) +
                              "\" execution error: got runtime exception \"" +
                              oneapi_device_->oneapi_error_message() + "\"");
  }

  debug_enqueue_end();

  return is_finished_ok;
}

bool OneapiDeviceQueue::synchronize()
{
  if (oneapi_device_->have_error()) {
    return false;
  }

  bool is_finished_ok = oneapi_device_->queue_synchronize(oneapi_device_->sycl_queue());
  if (is_finished_ok == false) {
    oneapi_device_->set_error("oneAPI unknown kernel execution error: got runtime exception \"" +
                              oneapi_device_->oneapi_error_message() + "\"");
  }

  debug_synchronize();

  return !(oneapi_device_->have_error());
}

void OneapiDeviceQueue::zero_to_device(device_memory &mem)
{
  oneapi_device_->mem_zero(mem);
}

void OneapiDeviceQueue::copy_to_device(device_memory &mem)
{
  oneapi_device_->mem_copy_to(mem);
}

void OneapiDeviceQueue::copy_from_device(device_memory &mem)
{
  oneapi_device_->mem_copy_from(mem);
}

#  ifdef SYCL_LINEAR_MEMORY_INTEROP_AVAILABLE
unique_ptr<DeviceGraphicsInterop> OneapiDeviceQueue::graphics_interop_create()
{
  return make_unique<OneapiDeviceGraphicsInterop>(this);
}
#  endif

CCL_NAMESPACE_END

#endif /* WITH_ONEAPI */
