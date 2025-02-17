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

bool VKBuffer::create(size_t size_in_bytes,
                      VkBufferUsageFlags buffer_usage,
                      VkMemoryPropertyFlags required_flags,
                      VkMemoryPropertyFlags preferred_flags,
                      VmaAllocationCreateFlags allocation_flags)
{
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
  vma_create_info.flags = allocation_flags;
  vma_create_info.priority = 1.0f;
  vma_create_info.requiredFlags = required_flags;
  vma_create_info.preferredFlags = preferred_flags;
  vma_create_info.usage = VMA_MEMORY_USAGE_AUTO;

  VkResult result = vmaCreateBuffer(
      allocator, &create_info, &vma_create_info, &vk_buffer_, &allocation_, nullptr);
  if (result != VK_SUCCESS) {
    return false;
  }

  device.resources.add_buffer(vk_buffer_);

  vmaGetAllocationMemoryProperties(allocator, allocation_, &vk_memory_property_flags_);

  if (vk_memory_property_flags_ & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
    return map();
  }
  return true;
}

void VKBuffer::update_immediately(const void *data) const
{
  BLI_assert_msg(is_mapped(), "Cannot update a non-mapped buffer.");
  memcpy(mapped_memory_, data, size_in_bytes_);
}

void VKBuffer::update_render_graph(VKContext &context, void *data) const
{
  BLI_assert(size_in_bytes_ <= 65536 && size_in_bytes_ % 4 == 0);
  render_graph::VKUpdateBufferNode::CreateInfo update_buffer = {};
  update_buffer.dst_buffer = vk_buffer_;
  update_buffer.data_size = size_in_bytes_;
  update_buffer.data = data;
  context.render_graph().add_node(update_buffer);
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
  context.render_graph().add_node(fill_buffer);
}

void VKBuffer::async_flush_to_host(VKContext &context)
{
  BLI_assert(async_timeline_ == 0);
  context.rendering_end();
  context.descriptor_set_get().upload_descriptor_sets();
  async_timeline_ = context.flush_render_graph(RenderGraphFlushFlags::SUBMIT |
                                               RenderGraphFlushFlags::RENEW_RENDER_GRAPH);
}

void VKBuffer::read_async(VKContext &context, void *data)
{
  BLI_assert_msg(is_mapped(), "Cannot read a non-mapped buffer.");
  if (async_timeline_ == 0) {
    async_flush_to_host(context);
  }
  VKDevice &device = VKBackend::get().device;
  device.wait_for_timeline(async_timeline_);
  async_timeline_ = 0;
  memcpy(data, mapped_memory_, size_in_bytes_);
}

void VKBuffer::read(VKContext &context, void *data) const
{

  BLI_assert_msg(is_mapped(), "Cannot read a non-mapped buffer.");
  BLI_assert(async_timeline_ == 0);
  context.rendering_end();
  context.descriptor_set_get().upload_descriptor_sets();
  context.flush_render_graph(RenderGraphFlushFlags::SUBMIT |
                             RenderGraphFlushFlags::WAIT_FOR_COMPLETION |
                             RenderGraphFlushFlags::RENEW_RENDER_GRAPH);
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

  VKDiscardPool::discard_pool_get().discard_buffer(vk_buffer_, allocation_);

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
