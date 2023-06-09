/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_command_buffer.hh"
#include "vk_buffer.hh"
#include "vk_context.hh"
#include "vk_framebuffer.hh"
#include "vk_index_buffer.hh"
#include "vk_memory.hh"
#include "vk_pipeline.hh"
#include "vk_texture.hh"
#include "vk_vertex_buffer.hh"

#include "BLI_assert.h"

namespace blender::gpu {

VKCommandBuffer::~VKCommandBuffer()
{
  if (vk_device_ != VK_NULL_HANDLE) {
    VK_ALLOCATION_CALLBACKS;
    vkDestroyFence(vk_device_, vk_fence_, vk_allocation_callbacks);
    vk_fence_ = VK_NULL_HANDLE;
  }
}

void VKCommandBuffer::init(const VkDevice vk_device,
                           const VkQueue vk_queue,
                           VkCommandBuffer vk_command_buffer)
{
  vk_device_ = vk_device;
  vk_queue_ = vk_queue;
  vk_command_buffer_ = vk_command_buffer;
  submission_id_.reset();
  state.stage = Stage::Initial;

  /* When a the last GHOST context is destroyed the device is deallocate. A moment later the GPU
   * context is destroyed. The first step is to activate it. Activating would retrieve the device
   * from GHOST which in that case is a #VK_NULL_HANDLE. */
  if (vk_device == VK_NULL_HANDLE) {
    return;
  }

  if (vk_fence_ == VK_NULL_HANDLE) {
    VK_ALLOCATION_CALLBACKS;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(vk_device_, &fenceInfo, vk_allocation_callbacks, &vk_fence_);
  }
  else {
    vkResetFences(vk_device_, 1, &vk_fence_);
  }
}

void VKCommandBuffer::begin_recording()
{
  if (is_in_stage(Stage::Submitted)) {
    vkWaitForFences(vk_device_, 1, &vk_fence_, VK_TRUE, FenceTimeout);
    vkResetFences(vk_device_, 1, &vk_fence_);
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
}

void VKCommandBuffer::end_recording()
{
  ensure_no_active_framebuffer();
  vkEndCommandBuffer(vk_command_buffer_);
  stage_transfer(Stage::Recording, Stage::BetweenRecordingAndSubmitting);
}

void VKCommandBuffer::bind(const VKPipeline &pipeline, VkPipelineBindPoint bind_point)
{
  vkCmdBindPipeline(vk_command_buffer_, bind_point, pipeline.vk_handle());
}

void VKCommandBuffer::bind(const VKDescriptorSet &descriptor_set,
                           const VkPipelineLayout vk_pipeline_layout,
                           VkPipelineBindPoint bind_point)
{
  VkDescriptorSet vk_descriptor_set = descriptor_set.vk_handle();
  vkCmdBindDescriptorSets(
      vk_command_buffer_, bind_point, vk_pipeline_layout, 0, 1, &vk_descriptor_set, 0, 0);
}

void VKCommandBuffer::bind(const uint32_t binding,
                           const VKVertexBuffer &vertex_buffer,
                           const VkDeviceSize offset)
{
  bind(binding, vertex_buffer.vk_handle(), offset);
}

void VKCommandBuffer::bind(const uint32_t binding, const VKBufferWithOffset &vertex_buffer)
{
  bind(binding, vertex_buffer.buffer.vk_handle(), vertex_buffer.offset);
}

void VKCommandBuffer::bind(const uint32_t binding,
                           const VkBuffer &vk_vertex_buffer,
                           const VkDeviceSize offset)
{
  validate_framebuffer_exists();
  ensure_active_framebuffer();
  vkCmdBindVertexBuffers(vk_command_buffer_, binding, 1, &vk_vertex_buffer, &offset);
}

void VKCommandBuffer::bind(const VKBufferWithOffset &index_buffer, VkIndexType index_type)
{
  validate_framebuffer_exists();
  ensure_active_framebuffer();
  vkCmdBindIndexBuffer(
      vk_command_buffer_, index_buffer.buffer.vk_handle(), index_buffer.offset, index_type);
}

void VKCommandBuffer::begin_render_pass(const VKFrameBuffer &framebuffer)
{
  validate_framebuffer_not_exists();
  state.framebuffer_ = &framebuffer;
}

void VKCommandBuffer::end_render_pass(const VKFrameBuffer &framebuffer)
{
  UNUSED_VARS_NDEBUG(framebuffer);
  validate_framebuffer_exists();
  BLI_assert(state.framebuffer_ == &framebuffer);
  ensure_no_active_framebuffer();
  state.framebuffer_ = nullptr;
}

void VKCommandBuffer::push_constants(const VKPushConstants &push_constants,
                                     const VkPipelineLayout vk_pipeline_layout,
                                     const VkShaderStageFlags vk_shader_stages)
{
  BLI_assert(push_constants.layout_get().storage_type_get() ==
             VKPushConstants::StorageType::PUSH_CONSTANTS);
  vkCmdPushConstants(vk_command_buffer_,
                     vk_pipeline_layout,
                     vk_shader_stages,
                     push_constants.offset(),
                     push_constants.layout_get().size_in_bytes(),
                     push_constants.data());
}

void VKCommandBuffer::fill(VKBuffer &buffer, uint32_t clear_data)
{
  ensure_no_active_framebuffer();
  vkCmdFillBuffer(vk_command_buffer_, buffer.vk_handle(), 0, buffer.size_in_bytes(), clear_data);
}

void VKCommandBuffer::copy(VKBuffer &dst_buffer,
                           VKTexture &src_texture,
                           Span<VkBufferImageCopy> regions)
{
  ensure_no_active_framebuffer();
  vkCmdCopyImageToBuffer(vk_command_buffer_,
                         src_texture.vk_image_handle(),
                         src_texture.current_layout_get(),
                         dst_buffer.vk_handle(),
                         regions.size(),
                         regions.data());
}

void VKCommandBuffer::copy(VKTexture &dst_texture,
                           VKBuffer &src_buffer,
                           Span<VkBufferImageCopy> regions)
{
  ensure_no_active_framebuffer();
  vkCmdCopyBufferToImage(vk_command_buffer_,
                         src_buffer.vk_handle(),
                         dst_texture.vk_image_handle(),
                         dst_texture.current_layout_get(),
                         regions.size(),
                         regions.data());
}

void VKCommandBuffer::copy(VKTexture &dst_texture,
                           VKTexture &src_texture,
                           Span<VkImageCopy> regions)
{
  ensure_no_active_framebuffer();
  vkCmdCopyImage(vk_command_buffer_,
                 src_texture.vk_image_handle(),
                 src_texture.current_layout_get(),
                 dst_texture.vk_image_handle(),
                 dst_texture.current_layout_get(),
                 regions.size(),
                 regions.data());
}

void VKCommandBuffer::blit(VKTexture &dst_texture,
                           VKTexture &src_texture,
                           Span<VkImageBlit> regions)
{
  blit(dst_texture,
       dst_texture.current_layout_get(),
       src_texture,
       src_texture.current_layout_get(),
       regions);
}

void VKCommandBuffer::blit(VKTexture &dst_texture,
                           VkImageLayout dst_layout,
                           VKTexture &src_texture,
                           VkImageLayout src_layout,
                           Span<VkImageBlit> regions)
{
  ensure_no_active_framebuffer();
  vkCmdBlitImage(vk_command_buffer_,
                 src_texture.vk_image_handle(),
                 src_layout,
                 dst_texture.vk_image_handle(),
                 dst_layout,
                 regions.size(),
                 regions.data(),
                 VK_FILTER_NEAREST);
}

void VKCommandBuffer::clear(VkImage vk_image,
                            VkImageLayout vk_image_layout,
                            const VkClearColorValue &vk_clear_color,
                            Span<VkImageSubresourceRange> ranges)
{
  ensure_no_active_framebuffer();
  vkCmdClearColorImage(vk_command_buffer_,
                       vk_image,
                       vk_image_layout,
                       &vk_clear_color,
                       ranges.size(),
                       ranges.data());
}

void VKCommandBuffer::clear(Span<VkClearAttachment> attachments, Span<VkClearRect> areas)
{
  validate_framebuffer_exists();
  ensure_active_framebuffer();
  vkCmdClearAttachments(
      vk_command_buffer_, attachments.size(), attachments.data(), areas.size(), areas.data());
}

void VKCommandBuffer::draw(int v_first, int v_count, int i_first, int i_count)
{
  validate_framebuffer_exists();
  ensure_active_framebuffer();
  vkCmdDraw(vk_command_buffer_, v_count, i_count, v_first, i_first);
  state.draw_counts++;
}

void VKCommandBuffer::draw(
    int index_count, int instance_count, int first_index, int vertex_offset, int first_instance)
{
  validate_framebuffer_exists();
  ensure_active_framebuffer();
  vkCmdDrawIndexed(
      vk_command_buffer_, index_count, instance_count, first_index, vertex_offset, first_instance);
  state.draw_counts++;
}

void VKCommandBuffer::pipeline_barrier(VkPipelineStageFlags source_stages,
                                       VkPipelineStageFlags destination_stages)
{
  if (state.framebuffer_) {
    ensure_active_framebuffer();
  }
  vkCmdPipelineBarrier(vk_command_buffer_,
                       source_stages,
                       destination_stages,
                       0,
                       0,
                       nullptr,
                       0,
                       nullptr,
                       0,
                       nullptr);
}

void VKCommandBuffer::pipeline_barrier(Span<VkImageMemoryBarrier> image_memory_barriers)
{
  ensure_no_active_framebuffer();
  vkCmdPipelineBarrier(vk_command_buffer_,
                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_DEPENDENCY_BY_REGION_BIT,
                       0,
                       nullptr,
                       0,
                       nullptr,
                       image_memory_barriers.size(),
                       image_memory_barriers.data());
}

void VKCommandBuffer::dispatch(int groups_x_len, int groups_y_len, int groups_z_len)
{
  ensure_no_active_framebuffer();
  vkCmdDispatch(vk_command_buffer_, groups_x_len, groups_y_len, groups_z_len);
}

void VKCommandBuffer::submit()
{
  ensure_no_active_framebuffer();
  end_recording();
  encode_recorded_commands();
  submit_encoded_commands();
  begin_recording();
}

void VKCommandBuffer::encode_recorded_commands()
{
  /* Intentionally not implemented. For the graphics pipeline we want to extract the
   * resources and its usages so we can encode multiple commands in the same command buffer with
   * the correct synchronizations. */
}

void VKCommandBuffer::submit_encoded_commands()
{
  VkSubmitInfo submit_info = {};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &vk_command_buffer_;

  vkQueueSubmit(vk_queue_, 1, &submit_info, vk_fence_);
  submission_id_.next();
  stage_transfer(Stage::BetweenRecordingAndSubmitting, Stage::Submitted);
}

/* -------------------------------------------------------------------- */
/** \name FrameBuffer/RenderPass state tracking
 * \{ */

void VKCommandBuffer::validate_framebuffer_not_exists()
{
  BLI_assert_msg(state.framebuffer_ == nullptr && state.framebuffer_active_ == false,
                 "State error: expected no framebuffer being tracked.");
}

void VKCommandBuffer::validate_framebuffer_exists()
{
  BLI_assert_msg(state.framebuffer_, "State error: expected framebuffer being tracked.");
}

void VKCommandBuffer::ensure_no_active_framebuffer()
{
  state.checks_++;
  if (state.framebuffer_ && state.framebuffer_active_) {
    vkCmdEndRenderPass(vk_command_buffer_);
    state.framebuffer_active_ = false;
    state.switches_++;
  }
}

void VKCommandBuffer::ensure_active_framebuffer()
{
  BLI_assert(state.framebuffer_);
  state.checks_++;
  if (!state.framebuffer_active_) {
    VkRenderPassBeginInfo render_pass_begin_info = {};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = state.framebuffer_->vk_render_pass_get();
    render_pass_begin_info.framebuffer = state.framebuffer_->vk_framebuffer_get();
    render_pass_begin_info.renderArea = state.framebuffer_->vk_render_area_get();
    /* We don't use clear ops, but vulkan wants to have at least one. */
    VkClearValue clear_value = {};
    render_pass_begin_info.clearValueCount = 1;
    render_pass_begin_info.pClearValues = &clear_value;

    vkCmdBeginRenderPass(vk_command_buffer_, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    state.framebuffer_active_ = true;
    state.switches_++;
  }
}

/** \} */

}  // namespace blender::gpu
