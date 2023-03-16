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
  if (result != VK_SUCCESS) {
    return false;
  }

  /* All buffers are mapped to virtual memory. */
  return map(context);
}

void VKBuffer::update(const void *data) const
{
  BLI_assert_msg(is_mapped(), "Cannot update a non-mapped buffer.");
  memcpy(mapped_memory_, data, size_in_bytes_);
}

void VKBuffer::read(void *data) const
{
  BLI_assert_msg(is_mapped(), "Cannot read a non-mapped buffer.");
  memcpy(data, mapped_memory_, size_in_bytes_);
}

void *VKBuffer::mapped_memory_get() const
{
  BLI_assert_msg(is_mapped(), "Cannot access a non-mapped buffer.");
  return mapped_memory_;
}

bool VKBuffer::is_mapped() const
{
  return mapped_memory_ != nullptr;
}

bool VKBuffer::map(VKContext &context)
{
  BLI_assert(!is_mapped());
  VmaAllocator allocator = context.mem_allocator_get();
  VkResult result = vmaMapMemory(allocator, allocation_, &mapped_memory_);
  return result == VK_SUCCESS;
}

void VKBuffer::unmap(VKContext &context)
{
  BLI_assert(is_mapped());
  VmaAllocator allocator = context.mem_allocator_get();
  vmaUnmapMemory(allocator, allocation_);
  mapped_memory_ = nullptr;
}

bool VKBuffer::free(VKContext &context)
{
  if (is_mapped()) {
    unmap(context);
  }

  VmaAllocator allocator = context.mem_allocator_get();
  vmaDestroyBuffer(allocator, vk_buffer_, allocation_);
  return true;
}

}  // namespace blender::gpu
