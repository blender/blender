/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_buffer.hh"

namespace blender::gpu {

VKBuffer::~VKBuffer()
{
  VKContext &context = *VKContext::get();
  free(context);
}

bool VKBuffer::is_allocated() const
{
  return allocation_ != VK_NULL_HANDLE;
}

static VmaAllocationCreateFlagBits vma_allocation_flags(GPUUsageType usage)
{
  switch (usage) {
    case GPU_USAGE_STATIC:
      return static_cast<VmaAllocationCreateFlagBits>(
          VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
    case GPU_USAGE_DYNAMIC:
      return static_cast<VmaAllocationCreateFlagBits>(
          VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
    case GPU_USAGE_DEVICE_ONLY:
      return static_cast<VmaAllocationCreateFlagBits>(
          VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
          VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    case GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY:
    case GPU_USAGE_STREAM:
      break;
  }
  BLI_assert_msg(false, "Unimplemented GPUUsageType");
  return static_cast<VmaAllocationCreateFlagBits>(VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                                                  VMA_ALLOCATION_CREATE_MAPPED_BIT);
}

bool VKBuffer::create(VKContext &context,
                      int64_t size_in_bytes,
                      GPUUsageType usage,
                      VkBufferUsageFlagBits buffer_usage)
{
  BLI_assert(!is_allocated());

  size_in_bytes_ = size_in_bytes;

  VmaAllocator allocator = context.mem_allocator_get();
  VkBufferCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  create_info.flags = 0;
  create_info.size = size_in_bytes;
  create_info.usage = buffer_usage;
  /* We use the same command queue for the compute and graphics pipeline, so it is safe to use
   * exclusive resource handling. */
  create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  create_info.queueFamilyIndexCount = 1;
  create_info.pQueueFamilyIndices = context.queue_family_ptr_get();

  VmaAllocationCreateInfo vma_create_info = {};
  vma_create_info.flags = vma_allocation_flags(usage);
  vma_create_info.priority = 1.0f;
  vma_create_info.usage = VMA_MEMORY_USAGE_AUTO;

  VkResult result = vmaCreateBuffer(
      allocator, &create_info, &vma_create_info, &vk_buffer_, &allocation_, nullptr);
  return result == VK_SUCCESS;
}

bool VKBuffer::update(VKContext &context, const void *data)
{
  void *mapped_memory;
  bool result = map(context, &mapped_memory);
  if (result) {
    memcpy(mapped_memory, data, size_in_bytes_);
    unmap(context);
  }
  return result;
}

bool VKBuffer::map(VKContext &context, void **r_mapped_memory) const
{
  VmaAllocator allocator = context.mem_allocator_get();
  VkResult result = vmaMapMemory(allocator, allocation_, r_mapped_memory);
  return result == VK_SUCCESS;
}

void VKBuffer::unmap(VKContext &context) const
{
  VmaAllocator allocator = context.mem_allocator_get();
  vmaUnmapMemory(allocator, allocation_);
}

bool VKBuffer::free(VKContext &context)
{
  VmaAllocator allocator = context.mem_allocator_get();
  vmaDestroyBuffer(allocator, vk_buffer_, allocation_);
  return true;
}

}  // namespace blender::gpu
