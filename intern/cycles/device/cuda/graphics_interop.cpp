/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_CUDA

#  include "device/cuda/graphics_interop.h"

#  include "device/cuda/device_impl.h"
#  include "device/cuda/util.h"

#  include "session/display_driver.h"

#  ifdef _WIN32
#    include "util/windows.h"
#  else
#    include <unistd.h>
#  endif

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

void CUDADeviceGraphicsInterop::set_buffer(GraphicsInteropBuffer &interop_buffer)
{
  CUDAContextScope scope(device_);

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
      const CUresult result = cuGraphicsGLRegisterBuffer(&cu_graphics_resource_,
                                                         interop_buffer.take_handle(),
                                                         CU_GRAPHICS_MAP_RESOURCE_FLAGS_NONE);
      if (result != CUDA_SUCCESS) {
        LOG_ERROR << "Error registering OpenGL buffer: " << cuewErrorString(result);
        break;
      }

      buffer_size_ = interop_buffer.get_size();
      break;
    }
    case GraphicsInteropDevice::VULKAN: {
      CUDA_EXTERNAL_MEMORY_HANDLE_DESC external_memory_handle_desc = {};
#  ifdef _WIN32
      /* cuImportExternalMemory will not take ownership of the handle. */
      vulkan_windows_handle_ = interop_buffer.take_handle();
      external_memory_handle_desc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32;
      external_memory_handle_desc.handle.win32.handle = reinterpret_cast<void *>(
          vulkan_windows_handle_);
#  else
      /* cuImportExternalMemory will take ownership of the handle. */
      external_memory_handle_desc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD;
      external_memory_handle_desc.handle.fd = interop_buffer.take_handle();
#  endif
      external_memory_handle_desc.size = interop_buffer.get_size();

      CUresult result = cuImportExternalMemory(&cu_external_memory_, &external_memory_handle_desc);
      if (result != CUDA_SUCCESS) {
#  ifdef _WIN32
        CloseHandle(HANDLE(vulkan_windows_handle_));
        vulkan_windows_handle_ = 0;
#  else
        close(external_memory_handle_desc.handle.fd);
#  endif
        LOG_ERROR << "Error importing Vulkan memory: " << cuewErrorString(result);
        break;
      }

      buffer_size_ = interop_buffer.get_size();

      CUDA_EXTERNAL_MEMORY_BUFFER_DESC external_memory_buffer_desc = {};
      external_memory_buffer_desc.size = external_memory_handle_desc.size;
      external_memory_buffer_desc.offset = 0;

      CUdeviceptr external_memory_device_ptr = 0;
      result = cuExternalMemoryGetMappedBuffer(
          &external_memory_device_ptr, cu_external_memory_, &external_memory_buffer_desc);
      if (result != CUDA_SUCCESS) {
        if (external_memory_device_ptr) {
          cuMemFree(external_memory_device_ptr);
          external_memory_device_ptr = 0;
        }

        LOG_ERROR << "Error mapping Vulkan memory: " << cuewErrorString(result);
        break;
      }

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

  if (cu_buffer && need_zero_) {
    cuda_device_assert(device_, cuMemsetD8Async(cu_buffer, 0, buffer_size_, queue_->stream()));

    need_zero_ = false;
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

  if (cu_external_memory_ptr_) {
    cuda_device_assert(device_, cuMemFree(cu_external_memory_ptr_));
    cu_external_memory_ptr_ = 0;
  }

  if (cu_external_memory_) {
    cuda_device_assert(device_, cuDestroyExternalMemory(cu_external_memory_));
    cu_external_memory_ = nullptr;
  }

#  ifdef _WIN32
  if (vulkan_windows_handle_) {
    CloseHandle(HANDLE(vulkan_windows_handle_));
    vulkan_windows_handle_ = 0;
  }
#  endif

  buffer_size_ = 0;

  need_zero_ = false;
}

CCL_NAMESPACE_END

#endif
