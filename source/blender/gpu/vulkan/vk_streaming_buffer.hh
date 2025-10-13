/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "render_graph/vk_render_graph.hh"
#include "vk_common.hh"
#include "vk_staging_buffer.hh"

namespace blender::gpu {
class VKBuffer;
class VKContext;

/**
 * Streaming buffer to improve performance of GPU_USAGE_STREAM.
 *
 * GPU_USAGE_STREAM is used for buffers that are uploaded once, and used a few times before being
 * rewritten. This class improves the performance by buffering the data in a single host transfer
 * buffer. This reduces barriers and more rendering can be performed between data transfers.
 */
class VKStreamingBuffer {
  /** Current host buffer storing the data to be uploaded. */
  std::optional<std::unique_ptr<VKBuffer>> host_buffer_;
  /** Minimum alignment for streaming. Needs to be set to
   * `VkPhysicalDeviceLimits.min*OffsetAlignment` */
  VkDeviceSize min_offset_alignment_;
  /** Device buffer that is being updated. */
  VkBuffer vk_buffer_dst_;
  /** Size of 'vk_buffer_dst_' */
  VkDeviceSize vk_buffer_size_;
  /**Current offset in the host buffer where new data will be stored. */
  VkDeviceSize offset_ = 0;
  /**
   * Render graph node handle for the copy of the host bufer to vk_buffer_dst_. Used to update the
   * previous added copy buffer node.
   */
  render_graph::NodeHandle copy_buffer_handle_ = 0;

 public:
  VKStreamingBuffer(VKBuffer &buffer, VkDeviceSize min_offset_alligment);
  ~VKStreamingBuffer();

  /**
   * Add 'data_size' bytes from 'data' to the streaming buffer. Returns the offset in the device
   * buffer where the data is stored.
   */
  VkDeviceSize update(VKContext &context, const void *data, size_t data_size);

  VkBuffer vk_buffer_dst()
  {
    return vk_buffer_dst_;
  }
};
}  // namespace blender::gpu
