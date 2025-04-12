/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

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
  free();
}

void CUDADeviceGraphicsInterop::set_buffer(const GraphicsInteropBuffer &interop_buffer)
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

  CUDAContextScope scope(device_);
  free();

  native_type_ = interop_buffer.type;
  native_handle_ = interop_buffer.handle;
  native_size_ = interop_buffer.size;
  buffer_area_ = new_buffer_area;

  switch (interop_buffer.type) {
    case GraphicsInteropDevice::OPENGL: {
      const CUresult result = cuGraphicsGLRegisterBuffer(
          &cu_graphics_resource_, interop_buffer.handle, CU_GRAPHICS_MAP_RESOURCE_FLAGS_NONE);
      if (result != CUDA_SUCCESS) {
        LOG(ERROR) << "Error registering OpenGL buffer: " << cuewErrorString(result);
      }
      break;
    }
    case GraphicsInteropDevice::NONE:
      break;
  }
}

device_ptr CUDADeviceGraphicsInterop::map()
{
  if (cu_graphics_resource_) {
    /* OpenGL buffer needs mapping. */
    CUDAContextScope scope(device_);

    CUdeviceptr cu_buffer;
    size_t bytes;

    cuda_device_assert(device_,
                       cuGraphicsMapResources(1, &cu_graphics_resource_, queue_->stream()));
    cuda_device_assert(
        device_, cuGraphicsResourceGetMappedPointer(&cu_buffer, &bytes, cu_graphics_resource_));

    if (need_clear_) {
      cuda_device_assert(
          device_,
          cuMemsetD8Async(static_cast<CUdeviceptr>(cu_buffer), 0, bytes, queue_->stream()));

      need_clear_ = false;
    }

    return static_cast<device_ptr>(cu_buffer);
  }

  return 0;
}

void CUDADeviceGraphicsInterop::unmap()
{
  if (cu_graphics_resource_) {
    CUDAContextScope scope(device_);

    cuda_device_assert(device_,
                       cuGraphicsUnmapResources(1, &cu_graphics_resource_, queue_->stream()));
  }
}

void CUDADeviceGraphicsInterop::free()
{
  if (cu_graphics_resource_) {
    cuda_device_assert(device_, cuGraphicsUnregisterResource(cu_graphics_resource_));
    cu_graphics_resource_ = nullptr;
  }
}

CCL_NAMESPACE_END

#endif
