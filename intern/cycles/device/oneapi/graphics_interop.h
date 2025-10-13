/* SPDX-FileCopyrightText: 2025 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#if defined(WITH_ONEAPI) && defined(SYCL_LINEAR_MEMORY_INTEROP_AVAILABLE)

#  include <sycl/sycl.hpp>

#  include "device/graphics_interop.h"
#  include "session/display_driver.h"

#  include "device/oneapi/device.h"
#  include "device/oneapi/queue.h"

CCL_NAMESPACE_BEGIN

class OneapiDevice;
class OneapiDeviceQueue;

class OneapiDeviceGraphicsInterop : public DeviceGraphicsInterop {
 public:
  explicit OneapiDeviceGraphicsInterop(OneapiDeviceQueue *queue);

  OneapiDeviceGraphicsInterop(const OneapiDeviceGraphicsInterop &other) = delete;
  OneapiDeviceGraphicsInterop(OneapiDeviceGraphicsInterop &&other) noexcept = delete;

  ~OneapiDeviceGraphicsInterop() override;

  OneapiDeviceGraphicsInterop &operator=(const OneapiDeviceGraphicsInterop &other) = delete;
  OneapiDeviceGraphicsInterop &operator=(OneapiDeviceGraphicsInterop &&other) = delete;

  void set_buffer(GraphicsInteropBuffer &interop_buffer) override;

  device_ptr map() override;
  void unmap() override;

 protected:
  OneapiDeviceQueue *queue_ = nullptr;
  OneapiDevice *device_ = nullptr;

  /* Size of the buffer in bytes. */
  size_t buffer_size_ = 0;

  /* The destination was requested to be cleared. */
  bool need_zero_ = false;

  /* Oneapi resources. */
  sycl::ext::oneapi::experimental::external_mem sycl_external_memory_{};
  void *sycl_memory_ptr_ = nullptr;

  /* Vulkan handle to free. */
#  ifdef _WIN32
  void *vulkan_windows_handle_ = nullptr;
#  endif

  void free();
};

CCL_NAMESPACE_END

#endif
