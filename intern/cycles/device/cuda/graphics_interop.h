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

#ifdef WITH_CUDA

#  include "device/graphics_interop.h"

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

  ~CUDADeviceGraphicsInterop();

  CUDADeviceGraphicsInterop &operator=(const CUDADeviceGraphicsInterop &other) = delete;
  CUDADeviceGraphicsInterop &operator=(CUDADeviceGraphicsInterop &&other) = delete;

  virtual void set_display_interop(const DisplayDriver::GraphicsInterop &display_interop) override;

  virtual device_ptr map() override;
  virtual void unmap() override;

 protected:
  CUDADeviceQueue *queue_ = nullptr;
  CUDADevice *device_ = nullptr;

  /* OpenGL PBO which is currently registered as the destination for the CUDA buffer. */
  uint opengl_pbo_id_ = 0;
  /* Buffer area in pixels of the corresponding PBO. */
  int64_t buffer_area_ = 0;

  /* The destination was requested to be cleared. */
  bool need_clear_ = false;

  CUgraphicsResource cu_graphics_resource_ = nullptr;
};

CCL_NAMESPACE_END

#endif
