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

#  include "device/cuda/graphics_interop.h"

#  include "device/cuda/device_impl.h"
#  include "device/cuda/util.h"

CCL_NAMESPACE_BEGIN

CUDADeviceGraphicsInterop::CUDADeviceGraphicsInterop(CUDADeviceQueue *queue)
    : queue_(queue), device_(static_cast<CUDADevice *>(queue->device))
{
}

CUDADeviceGraphicsInterop::~CUDADeviceGraphicsInterop()
{
  CUDAContextScope scope(device_);

  if (cu_graphics_resource_) {
    cuda_device_assert(device_, cuGraphicsUnregisterResource(cu_graphics_resource_));
  }
}

void CUDADeviceGraphicsInterop::set_display_interop(
    const DisplayDriver::GraphicsInterop &display_interop)
{
  const int64_t new_buffer_area = int64_t(display_interop.buffer_width) *
                                  display_interop.buffer_height;

  need_clear_ = display_interop.need_clear;

  if (!display_interop.need_recreate) {
    if (opengl_pbo_id_ == display_interop.opengl_pbo_id && buffer_area_ == new_buffer_area) {
      return;
    }
  }

  CUDAContextScope scope(device_);

  if (cu_graphics_resource_) {
    cuda_device_assert(device_, cuGraphicsUnregisterResource(cu_graphics_resource_));
  }

  const CUresult result = cuGraphicsGLRegisterBuffer(
      &cu_graphics_resource_, display_interop.opengl_pbo_id, CU_GRAPHICS_MAP_RESOURCE_FLAGS_NONE);
  if (result != CUDA_SUCCESS) {
    LOG(ERROR) << "Error registering OpenGL buffer: " << cuewErrorString(result);
  }

  opengl_pbo_id_ = display_interop.opengl_pbo_id;
  buffer_area_ = new_buffer_area;
}

device_ptr CUDADeviceGraphicsInterop::map()
{
  if (!cu_graphics_resource_) {
    return 0;
  }

  CUDAContextScope scope(device_);

  CUdeviceptr cu_buffer;
  size_t bytes;

  cuda_device_assert(device_, cuGraphicsMapResources(1, &cu_graphics_resource_, queue_->stream()));
  cuda_device_assert(
      device_, cuGraphicsResourceGetMappedPointer(&cu_buffer, &bytes, cu_graphics_resource_));

  if (need_clear_) {
    cuda_device_assert(
        device_, cuMemsetD8Async(static_cast<CUdeviceptr>(cu_buffer), 0, bytes, queue_->stream()));

    need_clear_ = false;
  }

  return static_cast<device_ptr>(cu_buffer);
}

void CUDADeviceGraphicsInterop::unmap()
{
  CUDAContextScope scope(device_);

  cuda_device_assert(device_,
                     cuGraphicsUnmapResources(1, &cu_graphics_resource_, queue_->stream()));
}

CCL_NAMESPACE_END

#endif
