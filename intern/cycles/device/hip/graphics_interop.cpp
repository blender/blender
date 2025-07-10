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

void HIPDeviceGraphicsInterop::set_buffer(GraphicsInteropBuffer &interop_buffer)
{
  HIPContextScope scope(device_);

  if (interop_buffer.is_empty()) {
    free();
    return;
  }

  need_zero_ |= interop_buffer.take_zero();

  if (!interop_buffer.has_new_handle()) {
    return;
  }

  free();

  switch (interop_buffer.get_type()) {
    case GraphicsInteropDevice::OPENGL: {
      const hipError_t result = hipGraphicsGLRegisterBuffer(
          &hip_graphics_resource_, interop_buffer.take_handle(), hipGraphicsRegisterFlagsNone);

      if (result != hipSuccess) {
        LOG_ERROR << "Error registering OpenGL buffer: " << hipewErrorString(result);
        break;
      }

      buffer_size_ = interop_buffer.get_size();

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

  if (hip_buffer && need_zero_) {
    hip_device_assert(device_, hipMemsetD8Async(hip_buffer, 0, buffer_size_, queue_->stream()));

    need_zero_ = false;
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

  if (hip_external_memory_ptr_) {
    hip_device_assert(device_, hipFree(hip_external_memory_ptr_));
    hip_external_memory_ptr_ = 0;
  }

  buffer_size_ = 0;

  need_zero_ = false;
}

CCL_NAMESPACE_END

#endif
