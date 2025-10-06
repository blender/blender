/* SPDX-FileCopyrightText: 2025 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#if defined(WITH_ONEAPI) && defined(SYCL_LINEAR_MEMORY_INTEROP_AVAILABLE)

#  include "device/oneapi/graphics_interop.h"

#  include "device/oneapi/device.h"
#  include "device/oneapi/device_impl.h"
#  include "device/oneapi/queue.h"

#  include "session/display_driver.h"

#  ifdef _WIN32
#    include "util/windows.h"
#  else
#    include <unistd.h>
#  endif

CCL_NAMESPACE_BEGIN

OneapiDeviceGraphicsInterop::OneapiDeviceGraphicsInterop(OneapiDeviceQueue *queue)
    : queue_(queue), device_(static_cast<OneapiDevice *>(queue->device))
{
}

OneapiDeviceGraphicsInterop::~OneapiDeviceGraphicsInterop()
{
  free();
}

void OneapiDeviceGraphicsInterop::set_buffer(GraphicsInteropBuffer &interop_buffer)
{
  if (interop_buffer.is_empty()) {
    free();
    return;
  }

  need_zero_ |= interop_buffer.take_zero();

  if (!interop_buffer.has_new_handle()) {
    return;
  }

  free();

  if (interop_buffer.get_type() != GraphicsInteropDevice::VULKAN) {
    /* SYCL only supports interop with Vulkan and D3D. */
    LOG_ERROR
        << "oneAPI interop set_buffer called for invalid graphics API. Only Vulkan is supported.";
    return;
  }

#  ifdef _WIN32
  /* import_external_memory will not take ownership of the handle. */
  vulkan_windows_handle_ = reinterpret_cast<void *>(interop_buffer.take_handle());
  auto sycl_mem_handle_type =
      sycl::ext::oneapi::experimental::external_mem_handle_type::win32_nt_handle;
  sycl::ext::oneapi::experimental::external_mem_descriptor<
      sycl::ext::oneapi::experimental::resource_win32_handle>
      sycl_external_mem_descriptor{vulkan_windows_handle_, sycl_mem_handle_type};
#  else
  /* import_external_memory will take ownership of the file descriptor. */
  auto sycl_mem_handle_type = sycl::ext::oneapi::experimental::external_mem_handle_type::opaque_fd;
  sycl::ext::oneapi::experimental::external_mem_descriptor<
      sycl::ext::oneapi::experimental::resource_fd>
      sycl_external_mem_descriptor{static_cast<int>(interop_buffer.take_handle()),
                                   sycl_mem_handle_type};
#  endif

  sycl::queue *sycl_queue = reinterpret_cast<sycl::queue *>(device_->sycl_queue());
  try {
    sycl_external_memory_ = sycl::ext::oneapi::experimental::import_external_memory(
        sycl_external_mem_descriptor, *sycl_queue);
  }
  catch (sycl::exception &e) {
#  ifdef _WIN32
    CloseHandle(HANDLE(vulkan_windows_handle_));
    vulkan_windows_handle_ = nullptr;
#  else
    close(sycl_external_mem_descriptor.external_resource.file_descriptor);
#  endif
    LOG_ERROR << "Error importing Vulkan memory: " << e.what();
    return;
  }

  buffer_size_ = interop_buffer.get_size();

  /* Like the CUDA/HIP backend, we map the buffer persistently. */
  try {
    sycl_memory_ptr_ = sycl::ext::oneapi::experimental::map_external_linear_memory(
        sycl_external_memory_, 0, buffer_size_, *sycl_queue);
  }
  catch (sycl::exception &e) {
    try {
      sycl::ext::oneapi::experimental::release_external_memory(sycl_external_memory_, *sycl_queue);
    }
    catch (sycl::exception &e) {
      LOG_ERROR << "Could not release external Vulkan memory: " << e.what();
    }
    sycl_external_memory_ = {};
    buffer_size_ = 0;
    /* Only need to close Windows handle, as file descriptor is owned by compute API. */
#  ifdef _WIN32
    CloseHandle(HANDLE(vulkan_windows_handle_));
    vulkan_windows_handle_ = nullptr;
#  endif
    LOG_ERROR << "Error mapping external Vulkan memory: " << e.what();
    return;
  }
}

device_ptr OneapiDeviceGraphicsInterop::map()
{
  if (sycl_memory_ptr_ && need_zero_) {
    try {
      /* We do not wait on the returned event here, as CUDA also uses "cuMemsetD8Async". */
      sycl::queue *sycl_queue = reinterpret_cast<sycl::queue *>(device_->sycl_queue());
      sycl_queue->memset(sycl_memory_ptr_, 0, buffer_size_);
    }
    catch (sycl::exception &e) {
      LOG_ERROR << "Error clearing external Vulkan memory: " << e.what();
      return device_ptr(0);
    }
    need_zero_ = false;
  }

  return reinterpret_cast<device_ptr>(sycl_memory_ptr_);
}

void OneapiDeviceGraphicsInterop::unmap() {}

void OneapiDeviceGraphicsInterop::free()
{
  if (sycl_external_memory_.raw_handle) {
    sycl::queue *sycl_queue = reinterpret_cast<sycl::queue *>(device_->sycl_queue());
    try {
      sycl::ext::oneapi::experimental::unmap_external_linear_memory(sycl_memory_ptr_, *sycl_queue);
    }
    catch (sycl::exception &e) {
      LOG_ERROR << "Could not unmap external Vulkan memory: " << e.what();
    }
    try {
      sycl::ext::oneapi::experimental::release_external_memory(sycl_external_memory_, *sycl_queue);
    }
    catch (sycl::exception &e) {
      LOG_ERROR << "Could not release external Vulkan memory: " << e.what();
    }
    sycl_memory_ptr_ = {};
    sycl_external_memory_ = {};
  }

#  ifdef _WIN32
  if (vulkan_windows_handle_) {
    CloseHandle(HANDLE(vulkan_windows_handle_));
    vulkan_windows_handle_ = nullptr;
  }
#  endif

  buffer_size_ = 0;

  need_zero_ = false;
}

CCL_NAMESPACE_END

#endif
