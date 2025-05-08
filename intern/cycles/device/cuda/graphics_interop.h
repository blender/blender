/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_CUDA

#  include "device/graphics_interop.h"
#  include "session/display_driver.h"

#  ifdef WITH_CUDA_DYNLOAD
#    include "cuew.h"
#  else
#    include <cuda.h>
#  endif

CCL_NAMESPACE_BEGIN

class CUDADevice;
class CUDADeviceQueue;

class CUDADeviceGraphicsInterop : public DeviceGraphicsInterop {
 public:
  explicit CUDADeviceGraphicsInterop(CUDADeviceQueue *queue);

  CUDADeviceGraphicsInterop(const CUDADeviceGraphicsInterop &other) = delete;
  CUDADeviceGraphicsInterop(CUDADeviceGraphicsInterop &&other) noexcept = delete;

  ~CUDADeviceGraphicsInterop() override;

  CUDADeviceGraphicsInterop &operator=(const CUDADeviceGraphicsInterop &other) = delete;
  CUDADeviceGraphicsInterop &operator=(CUDADeviceGraphicsInterop &&other) = delete;

  void set_buffer(GraphicsInteropBuffer &interop_buffer) override;

  device_ptr map() override;
  void unmap() override;

 protected:
  CUDADeviceQueue *queue_ = nullptr;
  CUDADevice *device_ = nullptr;

  /* Size of the buffer in bytes. */
  size_t buffer_size_ = 0;

  /* The destination was requested to be cleared. */
  bool need_zero_ = false;

  /* CUDA resources. */
  CUgraphicsResource cu_graphics_resource_ = nullptr;
  CUexternalMemory cu_external_memory_ = nullptr;
  CUdeviceptr cu_external_memory_ptr_ = 0;

  /* Vulkan handle to free. */
#  ifdef _WIN32
  int64_t vulkan_windows_handle_ = 0;
#  endif

  void free();
};

CCL_NAMESPACE_END

#endif
