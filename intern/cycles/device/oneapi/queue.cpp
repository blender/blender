/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Intel Corporation */

#ifdef WITH_ONEAPI

#  include "device/oneapi/queue.h"
#  include "device/oneapi/device_impl.h"
#  include "util/log.h"
#  include "util/time.h"
#  include <iomanip>
#  include <vector>

#  include "kernel/device/oneapi/kernel.h"

CCL_NAMESPACE_BEGIN

struct KernelExecutionInfo {
  double elapsed_summary = 0.0;
  int enqueue_count = 0;
};

/* OneapiDeviceQueue */

OneapiDeviceQueue::OneapiDeviceQueue(OneapiDevice *device)
    : DeviceQueue(device), oneapi_device_(device), kernel_context_(nullptr)
{
}

OneapiDeviceQueue::~OneapiDeviceQueue()
{
  delete kernel_context_;
}

int OneapiDeviceQueue::num_concurrent_states(const size_t state_size) const
{
  const int max_num_threads = oneapi_device_->get_num_multiprocessors() *
                              oneapi_device_->get_max_num_threads_per_multiprocessor();
  int num_states = max(8 * max_num_threads, 65536) * 16;

  VLOG_DEVICE_STATS << "GPU queue concurrent states: " << num_states << ", using up to "
                    << string_human_readable_size(num_states * state_size);

  return num_states;
}

int OneapiDeviceQueue::num_concurrent_busy_states(const size_t /*state_size*/) const
{
  const int max_num_threads = oneapi_device_->get_num_multiprocessors() *
                              oneapi_device_->get_max_num_threads_per_multiprocessor();

  return 4 * max(8 * max_num_threads, 65536);
}

void OneapiDeviceQueue::init_execution()
{
  oneapi_device_->load_texture_info();

  SyclQueue *device_queue = oneapi_device_->sycl_queue();
  void *kg_dptr = (void *)oneapi_device_->kernel_globals_device_pointer();
  assert(device_queue);
  assert(kg_dptr);
  kernel_context_ = new KernelContext{device_queue, kg_dptr};

  debug_init_execution();
}

bool OneapiDeviceQueue::enqueue(DeviceKernel kernel,
                                const int signed_kernel_work_size,
                                DeviceKernelArguments const &_args)
{
  if (oneapi_device_->have_error()) {
    return false;
  }

  void **args = const_cast<void **>(_args.values);

  debug_enqueue_begin(kernel, signed_kernel_work_size);
  assert(signed_kernel_work_size >= 0);
  size_t kernel_work_size = (size_t)signed_kernel_work_size;

  size_t kernel_local_size = oneapi_kernel_preferred_local_size(
      kernel_context_->queue, (::DeviceKernel)kernel, kernel_work_size);
  size_t uniformed_kernel_work_size = round_up(kernel_work_size, kernel_local_size);

  assert(kernel_context_);

  /* Call the oneAPI kernel DLL to launch the requested kernel. */
  bool is_finished_ok = oneapi_device_->enqueue_kernel(
      kernel_context_, kernel, uniformed_kernel_work_size, args);

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
  if (is_finished_ok == false)
    oneapi_device_->set_error("oneAPI unknown kernel execution error: got runtime exception \"" +
                              oneapi_device_->oneapi_error_message() + "\"");

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

CCL_NAMESPACE_END

#endif /* WITH_ONEAPI */
