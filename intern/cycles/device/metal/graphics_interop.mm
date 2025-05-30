/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_METAL

#  include "device/metal/graphics_interop.h"

#  include "device/metal/device_impl.h"

CCL_NAMESPACE_BEGIN

MetalDeviceGraphicsInterop::MetalDeviceGraphicsInterop(MetalDeviceQueue *queue)
    : queue_(queue), device_(static_cast<MetalDevice *>(queue->device))
{
}

MetalDeviceGraphicsInterop::~MetalDeviceGraphicsInterop() = default;

void MetalDeviceGraphicsInterop::set_buffer(GraphicsInteropBuffer &interop_buffer)
{
  if (interop_buffer.is_empty()) {
    mem_.mtlBuffer = nullptr;
    size_ = 0;
    return;
  }

  need_zero_ |= interop_buffer.take_zero();

  if (!interop_buffer.has_new_handle()) {
    return;
  }

  mem_.mtlBuffer = reinterpret_cast<id<MTLBuffer>>(interop_buffer.take_handle());
  size_ = interop_buffer.get_size();
}

device_ptr MetalDeviceGraphicsInterop::map()
{
  if (mem_.mtlBuffer && need_zero_) {
    memset([mem_.mtlBuffer contents], 0, size_);
    need_zero_ = false;
  }

  return device_ptr(&mem_);
}

void MetalDeviceGraphicsInterop::unmap() {}

CCL_NAMESPACE_END

#endif
