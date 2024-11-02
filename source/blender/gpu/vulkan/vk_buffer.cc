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

static VmaAllocationCreateFlags vma_allocation_flags(GPUUsageType usage)
{
  switch (usage) {
    case GPU_USAGE_STATIC:
    case GPU_USAGE_DEVICE_ONLY:
      return 0;
    case GPU_USAGE_DYNAMIC:
    case GPU_USAGE_STREAM:
      return VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    case GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY:
      break;
  }
  BLI_assert_msg(false, "Unimplemented GPUUsageType");
  return VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
}

static VkMemoryPropertyFlags vma_preferred_flags()
{
  return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
}

static VkMemoryPropertyFlags vma_required_flags(const bool is_host_visible)
{
  return is_host_visible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : 0;
}

bool VKBuffer::create(size_t size_in_bytes,
                      GPUUsageType usage,
                      VkBufferUsageFlags buffer_usage,
                      const bool is_host_visible)
{
  /*
   * TODO: Check which memory is selected and adjust the creation flag to add mapping. This way the
   * staging buffer can be skipped, or in case of a vertex buffer an intermediate buffer can be
   * removed.
   */

  BLI_assert(!is_allocated());
  BLI_assert(vk_buffer_ == VK_NULL_HANDLE);
  BLI_assert(mapped_memory_ == nullptr);

  size_in_bytes_ = size_in_bytes;
  VKDevice &device = VKBackend::get().device;

  VmaAllocator allocator = device.mem_allocator_get();
  VkBufferCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  create_info.flags = 0;
  /*
   * Vulkan doesn't allow empty buffers but some areas (DrawManager Instance data, PyGPU) create
   * them.
   */
  create_info.size = max_ulul(size_in_bytes, 1);
  create_info.usage = buffer_usage;
  /* We use the same command queue for the compute and graphics pipeline, so it is safe to use
   * exclusive resource handling. */
  create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  create_info.queueFamilyIndexCount = 1;
  const uint32_t queue_family_indices[1] = {device.queue_family_get()};
  create_info.pQueueFamilyIndices = queue_family_indices;

  VmaAllocationCreateInfo vma_create_info = {};
  vma_create_info.flags = vma_allocation_flags(usage);
  vma_create_info.priority = 1.0f;
  vma_create_info.requiredFlags = vma_required_flags(is_host_visible);
  vma_create_info.preferredFlags = vma_preferred_flags();
  vma_create_info.usage = VMA_MEMORY_USAGE_AUTO;

  VkResult result = vmaCreateBuffer(
      allocator, &create_info, &vma_create_info, &vk_buffer_, &allocation_, nullptr);
  if (result != VK_SUCCESS) {
    return false;
  }

  device.resources.add_buffer(vk_buffer_);

  if (is_host_visible) {
    return map();
  }
  return true;
}

void VKBuffer::update_immediately(const void *data) const
{
  BLI_assert_msg(is_mapped(), "Cannot update a non-mapped buffer.");
  memcpy(mapped_memory_, data, size_in_bytes_);
  flush();
}

void VKBuffer::update_render_graph(VKContext &context, void *data) const
{
  BLI_assert(size_in_bytes_ <= 65536 && size_in_bytes_ % 4 == 0);
  render_graph::VKUpdateBufferNode::CreateInfo update_buffer = {};
  update_buffer.dst_buffer = vk_buffer_;
  update_buffer.data_size = size_in_bytes_;
  update_buffer.data = data;
  context.render_graph.add_node(update_buffer);
}

void VKBuffer::flush() const
{
  const VKDevice &device = VKBackend::get().device;
  VmaAllocator allocator = device.mem_allocator_get();
  vmaFlushAllocation(allocator, allocation_, 0, max_ulul(size_in_bytes(), 1));
}

void VKBuffer::clear(VKContext &context, uint32_t clear_value)
{
  render_graph::VKFillBufferNode::CreateInfo fill_buffer = {};
  fill_buffer.vk_buffer = vk_buffer_;
  fill_buffer.data = clear_value;
  fill_buffer.size = size_in_bytes_;
  context.render_graph.add_node(fill_buffer);
}

void VKBuffer::read(VKContext &context, void *data) const
{
  BLI_assert_msg(is_mapped(), "Cannot read a non-mapped buffer.");
  context.rendering_end();
  context.descriptor_set_get().upload_descriptor_sets();
  context.render_graph.submit_buffer_for_read(vk_buffer_);
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
  const VKDevice &device = VKBackend::get().device;
  VmaAllocator allocator = device.mem_allocator_get();
  VkResult result = vmaMapMemory(allocator, allocation_, &mapped_memory_);
  return result == VK_SUCCESS;
}

void VKBuffer::unmap()
{
  BLI_assert(is_mapped());
  const VKDevice &device = VKBackend::get().device;
  VmaAllocator allocator = device.mem_allocator_get();
  vmaUnmapMemory(allocator, allocation_);
  mapped_memory_ = nullptr;
}

bool VKBuffer::free()
{
  if (is_mapped()) {
    unmap();
  }

  VKDevice &device = VKBackend::get().device;
  device.discard_pool_for_current_thread().discard_buffer(vk_buffer_, allocation_);

  allocation_ = VK_NULL_HANDLE;
  vk_buffer_ = VK_NULL_HANDLE;

  return true;
}

void VKBuffer::free_immediately(VKDevice &device)
{
  BLI_assert(vk_buffer_ != VK_NULL_HANDLE);
  BLI_assert(allocation_ != VK_NULL_HANDLE);
  if (is_mapped()) {
    unmap();
  }
  device.resources.remove_buffer(vk_buffer_);
  vmaDestroyBuffer(device.mem_allocator_get(), vk_buffer_, allocation_);
  allocation_ = VK_NULL_HANDLE;
  vk_buffer_ = VK_NULL_HANDLE;
}

}  // namespace blender::gpu
