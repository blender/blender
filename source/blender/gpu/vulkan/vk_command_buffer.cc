/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_command_buffer.hh"
#include "vk_backend.hh"

namespace blender::gpu {

VKCommandBuffer::~VKCommandBuffer()
{
  free();
}

void VKCommandBuffer::free()
{
  if (vk_command_buffer_ != VK_NULL_HANDLE) {
    VKDevice &device = VKBackend::get().device_get();
    vkFreeCommandBuffers(device.device_get(), vk_command_pool_, 1, &vk_command_buffer_);
    vk_command_buffer_ = VK_NULL_HANDLE;
  }
  vk_command_pool_ = VK_NULL_HANDLE;
}

bool VKCommandBuffer::is_initialized() const
{
  return vk_command_buffer_ != VK_NULL_HANDLE;
}

void VKCommandBuffer::init(const VkCommandPool vk_command_pool,
                           const VkCommandBuffer vk_command_buffer)
{
  if (is_initialized()) {
    return;
  }

  vk_command_pool_ = vk_command_pool;
  vk_command_buffer_ = vk_command_buffer;
  state.stage = Stage::Initial;
}

void VKCommandBuffer::begin_recording()
{
  if (is_in_stage(Stage::Submitted)) {
    stage_transfer(Stage::Submitted, Stage::Executed);
  }
  if (is_in_stage(Stage::Executed)) {
    vkResetCommandBuffer(vk_command_buffer_, 0);
    stage_transfer(Stage::Executed, Stage::Initial);
  }

  VkCommandBufferBeginInfo begin_info = {};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkBeginCommandBuffer(vk_command_buffer_, &begin_info);
  stage_transfer(Stage::Initial, Stage::Recording);
  state.recorded_command_counts = 0;
}

void VKCommandBuffer::end_recording()
{
  vkEndCommandBuffer(vk_command_buffer_);
  stage_transfer(Stage::Recording, Stage::BetweenRecordingAndSubmitting);
}

void VKCommandBuffer::commands_submitted()
{
  stage_transfer(Stage::BetweenRecordingAndSubmitting, Stage::Submitted);
}

}  // namespace blender::gpu
