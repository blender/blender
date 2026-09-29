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

ConcurrentStatesParams OneapiDeviceQueue::concurrent_states_params() const
{
  /* NOTE(@xavierh-intel): At the time of writing this, 32768 * the number of
   * Execution Units is the best baseline across at least Intel Arc B390 and
   * Arc Pro B70.  */
  ConcurrentStatesParams params;
  params.baseline = 32768 * oneapi_device_->get_num_multiprocessors();
  /* Only shrink to fit within available memory, growing did not show clear
   * benefit compared to the baseline. */
  params.min = params.baseline / 8;
  params.max = params.baseline;
  params.reserve_percent = 2;
  return params;
}

void OneapiDeviceQueue::get_memory_info(size_t &total, size_t &free) const
{
  oneapi_device_->get_device_memory_info(total, free);
}

int OneapiDeviceQueue::num_sort_partitions(int max_num_paths, uint /*max_scene_shaders*/) const
{
  int sort_partition_elements = (oneapi_device_->get_max_num_threads_per_multiprocessor() >= 128) ?
                                    32768 :
                                    8192;
  /* Sort partitioning with local sorting on Intel GPUs is currently the most effective solution no
   * matter the number of shaders. */
  return max(max_num_paths / sort_partition_elements, 1);
}

void OneapiDeviceQueue::init_execution()
{
  oneapi_device_->load_image_info(nullptr);

  SyclQueue *device_queue = oneapi_device_->sycl_queue();
  void *kg_dptr = oneapi_device_->kernel_globals_device_pointer();
  assert(device_queue);
  assert(kg_dptr);
  kernel_context_ = make_unique<KernelContext>();
  kernel_context_->queue = device_queue;
  kernel_context_->kernel_globals = kg_dptr;

  debug_init_execution();
}

void OneapiDeviceQueue::load_image_info()
{
  oneapi_device_->load_image_info(this);
}

bool OneapiDeviceQueue::enqueue(DeviceKernel kernel,
                                const int signed_kernel_work_size,
                                const DeviceKernelArguments &_args)
{
  if (oneapi_device_->have_error()) {
    return false;
  }

  /* Update image info in case memory moved to host. */
  if (oneapi_device_->load_image_info(nullptr)) {
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

void *OneapiDeviceQueue::copy_from_device_synchronized(device_memory &mem,
                                                       vector<uint8_t> &storage)
{
  if (mem.memory_size() == 0) {
    return nullptr;
  }

  storage.resize(mem.memory_size());
  oneapi_device_->mem_copy_from(mem, 0, 0, 0, 0, storage.data());
  synchronize();
  return storage.data();
}

#  ifdef SYCL_LINEAR_MEMORY_INTEROP_AVAILABLE
unique_ptr<DeviceGraphicsInterop> OneapiDeviceQueue::graphics_interop_create()
{
  return make_unique<OneapiDeviceGraphicsInterop>(this);
}
#  endif

CCL_NAMESPACE_END

#endif /* WITH_ONEAPI */
