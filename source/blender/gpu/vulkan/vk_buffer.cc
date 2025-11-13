/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_buffer.hh"
#include "vk_backend.hh"
#include "vk_context.hh"
#include <vulkan/vulkan_core.h>

#include "CLG_log.h"

static CLG_LogRef LOG = {"gpu.vulkan"};

namespace blender::gpu {

VKBuffer::~VKBuffer()
{
  if (is_allocated()) {
    free();
  }
}

bool VKBuffer::create(size_t size_in_bytes,
                      VkBufferUsageFlags buffer_usage,
                      VmaMemoryUsage vma_memory_usage,
                      VmaAllocationCreateFlags allocation_flags,
                      float priority,
                      bool export_memory)
{
  BLI_assert(!is_allocated());
  BLI_assert(vk_buffer_ == VK_NULL_HANDLE);
  BLI_assert(mapped_memory_ == nullptr);
  if (allocation_failed_) {
    return false;
  }

  size_in_bytes_ = size_in_bytes;
  /*
   * Vulkan doesn't allow empty buffers but some areas (DrawManager Instance data, PyGPU) create
   * them.
   */
  alloc_size_in_bytes_ = ceil_to_multiple_ul(max_ulul(size_in_bytes_, 16), 16);
  VKDevice &device = VKBackend::get().device;

  /* Precheck max buffer size. */
  if (device.extensions_get().maintenance4 &&
      alloc_size_in_bytes_ > device.physical_device_maintenance4_properties_get().maxBufferSize)
  {
    CLOG_WARN(
        &LOG,
        "Couldn't allocate buffer, requested allocation exceeds the maxBufferSize of the device.");
    allocation_failed_ = true;
    size_in_bytes_ = 0;
    alloc_size_in_bytes_ = 0;
    return false;
  }

  VmaAllocator allocator = device.mem_allocator_get();
  VkBufferCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  create_info.flags = 0;
  create_info.size = alloc_size_in_bytes_;
  create_info.usage = buffer_usage;
  /* We use the same command queue for the compute and graphics pipeline, so it is safe to use
   * exclusive resource handling. */
  create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  create_info.queueFamilyIndexCount = 1;
  const uint32_t queue_family_indices[1] = {device.queue_family_get()};
  create_info.pQueueFamilyIndices = queue_family_indices;

  VkExternalMemoryBufferCreateInfo external_memory_create_info = {
      VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, nullptr, 0};

  VmaAllocationCreateInfo vma_create_info = {};
  vma_create_info.flags = allocation_flags;
  vma_create_info.priority = priority;
  vma_create_info.usage = vma_memory_usage;

  if (export_memory) {
    create_info.pNext = &external_memory_create_info;
    external_memory_create_info.handleTypes = vk_external_memory_handle_type();

    /* Dedicated allocation for zero offset. */
    vma_create_info.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    vma_create_info.pool = device.vma_pools.external_memory_pixel_buffer.pool;
  }

  VkResult result = vmaCreateBuffer(
      allocator, &create_info, &vma_create_info, &vk_buffer_, &allocation_, nullptr);
  if (result != VK_SUCCESS) {
    allocation_failed_ = true;
    size_in_bytes_ = 0;
    alloc_size_in_bytes_ = 0;
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
  update_sub_immediately(0, size_in_bytes_, data);
}

void VKBuffer::update_sub_immediately(size_t start_offset,
                                      size_t data_size,
                                      const void *data) const
{
  BLI_assert_msg(is_mapped(), "Cannot update a non-mapped buffer.");
  memcpy(static_cast<uint8_t *>(mapped_memory_) + start_offset, data, data_size);
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
  fill_buffer.size = alloc_size_in_bytes_;
  context.render_graph().add_node(fill_buffer);
}

void VKBuffer::async_flush_to_host(VKContext &context)
{
  BLI_assert(async_timeline_ == 0);
  context.rendering_end();
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
  context.flush_render_graph(RenderGraphFlushFlags::SUBMIT |
                             RenderGraphFlushFlags::WAIT_FOR_COMPLETION |
                             RenderGraphFlushFlags::RENEW_RENDER_GRAPH);
  memcpy(data, mapped_memory_, size_in_bytes_);
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

VkDeviceMemory VKBuffer::export_memory_get(size_t &memory_size)
{
  const VKDevice &device = VKBackend::get().device;
  VmaAllocator allocator = device.mem_allocator_get();

  VmaAllocationInfo info = {};
  vmaGetAllocationInfo(allocator, allocation_, &info);

  /* VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT should ensure this. */
  if (info.offset != 0) {
    BLI_assert(!"Failed to get zero offset export memory for Vulkan buffer");
    return nullptr;
  }

  memory_size = info.size;
  return info.deviceMemory;
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
