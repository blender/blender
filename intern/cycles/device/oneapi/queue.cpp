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
    : DeviceQueue(device),
      oneapi_device_(device),
      oneapi_dll_(device->oneapi_dll_object()),
      kernel_context_(nullptr)
{
}

OneapiDeviceQueue::~OneapiDeviceQueue()
{
  delete kernel_context_;
}

int OneapiDeviceQueue::num_concurrent_states(const size_t state_size) const
{
  int num_states;

  /* TODO: implement and use get_num_multiprocessors and get_max_num_threads_per_multiprocessor. */
  const size_t compute_units = oneapi_dll_.oneapi_get_compute_units_amount(
      oneapi_device_->sycl_queue());
  if (compute_units >= 128) {
    /* dGPU path, make sense to allocate more states, because it will be dedicated GPU memory. */
    int base = 1024 * 1024;
    /* linear dependency (with coefficient less that 1) from amount of compute units. */
    num_states = (base * (compute_units / 128)) * 3 / 4;

    /* Limit amount of integrator states by one quarter of device memory, because
     * other allocations will need some space as well
     * TODO: base this calculation on the how many states what the GPU is actually capable of
     * running, with some headroom to improve occupancy. If the texture don't fit, offload into
     * unified memory. */
    size_t states_memory_size = num_states * state_size;
    size_t device_memory_amount =
        (oneapi_dll_.oneapi_get_memcapacity)(oneapi_device_->sycl_queue());
    if (states_memory_size >= device_memory_amount / 4) {
      num_states = device_memory_amount / 4 / state_size;
    }
  }
  else {
    /* iGPU path - no real need to allocate a lot of integrator states because it is shared GPU
     * memory. */
    num_states = 1024 * 512;
  }

  VLOG_DEVICE_STATS << "GPU queue concurrent states: " << num_states << ", using up to "
                    << string_human_readable_size(num_states * state_size);

  return num_states;
}

int OneapiDeviceQueue::num_concurrent_busy_states() const
{
  const size_t compute_units = oneapi_dll_.oneapi_get_compute_units_amount(
      oneapi_device_->sycl_queue());
  if (compute_units >= 128) {
    return 1024 * 1024;
  }
  else {
    return 1024 * 512;
  }
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

  debug_enqueue(kernel, signed_kernel_work_size);
  assert(signed_kernel_work_size >= 0);
  size_t kernel_work_size = (size_t)signed_kernel_work_size;

  size_t kernel_local_size = oneapi_dll_.oneapi_kernel_preferred_local_size(
      kernel_context_->queue, (::DeviceKernel)kernel, kernel_work_size);
  size_t uniformed_kernel_work_size = round_up(kernel_work_size, kernel_local_size);

  assert(kernel_context_);

  /* Call the oneAPI kernel DLL to launch the requested kernel. */
  bool is_finished_ok = oneapi_dll_.oneapi_enqueue_kernel(
      kernel_context_, kernel, uniformed_kernel_work_size, args);

  if (is_finished_ok == false) {
    oneapi_device_->set_error("oneAPI kernel \"" + std::string(device_kernel_as_string(kernel)) +
                              "\" execution error: got runtime exception \"" +
                              oneapi_device_->oneapi_error_message() + "\"");
  }

  return is_finished_ok;
}

bool OneapiDeviceQueue::synchronize()
{
  if (oneapi_device_->have_error()) {
    return false;
  }

  bool is_finished_ok = oneapi_dll_.oneapi_queue_synchronize(oneapi_device_->sycl_queue());
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
