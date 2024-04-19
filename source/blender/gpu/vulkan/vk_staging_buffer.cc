/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_staging_buffer.hh"
#include "vk_command_buffers.hh"
#include "vk_context.hh"

namespace blender::gpu {

VKStagingBuffer::VKStagingBuffer(const VKBuffer &device_buffer, Direction direction)
    : device_buffer_(device_buffer)
{
  VkBufferUsageFlags usage;
  switch (direction) {
    case Direction::HostToDevice:
      usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      break;
    case Direction::DeviceToHost:
      usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  }

  host_buffer_.create(device_buffer.size_in_bytes(), GPU_USAGE_STREAM, usage, true);
}

void VKStagingBuffer::copy_to_device(VKContext &context)
{
  BLI_assert(host_buffer_.is_allocated() && host_buffer_.is_mapped());
  render_graph::VKCopyBufferNode::CreateInfo copy_buffer = {};
  copy_buffer.src_buffer = host_buffer_.vk_handle();
  copy_buffer.dst_buffer = device_buffer_.vk_handle();
  copy_buffer.region.size = device_buffer_.size_in_bytes();

  if (use_render_graph) {
    context.render_graph.add_node(copy_buffer);
  }
  else {
    VKCommandBuffers &command_buffers = context.command_buffers_get();
    command_buffers.copy(
        device_buffer_, copy_buffer.src_buffer, Span<VkBufferCopy>(&copy_buffer.region, 1));
    command_buffers.submit();
  }
}

void VKStagingBuffer::copy_from_device(VKContext &context)
{
  BLI_assert(host_buffer_.is_allocated() && host_buffer_.is_mapped());
  render_graph::VKCopyBufferNode::CreateInfo copy_buffer = {};
  copy_buffer.src_buffer = device_buffer_.vk_handle();
  copy_buffer.dst_buffer = host_buffer_.vk_handle();
  copy_buffer.region.size = device_buffer_.size_in_bytes();

  if (use_render_graph) {
    context.render_graph.add_node(copy_buffer);
  }
  else {
    VKCommandBuffers &command_buffers = context.command_buffers_get();
    command_buffers.copy(
        host_buffer_, copy_buffer.src_buffer, Span<VkBufferCopy>(&copy_buffer.region, 1));
    command_buffers.submit();
  }
}

void VKStagingBuffer::free()
{
  host_buffer_.free();
}

}  // namespace blender::gpu
