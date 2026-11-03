/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_staging_buffer.hh"
#include "vk_context.hh"

namespace blender::gpu {

VKStagingBuffer::VKStagingBuffer(const VKBuffer &device_buffer,
                                 Direction direction,
                                 VkDeviceSize device_buffer_offset,
                                 VkDeviceSize region_size)
    : device_buffer_(device_buffer),
      device_buffer_offset_(device_buffer_offset),
      region_size_(region_size == UINT64_MAX ? device_buffer.size_in_bytes() : region_size)
{
  VkBufferUsageFlags usage;
  VmaMemoryUsage vma_memory_usage;
  VmaAllocationCreateFlags vma_allocation_flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
  switch (direction) {
    case Direction::HostToDevice:
      usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      vma_memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
      vma_allocation_flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
      break;
    case Direction::DeviceToHost:
      usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      /* Prefer host cached memory for readback, as uncached memory can be very slow to read on the
       * host. VMA falls back to compatible host visible memory when no cached type is available.
       */
      vma_memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
      vma_allocation_flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
      break;
  }

  host_buffer_.create(
      region_size_, usage, vma_memory_usage, vma_allocation_flags, 0.4f, false, "StagingBuffer");
}

void VKStagingBuffer::copy_to_device(VKContext &context)
{
  BLI_assert(host_buffer_.is_allocated() && host_buffer_.is_mapped());
  render_graph::VKCopyBufferNode::CreateInfo copy_buffer = {};
  copy_buffer.src_buffer = host_buffer_.resource();
  copy_buffer.dst_buffer = device_buffer_.resource();
  copy_buffer.region.dstOffset = device_buffer_offset_;
  copy_buffer.region.size = region_size_;

  context.render_graph().add_node(copy_buffer);
}

void VKStagingBuffer::copy_from_device(VKContext &context)
{
  BLI_assert(host_buffer_.is_allocated() && host_buffer_.is_mapped());
  render_graph::VKCopyBufferNode::CreateInfo copy_buffer = {};
  copy_buffer.src_buffer = device_buffer_.resource();
  copy_buffer.dst_buffer = host_buffer_.resource();
  copy_buffer.region.srcOffset = device_buffer_offset_;
  copy_buffer.region.size = region_size_;

  context.render_graph().add_node(copy_buffer);
}

void VKStagingBuffer::free()
{
  host_buffer_.free();
}

}  // namespace blender::gpu
