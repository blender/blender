/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

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

  /* OpenGL PBO which is currently registered as the destination for the HIP buffer. */
  int64_t opengl_pbo_id_ = 0;
  /* Buffer area in pixels of the corresponding PBO. */
  int64_t buffer_area_ = 0;

  /* The destination was requested to be cleared. */
  bool need_clear_ = false;

  hipGraphicsResource hip_graphics_resource_ = nullptr;
};

CCL_NAMESPACE_END

#endif
