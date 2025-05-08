/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_METAL

#  include "device/graphics_interop.h"
#  include "device/metal/device_impl.h"

#  include "session/display_driver.h"

CCL_NAMESPACE_BEGIN

class MetalDevice;
class MetalDeviceQueue;

class MetalDeviceGraphicsInterop : public DeviceGraphicsInterop {
 public:
  explicit MetalDeviceGraphicsInterop(MetalDeviceQueue *queue);

  MetalDeviceGraphicsInterop(const MetalDeviceGraphicsInterop &other) = delete;
  MetalDeviceGraphicsInterop(MetalDeviceGraphicsInterop &&other) noexcept = delete;

  ~MetalDeviceGraphicsInterop() override;

  MetalDeviceGraphicsInterop &operator=(const MetalDeviceGraphicsInterop &other) = delete;
  MetalDeviceGraphicsInterop &operator=(MetalDeviceGraphicsInterop &&other) = delete;

  void set_buffer(GraphicsInteropBuffer &interop_buffer) override;

  device_ptr map() override;
  void unmap() override;

 protected:
  MetalDeviceQueue *queue_ = nullptr;
  MetalDevice *device_ = nullptr;

  /* Native handle. */
  MetalDevice::MetalMem mem_;
  size_t size_ = 0;

  /* The destination was requested to be cleared. */
  bool need_zero_ = false;
};

CCL_NAMESPACE_END

#endif
