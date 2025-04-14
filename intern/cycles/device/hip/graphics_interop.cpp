/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_HIP

#  include "device/hip/graphics_interop.h"

#  include "device/hip/device_impl.h"
#  include "device/hip/util.h"

CCL_NAMESPACE_BEGIN

HIPDeviceGraphicsInterop::HIPDeviceGraphicsInterop(HIPDeviceQueue *queue)
    : queue_(queue), device_(static_cast<HIPDevice *>(queue->device))
{
}

HIPDeviceGraphicsInterop::~HIPDeviceGraphicsInterop()
{
  HIPContextScope scope(device_);
  free();
}

void HIPDeviceGraphicsInterop::set_buffer(const GraphicsInteropBuffer &interop_buffer)
{
  const int64_t new_buffer_area = int64_t(interop_buffer.width) * interop_buffer.height;

  assert(interop_buffer.size >= interop_buffer.width * interop_buffer.height * sizeof(half4));

  need_clear_ = interop_buffer.need_clear;

  if (!interop_buffer.need_recreate) {
    if (native_type_ == interop_buffer.type && native_handle_ == interop_buffer.handle &&
        native_size_ == interop_buffer.size && buffer_area_ == new_buffer_area)
    {
      return;
    }
  }

  HIPContextScope scope(device_);
  free();

  native_type_ = interop_buffer.type;
  native_handle_ = interop_buffer.handle;
  native_size_ = interop_buffer.size;
  buffer_area_ = new_buffer_area;

  switch (interop_buffer.type) {
    case GraphicsInteropDevice::OPENGL: {
      const hipError_t result = hipGraphicsGLRegisterBuffer(
          &hip_graphics_resource_, interop_buffer.handle, hipGraphicsRegisterFlagsNone);

      if (result != hipSuccess) {
        LOG(ERROR) << "Error registering OpenGL buffer: " << hipewErrorString(result);
      }
      break;
    }
    case GraphicsInteropDevice::VULKAN:
    case GraphicsInteropDevice::METAL:
    case GraphicsInteropDevice::NONE:
      /* TODO: implement vulkan support. */
      break;
  }
}

device_ptr HIPDeviceGraphicsInterop::map()
{
  hipDeviceptr_t hip_buffer = 0;

  if (hip_graphics_resource_) {
    HIPContextScope scope(device_);
    size_t bytes;

    hip_device_assert(device_,
                      hipGraphicsMapResources(1, &hip_graphics_resource_, queue_->stream()));
    hip_device_assert(
        device_, hipGraphicsResourceGetMappedPointer(&hip_buffer, &bytes, hip_graphics_resource_));
  }
  else {
    /* Vulkan buffer is always mapped. */
    hip_buffer = hip_external_memory_ptr_;
  }

  if (hip_buffer && need_clear_) {
    hip_device_assert(
        device_, hipMemsetD8Async(hip_buffer, 0, buffer_area_ * sizeof(half4), queue_->stream()));

    need_clear_ = false;
  }

  return static_cast<device_ptr>(hip_buffer);
}

void HIPDeviceGraphicsInterop::unmap()
{
  if (hip_graphics_resource_) {
    HIPContextScope scope(device_);

    hip_device_assert(device_,
                      hipGraphicsUnmapResources(1, &hip_graphics_resource_, queue_->stream()));
  }
}

void HIPDeviceGraphicsInterop::free()
{
  if (hip_graphics_resource_) {
    hip_device_assert(device_, hipGraphicsUnregisterResource(hip_graphics_resource_));
    hip_graphics_resource_ = nullptr;
  }

  hip_external_memory_ptr_ = 0;
}

CCL_NAMESPACE_END

#endif
