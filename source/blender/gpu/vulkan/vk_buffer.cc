/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_buffer.hh"
#include "vk_backend.hh"
#include "vk_context.hh"

namespace blender::gpu {

VKBuffer::~VKBuffer()
{
  if (is_allocated()) {
    free();
  }
}

bool VKBuffer::is_allocated() const
{
  return allocation_ != VK_NULL_HANDLE;
}

static VmaAllocationCreateFlagBits vma_allocation_flags(GPUUsageType usage)
{
  switch (usage) {
    case GPU_USAGE_STATIC:
    case GPU_USAGE_DYNAMIC:
    case GPU_USAGE_STREAM:
      return static_cast<VmaAllocationCreateFlagBits>(
          VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
    case GPU_USAGE_DEVICE_ONLY:
      return static_cast<VmaAllocationCreateFlagBits>(
          VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
          VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    case GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY:
      break;
  }
  BLI_assert_msg(false, "Unimplemented GPUUsageType");
  return static_cast<VmaAllocationCreateFlagBits>(VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                                                  VMA_ALLOCATION_CREATE_MAPPED_BIT);
}

bool VKBuffer::create(int64_t size_in_bytes,
                      GPUUsageType usage,
                      VkBufferUsageFlagBits buffer_usage)
{
  BLI_assert(!is_allocated());
  BLI_assert(vk_buffer_ == VK_NULL_HANDLE);
  BLI_assert(mapped_memory_ == nullptr);

  size_in_bytes_ = size_in_bytes;
  const VKDevice &device = VKBackend::get().device_get();

  VmaAllocator allocator = device.mem_allocator_get();
  VkBufferCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  create_info.flags = 0;
  /*
   * Vulkan doesn't allow empty buffers but some areas (DrawManager Instance data, PyGPU) create
   * them.
   */
  create_info.size = max_ii(size_in_bytes, 1);
  create_info.usage = buffer_usage;
  /* We use the same command queue for the compute and graphics pipeline, so it is safe to use
   * exclusive resource handling. */
  create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  create_info.queueFamilyIndexCount = 1;
  create_info.pQueueFamilyIndices = device.queue_family_ptr_get();

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
  return map();
}

void VKBuffer::update(const void *data) const
{
  BLI_assert_msg(is_mapped(), "Cannot update a non-mapped buffer.");
  memcpy(mapped_memory_, data, size_in_bytes_);

  const VKDevice &device = VKBackend::get().device_get();
  VmaAllocator allocator = device.mem_allocator_get();
  vmaFlushAllocation(allocator, allocation_, 0, max_ii(size_in_bytes(), 1));
}

void VKBuffer::clear(VKContext &context, uint32_t clear_value)
{
  VKCommandBuffer &command_buffer = context.command_buffer_get();
  command_buffer.fill(*this, clear_value);
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

bool VKBuffer::map()
{
  BLI_assert(!is_mapped());
  const VKDevice &device = VKBackend::get().device_get();
  VmaAllocator allocator = device.mem_allocator_get();
  VkResult result = vmaMapMemory(allocator, allocation_, &mapped_memory_);
  return result == VK_SUCCESS;
}

void VKBuffer::unmap()
{
  BLI_assert(is_mapped());
  const VKDevice &device = VKBackend::get().device_get();
  VmaAllocator allocator = device.mem_allocator_get();
  vmaUnmapMemory(allocator, allocation_);
  mapped_memory_ = nullptr;
}

bool VKBuffer::free()
{
  if (is_mapped()) {
    unmap();
  }

  VKDevice &device = VKBackend::get().device_get();
  device.discard_buffer(vk_buffer_, allocation_);
  allocation_ = VK_NULL_HANDLE;
  vk_buffer_ = VK_NULL_HANDLE;
  return true;
}

}  // namespace blender::gpu
