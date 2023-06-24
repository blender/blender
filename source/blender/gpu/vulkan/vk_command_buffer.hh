/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "vk_common.hh"
#include "vk_resource_tracker.hh"

#include "BLI_utility_mixins.hh"

namespace blender::gpu {
class VKBuffer;
struct VKBufferWithOffset;
class VKDescriptorSet;
class VKFrameBuffer;
class VKIndexBuffer;
class VKPipeline;
class VKPushConstants;
class VKStorageBuffer;
class VKTexture;
class VKVertexBuffer;

/** Command buffer to keep track of the life-time of a command buffer. */
class VKCommandBuffer : NonCopyable, NonMovable {
  /** Not owning handle to the command buffer and device. Handle is owned by `GHOST_ContextVK`. */
  VkDevice vk_device_ = VK_NULL_HANDLE;
  VkCommandBuffer vk_command_buffer_ = VK_NULL_HANDLE;
  VkQueue vk_queue_ = VK_NULL_HANDLE;

  /**
   * Timeout to use when waiting for fences in nanoseconds.
   *
   * Currently added as the fence will halt when there are no commands in the command buffer for
   * the second time. This should be solved and this timeout should be removed.
   */
  static constexpr uint64_t FenceTimeout = UINT64_MAX;
  /** Owning handles */
  VkFence vk_fence_ = VK_NULL_HANDLE;
  VKSubmissionID submission_id_;

 private:
  enum class Stage {
    Initial,
    Recording,
    BetweenRecordingAndSubmitting,
    Submitted,
    Executed,
  };
  /*
   * Some vulkan command require an active frame buffer. Others require no active frame-buffer. As
   * our current API does not provide a solution for this we need to keep track of the actual state
   * and do the changes when recording the next command.
   *
   * This is a temporary solution to get things rolling.
   * TODO: In a future solution we should decide the scope of a command buffer.
   *
   * - command buffer per draw command.
   * - minimize command buffers and track render passes.
   * - add custom encoder to also track resource usages.
   *
   * Currently I expect the custom encoder has to be done eventually. But want to keep postponing
   * the custom encoder for now to collect more use cases it should solve. (first pixel drawn on
   * screen).
   *
   * Some command can also be encoded in another way when encoded as a first command. For example
   * clearing a frame-buffer textures isn't allowed inside a render pass, but clearing the
   * frame-buffer textures via ops is allowed. When clearing a frame-buffer texture directly after
   * beginning a render pass could be re-encoded to do this in the same command.
   *
   * So for now we track the state and temporary switch to another state if the command requires
   * it.
   */
  struct {
    /* Reference to the last_framebuffer where begin_render_pass was called for. */
    const VKFrameBuffer *framebuffer_ = nullptr;
    /* Is last_framebuffer_ currently bound. Each call should ensure the correct state. */
    bool framebuffer_active_ = false;
    /* Amount of times a check has been requested. */
    uint64_t checks_ = 0;
    /* Amount of times a check required to change the render pass. */
    uint64_t switches_ = 0;

    /* Number of times a vkDraw command has been recorded. */
    uint64_t draw_counts = 0;

    /**
     * Current stage of the command buffer to keep track of inconsistencies & incorrect usage.
     */
    Stage stage = Stage::Initial;

  } state;
  bool is_in_stage(Stage stage)
  {
    return state.stage == stage;
  }
  void stage_set(Stage stage)
  {
    state.stage = stage;
  }
  std::string to_string(Stage stage)
  {
    switch (stage) {
      case Stage::Initial:
        return "INITIAL";
      case Stage::Recording:
        return "RECORDING";
      case Stage::BetweenRecordingAndSubmitting:
        return "BEFORE_SUBMIT";
      case Stage::Submitted:
        return "SUBMITTED";
      case Stage::Executed:
        return "EXECUTED";
    }
    return "UNKNOWN";
  }
  void stage_transfer(Stage stage_from, Stage stage_to)
  {
    BLI_assert(is_in_stage(stage_from));
    UNUSED_VARS_NDEBUG(stage_from);
#if 0
    printf(" *** Transfer stage from %s to %s\n",
           to_string(stage_from).c_str(),
           to_string(stage_to).c_str());
#endif
    stage_set(stage_to);
  }

 public:
  virtual ~VKCommandBuffer();
  void init(const VkDevice vk_device, const VkQueue vk_queue, VkCommandBuffer vk_command_buffer);
  void begin_recording();
  void end_recording();

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
  void bind(const VKBufferWithOffset &index_buffer, VkIndexType index_type);

  void begin_render_pass(const VKFrameBuffer &framebuffer);
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
  void blit(VKTexture &dst_texture, VKTexture &src_texture, Span<VkImageBlit> regions);
  void blit(VKTexture &dst_texture,
            VkImageLayout dst_layout,
            VKTexture &src_texture,
            VkImageLayout src_layout,
            Span<VkImageBlit> regions);
  void pipeline_barrier(VkPipelineStageFlags source_stages,
                        VkPipelineStageFlags destination_stages);
  void pipeline_barrier(Span<VkImageMemoryBarrier> image_memory_barriers);
  /**
   * Clear color image resource.
   */
  void clear(VkImage vk_image,
             VkImageLayout vk_image_layout,
             const VkClearColorValue &vk_clear_color,
             Span<VkImageSubresourceRange> ranges);

  /**
   * Clear attachments of the active framebuffer.
   */
  void clear(Span<VkClearAttachment> attachments, Span<VkClearRect> areas);
  void fill(VKBuffer &buffer, uint32_t data);

  void draw(int v_first, int v_count, int i_first, int i_count);
  void draw(
      int index_count, int instance_count, int first_index, int vertex_offset, int first_instance);

  /**
   * Stop recording commands, encode + send the recordings to Vulkan, wait for the until the
   * commands have been executed and start the command buffer to accept recordings again.
   */
  void submit();

  const VKSubmissionID &submission_id_get() const
  {
    return submission_id_;
  }

 private:
  void encode_recorded_commands();
  void submit_encoded_commands();

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
   * Ensure that there is no framebuffer being tracked or the tracked framebuffer isn't bound.
   */
  void ensure_no_active_framebuffer();

  /**
   * Ensure that the tracked framebuffer is bound.
   */
  void ensure_active_framebuffer();
};

}  // namespace blender::gpu
