/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "vk_command_buffer.hh"
#include "vk_timeline_semaphore.hh"

namespace blender::gpu {
class VKFrameBuffer;
class VKStorageBuffer;
class VKBuffer;
class VKVertexBuffer;
class VKIndexBuffer;
class VKTexture;
class VKPushConstants;
struct VKBufferWithOffset;
class VKPipeline;
class VKDescriptorSet;

class VKCommandBuffers : public NonCopyable, NonMovable {
  VkCommandPool vk_command_pool_ = VK_NULL_HANDLE;
  enum class Type {
    DataTransferCompute = 0,
    Graphics = 1,
    Max = 2,
  };

  bool initialized_ = false;

  /**
   * Last submitted timeline value, what can be used to validate that all commands related
   * submitted by this command buffers have been finished.
   */
  VKTimelineSemaphore::Value last_signal_value_;

  /**
   * Active framebuffer for graphics command buffer.
   */
  VKFrameBuffer *framebuffer_ = nullptr;
  bool framebuffer_bound_ = false;

  /* TODO: General command buffer should not be used, but is added to help during the transition.
   */
  VKCommandBuffer buffers_[(int)Type::Max];
  VKSubmissionID submission_id_;

 public:
  ~VKCommandBuffers();

  void init(const VKDevice &device);

  /**
   * Have these command buffers already been initialized?
   */
  bool is_initialized() const
  {
    return initialized_;
  }

  void bind(const VKPipeline &vk_pipeline, VkPipelineBindPoint bind_point);
  void bind(const VKDescriptorSet &descriptor_set,
            const VkPipelineLayout vk_pipeline_layout,
            VkPipelineBindPoint bind_point);
  void bind(const uint32_t binding,
            const VKVertexBuffer &vertex_buffer,
            const VkDeviceSize offset);
  /* Bind the given buffer as a vertex buffer. */
  void bind(const uint32_t binding, const VKBufferWithOffset &vertex_buffer);
  void bind(const uint32_t binding, const VkBuffer &vk_vertex_buffer, const VkDeviceSize offset);
  /* Bind the given buffer as an index buffer. */
  void bind(const VKBuffer &index_buffer, VkIndexType index_type);

  void begin_render_pass(VKFrameBuffer &framebuffer);
  void end_render_pass(const VKFrameBuffer &framebuffer);

  /**
   * Add a push constant command to the command buffer.
   *
   * Only valid when the storage type of push_constants is StorageType::PUSH_CONSTANTS.
   */
  void push_constants(const VKPushConstants &push_constants,
                      const VkPipelineLayout vk_pipeline_layout,
                      const VkShaderStageFlags vk_shader_stages);
  void dispatch(int groups_x_len, int groups_y_len, int groups_z_len);
  void dispatch(VKStorageBuffer &command_buffer);
  /** Copy the contents of a texture MIP level to the dst buffer. */
  void copy(VKBuffer &dst_buffer, VKTexture &src_texture, Span<VkBufferImageCopy> regions);
  void copy(VKTexture &dst_texture, VKBuffer &src_buffer, Span<VkBufferImageCopy> regions);
  void copy(VKTexture &dst_texture, VKTexture &src_texture, Span<VkImageCopy> regions);
  void copy(const VKBuffer &dst_buffer, VkBuffer src_buffer, Span<VkBufferCopy> regions);
  void blit(VKTexture &dst_texture, VKTexture &src_texture, Span<VkImageBlit> regions);
  void blit(VKTexture &dst_texture,
            VkImageLayout dst_layout,
            VKTexture &src_texture,
            VkImageLayout src_layout,
            Span<VkImageBlit> regions);
  void pipeline_barrier(VkPipelineStageFlags source_stages,
                        VkPipelineStageFlags destination_stages,
                        Span<VkImageMemoryBarrier> image_memory_barriers);

  /**
   * Clear color image resource.
   */
  void clear(VkImage vk_image,
             VkImageLayout vk_image_layout,
             const VkClearColorValue &vk_clear_color,
             Span<VkImageSubresourceRange> ranges);

  /**
   * Clear depth/stencil aspect of an image resource.
   */
  void clear(VkImage vk_image,
             VkImageLayout vk_image_layout,
             const VkClearDepthStencilValue &vk_clear_color,
             Span<VkImageSubresourceRange> ranges);

  /**
   * Clear attachments of the active framebuffer.
   */
  void clear(Span<VkClearAttachment> attachments, Span<VkClearRect> areas);
  void fill(VKBuffer &buffer, uint32_t data);

  void draw(int v_first, int v_count, int i_first, int i_count);
  void draw_indexed(
      int index_count, int instance_count, int first_index, int vertex_offset, int first_instance);

  void draw_indirect(VkBuffer buffer, VkDeviceSize offset, uint32_t draw_count, uint32_t stride);
  void draw_indexed_indirect(VkBuffer buffer,
                             VkDeviceSize offset,
                             uint32_t draw_count,
                             uint32_t stride);

  void submit();
  void finish();

  const VKSubmissionID &submission_id_get() const
  {
    return submission_id_;
  }

 private:
  void init_command_pool(const VKDevice &device);
  void init_command_buffers(const VKDevice &device);

  void submit_command_buffers(VKDevice &device, MutableSpan<VKCommandBuffer *> command_buffers);

  VKCommandBuffer &command_buffer_get(Type type)
  {
    return buffers_[(int)type];
  }

  /**
   * Ensure that no draw_commands are scheduled.
   *
   * To ensure correct operation all draw commands should be flushed when adding a new compute
   * command.
   */
  void ensure_no_draw_commands();

  /**
   * Validate that there isn't a framebuffer being tracked (bound or not bound).
   *
   * Raises an assert in debug when a framebuffer is being tracked.
   */
  void validate_framebuffer_not_exists();

  /**
   * Validate that there is a framebuffer being tracked (bound or not bound).
   *
   * Raises an assert in debug when no framebuffer is being tracked.
   */
  void validate_framebuffer_exists();

  /**
   * Ensure that the tracked framebuffer is bound.
   */
  void ensure_active_framebuffer();
  /**
   * Ensure that the tracked framebuffer is bound.
   */
  void ensure_no_active_framebuffer();
};

}  // namespace blender::gpu
