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

#ifdef WITH_HIP

#  include "device/graphics_interop.h"

#  ifdef WITH_HIP_DYNLOAD
#    include "hipew.h"
#  endif

CCL_NAMESPACE_BEGIN

class HIPDevice;
class HIPDeviceQueue;

class HIPDeviceGraphicsInterop : public DeviceGraphicsInterop {
 public:
  explicit HIPDeviceGraphicsInterop(HIPDeviceQueue *queue);

  HIPDeviceGraphicsInterop(const HIPDeviceGraphicsInterop &other) = delete;
  HIPDeviceGraphicsInterop(HIPDeviceGraphicsInterop &&other) noexcept = delete;

  ~HIPDeviceGraphicsInterop();

  HIPDeviceGraphicsInterop &operator=(const HIPDeviceGraphicsInterop &other) = delete;
  HIPDeviceGraphicsInterop &operator=(HIPDeviceGraphicsInterop &&other) = delete;

  virtual void set_display_interop(const DisplayDriver::GraphicsInterop &display_interop) override;

  virtual device_ptr map() override;
  virtual void unmap() override;

 protected:
  HIPDeviceQueue *queue_ = nullptr;
  HIPDevice *device_ = nullptr;

  /* OpenGL PBO which is currently registered as the destination for the CUDA buffer. */
  uint opengl_pbo_id_ = 0;
  /* Buffer area in pixels of the corresponding PBO. */
  int64_t buffer_area_ = 0;

  /* The destination was requested to be cleared. */
  bool need_clear_ = false;

  hipGraphicsResource hip_graphics_resource_ = nullptr;
};

CCL_NAMESPACE_END

#endif
