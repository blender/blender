/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "vk_common.hh"
#include "vk_pipeline.hh"

namespace blender::gpu {
class VKBuffer;
class VKTexture;

/** Command buffer to keep track of the life-time of a command buffer.*/
class VKCommandBuffer : NonCopyable, NonMovable {
  /** None owning handle to the command buffer and device. Handle is owned by `GHOST_ContextVK`.*/
  VkDevice vk_device_ = VK_NULL_HANDLE;
  VkCommandBuffer vk_command_buffer_ = VK_NULL_HANDLE;
  VkQueue vk_queue_ = VK_NULL_HANDLE;

  /** Owning handles */
  VkFence vk_fence_ = VK_NULL_HANDLE;

 public:
  virtual ~VKCommandBuffer();
  void init(const VkDevice vk_device, const VkQueue vk_queue, VkCommandBuffer vk_command_buffer);
  void begin_recording();
  void end_recording();
  void bind(const VKPipeline &vk_pipeline, VkPipelineBindPoint bind_point);
  void bind(const VKDescriptorSet &descriptor_set,
            const VkPipelineLayout vk_pipeline_layout,
            VkPipelineBindPoint bind_point);
  void dispatch(int groups_x_len, int groups_y_len, int groups_z_len);
  /* Copy the contents of a texture mip level to the dst buffer.*/
  void copy(VKBuffer &dst_buffer, VKTexture &src_texture, Span<VkBufferImageCopy> regions);
  void pipeline_barrier(VkPipelineStageFlags source_stages,
                        VkPipelineStageFlags destination_stages);
  void pipeline_barrier(Span<VkImageMemoryBarrier> image_memory_barriers);

  /**
   * Stop recording commands, encode + send the recordings to Vulkan, wait for the until the
   * commands have been executed and start the command buffer to accept recordings again.
   */
  void submit();

 private:
  void encode_recorded_commands();
  void submit_encoded_commands();
};

}  // namespace blender::gpu
