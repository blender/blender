/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_streaming_buffer.hh"
#include "vk_buffer.hh"
#include "vk_context.hh"

namespace blender::gpu {
VKStreamingBuffer::VKStreamingBuffer(VKBuffer &buffer, VkDeviceSize min_offset_alignment)
    : min_offset_alignment_(min_offset_alignment),
      vk_buffer_dst_(buffer.vk_handle()),
      vk_buffer_size_(buffer.size_in_bytes())
{
}

VKStreamingBuffer::~VKStreamingBuffer()
{
  vk_buffer_dst_ = VK_NULL_HANDLE;
}

VkDeviceSize VKStreamingBuffer::update(VKContext &context, const void *data, size_t data_size)
{
  render_graph::VKRenderGraph &render_graph = context.render_graph();
  const bool allocate_new_buffer = !(host_buffer_.has_value() &&
                                     data_size <
                                         host_buffer_.value().get()->size_in_bytes() - offset_);
  if (allocate_new_buffer) {
    host_buffer_.emplace(std::make_unique<VKBuffer>());
    VKBuffer &host_buffer = *host_buffer_.value().get();
    host_buffer.create(vk_buffer_size_,
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VMA_MEMORY_USAGE_AUTO,
                       VMA_ALLOCATION_CREATE_MAPPED_BIT |
                           VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                       0.4f);
    offset_ = 0;

    render_graph::VKCopyBufferNode::CreateInfo copy_buffer = {
        host_buffer.vk_handle(), vk_buffer_dst(), {0, 0, 0}};
    copy_buffer_handle_ = render_graph.add_node(copy_buffer);
  }
  VKBuffer &host_buffer = *host_buffer_.value().get();

  VkDeviceSize start_offset = offset_;
  /* Advance the offset to the next possible offset considering the minimum allowed offset
   * alignment. */
  offset_ += data_size;
  if (min_offset_alignment_ > 1) {
    offset_ = ceil_to_multiple_ul(offset_, min_offset_alignment_);
  }

  memcpy(
      static_cast<void *>(static_cast<uint8_t *>(host_buffer.mapped_memory_get()) + start_offset),
      data,
      data_size);

  /* Increace the region size to copy to include the min offset alignment. */
  render_graph::VKCopyBufferNode::Data &copy_buffer_data = render_graph.get_node_data(
      copy_buffer_handle_);
  copy_buffer_data.region.size += offset_ - start_offset;
  return start_offset;
}

}  // namespace blender::gpu
