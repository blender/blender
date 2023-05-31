/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "vk_pixel_buffer.hh"

#include "vk_context.hh"

namespace blender::gpu {

VKPixelBuffer::VKPixelBuffer(int64_t size) : PixelBuffer(size)
{
  buffer_.create(size,
                 GPU_USAGE_STATIC,
                 static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT));
  debug::object_label(buffer_.vk_handle(), "PixelBuffer");
}

void *VKPixelBuffer::map()
{
  /* Vulkan buffers are always mapped between allocation and freeing. */
  return buffer_.mapped_memory_get();
}

void VKPixelBuffer::unmap()
{
  /* Vulkan buffers are always mapped between allocation and freeing. */
}

int64_t VKPixelBuffer::get_native_handle()
{
  return int64_t(buffer_.vk_handle());
}

size_t VKPixelBuffer::get_size()
{
  return size_;
}

}  // namespace blender::gpu
