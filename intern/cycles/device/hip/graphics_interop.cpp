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

  if (hip_graphics_resource_) {
    hip_device_assert(device_, hipGraphicsUnregisterResource(hip_graphics_resource_));
  }
}

void HIPDeviceGraphicsInterop::set_display_interop(
    const DisplayDriver::GraphicsInterop &display_interop)
{
  const int64_t new_buffer_area = int64_t(display_interop.buffer_width) *
                                  display_interop.buffer_height;

  need_clear_ = display_interop.need_clear;

  if (opengl_pbo_id_ == display_interop.opengl_pbo_id && buffer_area_ == new_buffer_area) {
    return;
  }

  HIPContextScope scope(device_);

  if (hip_graphics_resource_) {
    hip_device_assert(device_, hipGraphicsUnregisterResource(hip_graphics_resource_));
  }

  const hipError_t result = hipGraphicsGLRegisterBuffer(
      &hip_graphics_resource_, display_interop.opengl_pbo_id, hipGraphicsRegisterFlagsNone);
  if (result != hipSuccess) {
    LOG(ERROR) << "Error registering OpenGL buffer: " << hipewErrorString(result);
  }

  opengl_pbo_id_ = display_interop.opengl_pbo_id;
  buffer_area_ = new_buffer_area;
}

device_ptr HIPDeviceGraphicsInterop::map()
{
  if (!hip_graphics_resource_) {
    return 0;
  }

  HIPContextScope scope(device_);

  hipDeviceptr_t hip_buffer;
  size_t bytes;

  hip_device_assert(device_,
                    hipGraphicsMapResources(1, &hip_graphics_resource_, queue_->stream()));
  hip_device_assert(
      device_, hipGraphicsResourceGetMappedPointer(&hip_buffer, &bytes, hip_graphics_resource_));

  if (need_clear_) {
    hip_device_assert(
        device_,
        hipMemsetD8Async(static_cast<hipDeviceptr_t>(hip_buffer), 0, bytes, queue_->stream()));

    need_clear_ = false;
  }

  return static_cast<device_ptr>(hip_buffer);
}

void HIPDeviceGraphicsInterop::unmap()
{
  HIPContextScope scope(device_);

  hip_device_assert(device_,
                    hipGraphicsUnmapResources(1, &hip_graphics_resource_, queue_->stream()));
}

CCL_NAMESPACE_END

#endif
