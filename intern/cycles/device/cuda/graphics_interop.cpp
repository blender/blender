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
    case GraphicsInteropDevice::VULKAN: {
      CUDA_EXTERNAL_MEMORY_HANDLE_DESC external_memory_handle_desc = {};
#  ifdef _WIN32
      external_memory_handle_desc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32;
      external_memory_handle_desc.handle.win32.handle = reinterpret_cast<void *>(
          interop_buffer.handle);
#  else
      external_memory_handle_desc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD;
      external_memory_handle_desc.handle.fd = interop_buffer.handle;
#  endif
      external_memory_handle_desc.size = interop_buffer.size;

      const CUresult result = cuImportExternalMemory(&cu_external_memory_,
                                                     &external_memory_handle_desc);
      if (result != CUDA_SUCCESS) {
        LOG(ERROR) << "Error importing Vulkan memory: " << cuewErrorString(result);
        break;
      }

      CUDA_EXTERNAL_MEMORY_BUFFER_DESC external_memory_buffer_desc = {};
      external_memory_buffer_desc.size = external_memory_handle_desc.size;
      external_memory_buffer_desc.offset = 0;

      CUdeviceptr external_memory_device_ptr = 0;
      cuda_device_assert(device_,
                         cuExternalMemoryGetMappedBuffer(&external_memory_device_ptr,
                                                         cu_external_memory_,
                                                         &external_memory_buffer_desc));
      cu_external_memory_ptr_ = external_memory_device_ptr;
      break;
    }
    case GraphicsInteropDevice::METAL:
    case GraphicsInteropDevice::NONE:
      break;
  }
}

device_ptr CUDADeviceGraphicsInterop::map()
{
  CUdeviceptr cu_buffer = 0;

  if (cu_graphics_resource_) {
    /* OpenGL buffer needs mapping. */
    CUDAContextScope scope(device_);
    size_t bytes;

    cuda_device_assert(device_,
                       cuGraphicsMapResources(1, &cu_graphics_resource_, queue_->stream()));
    cuda_device_assert(
        device_, cuGraphicsResourceGetMappedPointer(&cu_buffer, &bytes, cu_graphics_resource_));
  }
  else {
    /* Vulkan buffer is always mapped. */
    cu_buffer = cu_external_memory_ptr_;
  }

  if (cu_buffer && need_clear_) {
    cuda_device_assert(
        device_, cuMemsetD8Async(cu_buffer, 0, buffer_area_ * sizeof(half4), queue_->stream()));

    need_clear_ = false;
  }

  return static_cast<device_ptr>(cu_buffer);
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

  if (cu_external_memory_) {
    cuda_device_assert(device_, cuDestroyExternalMemory(cu_external_memory_));
    cu_external_memory_ = nullptr;
  }

  cu_external_memory_ptr_ = 0;
}

CCL_NAMESPACE_END

#endif
