/* SPDX-FileCopyrightText: 2023 Blender Authors
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
class VKDevice;

/** Command buffer to keep track of the life-time of a command buffer. */
class VKCommandBuffer : NonCopyable, NonMovable {
  /**
   * Not owning handle to the command pool that created this command buffer. The command pool is
   * owned by #VKCommandBuffers.
   */
  VkCommandPool vk_command_pool_ = VK_NULL_HANDLE;
  VkCommandBuffer vk_command_buffer_ = VK_NULL_HANDLE;

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
    /**
     * Current stage of the command buffer to keep track of inconsistencies & incorrect usage.
     */
    Stage stage = Stage::Initial;

    /**
     * The number of command added to the command buffer since last submission.
     */
    uint64_t recorded_command_counts = 0;
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
  bool is_initialized() const;
  void init(VkCommandPool vk_command_pool, VkCommandBuffer vk_command_buffer);
  void begin_recording();
  void end_recording();
  void free();

  /**
   * Receive the vulkan handle of the command buffer.
   */
  VkCommandBuffer vk_command_buffer() const
  {
    return vk_command_buffer_;
  }

  bool has_recorded_commands() const
  {
    return state.recorded_command_counts != 0;
  }

  void command_recorded()
  {
    state.recorded_command_counts++;
  }

  void commands_submitted();
};

}  // namespace blender::gpu
