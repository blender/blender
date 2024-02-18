/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_command_buffers.hh"
#include "vk_backend.hh"
#include "vk_device.hh"
#include "vk_framebuffer.hh"
#include "vk_memory.hh"
#include "vk_pipeline.hh"
#include "vk_storage_buffer.hh"
#include "vk_texture.hh"
#include "vk_vertex_buffer.hh"

#include "BLI_assert.h"

namespace blender::gpu {

VKCommandBuffers::~VKCommandBuffers()
{
  command_buffer_get(Type::DataTransferCompute).free();
  command_buffer_get(Type::Graphics).free();

  VK_ALLOCATION_CALLBACKS;
  const VKDevice &device = VKBackend::get().device_get();

  if (vk_command_pool_ != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device.device_get(), vk_command_pool_, vk_allocation_callbacks);
    vk_command_pool_ = VK_NULL_HANDLE;
  }
}

void VKCommandBuffers::init(const VKDevice &device)
{
  if (initialized_) {
    return;
  }
  initialized_ = true;

  /* When a the last GHOST context is destroyed the device is deallocate. A moment later the GPU
   * context is destroyed. The first step is to activate it. Activating would retrieve the device
   * from GHOST which in that case is a #VK_NULL_HANDLE. */
  if (!device.is_initialized()) {
    return;
  }
  init_command_pool(device);
  init_command_buffers(device);
  submission_id_.reset();
}

static void init_command_buffer(VKCommandBuffer &command_buffer,
                                VkCommandPool vk_command_pool,
                                VkCommandBuffer vk_command_buffer,
                                const char *name)
{
  command_buffer.init(vk_command_pool, vk_command_buffer);
  command_buffer.begin_recording();
  debug::object_label(vk_command_buffer, name);
}

void VKCommandBuffers::init_command_pool(const VKDevice &device)
{
  BLI_assert(vk_command_pool_ == VK_NULL_HANDLE);

  VK_ALLOCATION_CALLBACKS;
  VkCommandPoolCreateInfo command_pool_info = {};
  command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  command_pool_info.queueFamilyIndex = device.queue_family_get();

  vkCreateCommandPool(
      device.device_get(), &command_pool_info, vk_allocation_callbacks, &vk_command_pool_);
}

void VKCommandBuffers::init_command_buffers(const VKDevice &device)
{
  BLI_assert(vk_command_pool_ != VK_NULL_HANDLE);
  VkCommandBuffer vk_command_buffers[uint32_t(Type::Max)] = {VK_NULL_HANDLE};
  VkCommandBufferAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = vk_command_pool_;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandBufferCount = uint32_t(Type::Max);
  vkAllocateCommandBuffers(device.device_get(), &alloc_info, vk_command_buffers);

  init_command_buffer(command_buffer_get(Type::DataTransferCompute),
                      vk_command_pool_,
                      vk_command_buffers[int(Type::DataTransferCompute)],
                      "Data Transfer Compute Command Buffer");
  init_command_buffer(command_buffer_get(Type::Graphics),
                      vk_command_pool_,
                      vk_command_buffers[int(Type::Graphics)],
                      "Graphics Command Buffer");
}

void VKCommandBuffers::submit_command_buffers(VKDevice &device,
                                              MutableSpan<VKCommandBuffer *> command_buffers)
{
  VKTimelineSemaphore &timeline_semaphore = device.timeline_semaphore_get();
  VkSemaphore timeline_handle = timeline_semaphore.vk_handle();
  VKTimelineSemaphore::Value wait_value = timeline_semaphore.value_get();
  last_signal_value_ = timeline_semaphore.value_increase();

  BLI_assert(ELEM(command_buffers.size(), 1, 2));
  VkCommandBuffer handles[2];
  int num_command_buffers = 0;

  for (VKCommandBuffer *command_buffer : command_buffers) {
    command_buffer->end_recording();
    handles[num_command_buffers++] = command_buffer->vk_command_buffer();
  }

  VkTimelineSemaphoreSubmitInfo timelineInfo;
  timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
  timelineInfo.pNext = nullptr;
  timelineInfo.waitSemaphoreValueCount = 1;
  timelineInfo.pWaitSemaphoreValues = wait_value;
  timelineInfo.signalSemaphoreValueCount = 1;
  timelineInfo.pSignalSemaphoreValues = last_signal_value_;
  VkPipelineStageFlags wait_stages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  VkSubmitInfo submit_info = {};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = num_command_buffers;
  submit_info.pCommandBuffers = handles;
  submit_info.pNext = &timelineInfo;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &timeline_handle;
  submit_info.pWaitDstStageMask = &wait_stages;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &timeline_handle;

  vkQueueSubmit(device.queue_get(), 1, &submit_info, VK_NULL_HANDLE);
  finish();

  for (VKCommandBuffer *command_buffer : command_buffers) {
    command_buffer->commands_submitted();
    command_buffer->begin_recording();
  }
}

void VKCommandBuffers::submit()
{
  VKDevice &device = VKBackend::get().device_get();
  VKCommandBuffer &data_transfer_compute = command_buffer_get(Type::DataTransferCompute);
  VKCommandBuffer &graphics = command_buffer_get(Type::Graphics);

  const bool has_data_transfer_compute_work = data_transfer_compute.has_recorded_commands();
  const bool has_graphics_work = graphics.has_recorded_commands();

  VKCommandBuffer *command_buffers[2] = {nullptr, nullptr};
  int command_buffer_index = 0;

  if (has_data_transfer_compute_work) {
    command_buffers[command_buffer_index++] = &data_transfer_compute;
  }

  if (has_graphics_work) {
    VKFrameBuffer *framebuffer = framebuffer_;
    end_render_pass(*framebuffer);
    command_buffers[command_buffer_index++] = &graphics;
    submit_command_buffers(device,
                           MutableSpan<VKCommandBuffer *>(command_buffers, command_buffer_index));
    begin_render_pass(*framebuffer);
  }
  else if (has_data_transfer_compute_work) {
    submit_command_buffers(device,
                           MutableSpan<VKCommandBuffer *>(command_buffers, command_buffer_index));
  }
}

void VKCommandBuffers::finish()
{
  VKDevice &device = VKBackend::get().device_get();
  VKTimelineSemaphore &timeline_semaphore = device.timeline_semaphore_get();
  timeline_semaphore.wait(device, last_signal_value_);
  submission_id_.next();
}

void VKCommandBuffers::ensure_no_draw_commands()
{
  if (command_buffer_get(Type::Graphics).has_recorded_commands()) {
    submit();
  }
}

void VKCommandBuffers::validate_framebuffer_not_exists()
{
  BLI_assert_msg(framebuffer_ == nullptr && framebuffer_bound_ == false,
                 "State error: expected no framebuffer being tracked.");
}

void VKCommandBuffers::validate_framebuffer_exists()
{
  BLI_assert_msg(framebuffer_, "State error: expected framebuffer being tracked.");
}

void VKCommandBuffers::ensure_active_framebuffer()
{
  BLI_assert(framebuffer_);
  if (framebuffer_ && !framebuffer_bound_) {
    VkRenderPassBeginInfo render_pass_begin_info = {};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    framebuffer_->vk_render_pass_ensure();
    render_pass_begin_info.renderPass = framebuffer_->vk_render_pass_get();
    render_pass_begin_info.framebuffer = framebuffer_->vk_framebuffer_get();
    render_pass_begin_info.renderArea = framebuffer_->vk_render_areas_get()[0];
    /* We don't use clear ops, but vulkan wants to have at least one. */
    VkClearValue clear_value = {};
    render_pass_begin_info.clearValueCount = 1;
    render_pass_begin_info.pClearValues = &clear_value;

    VKCommandBuffer &command_buffer = command_buffer_get(Type::Graphics);
    vkCmdBeginRenderPass(
        command_buffer.vk_command_buffer(), &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    framebuffer_bound_ = true;
  }
}

/**
 * \name Vulkan commands
 * \{
 */

void VKCommandBuffers::bind(const VKPipeline &vk_pipeline, VkPipelineBindPoint bind_point)
{
  Type type;
  if (bind_point == VK_PIPELINE_BIND_POINT_COMPUTE) {
    ensure_no_draw_commands();
    type = Type::DataTransferCompute;
  }
  else if (bind_point == VK_PIPELINE_BIND_POINT_GRAPHICS) {
    type = Type::Graphics;
  }
  else {
    BLI_assert_unreachable();
    return;
  }

  VKCommandBuffer &command_buffer = command_buffer_get(type);
  vkCmdBindPipeline(command_buffer.vk_command_buffer(), bind_point, vk_pipeline.vk_handle());
  command_buffer.command_recorded();
}

void VKCommandBuffers::bind(const VKDescriptorSet &descriptor_set,
                            const VkPipelineLayout vk_pipeline_layout,
                            VkPipelineBindPoint bind_point)
{
  Type type;
  if (bind_point == VK_PIPELINE_BIND_POINT_COMPUTE) {
    ensure_no_draw_commands();
    type = Type::DataTransferCompute;
  }
  else if (bind_point == VK_PIPELINE_BIND_POINT_GRAPHICS) {
    type = Type::Graphics;
  }
  else {
    BLI_assert_unreachable();
    return;
  }

  VKCommandBuffer &command_buffer = command_buffer_get(type);
  VkDescriptorSet vk_descriptor_set = descriptor_set.vk_handle();
  vkCmdBindDescriptorSets(command_buffer.vk_command_buffer(),
                          bind_point,
                          vk_pipeline_layout,
                          0,
                          1,
                          &vk_descriptor_set,
                          0,
                          nullptr);
  command_buffer.command_recorded();
}

void VKCommandBuffers::bind(const uint32_t binding,
                            const VKVertexBuffer &vertex_buffer,
                            const VkDeviceSize offset)
{
  bind(binding, vertex_buffer.vk_handle(), offset);
}

void VKCommandBuffers::bind(const uint32_t binding, const VKBufferWithOffset &vertex_buffer)
{
  bind(binding, vertex_buffer.buffer.vk_handle(), vertex_buffer.offset);
}

void VKCommandBuffers::bind(const uint32_t binding,
                            const VkBuffer &vk_vertex_buffer,
                            const VkDeviceSize offset)
{
  VKCommandBuffer &command_buffer = command_buffer_get(Type::Graphics);

  validate_framebuffer_exists();
  ensure_active_framebuffer();
  vkCmdBindVertexBuffers(
      command_buffer.vk_command_buffer(), binding, 1, &vk_vertex_buffer, &offset);
  command_buffer.command_recorded();
}

void VKCommandBuffers::bind(const VKBuffer &index_buffer, VkIndexType index_type)
{
  VKCommandBuffer &command_buffer = command_buffer_get(Type::Graphics);

  validate_framebuffer_exists();
  ensure_active_framebuffer();
  vkCmdBindIndexBuffer(
      command_buffer.vk_command_buffer(), index_buffer.vk_handle(), 0, index_type);
  command_buffer.command_recorded();
}

void VKCommandBuffers::begin_render_pass(VKFrameBuffer &framebuffer)
{
  BLI_assert(framebuffer_ == nullptr);
  framebuffer_ = &framebuffer;
  framebuffer_bound_ = false;
}

void VKCommandBuffers::ensure_no_active_framebuffer()
{
  if (framebuffer_ && framebuffer_bound_) {
    VKCommandBuffer &command_buffer = command_buffer_get(Type::Graphics);
    vkCmdEndRenderPass(command_buffer.vk_command_buffer());
    command_buffer.command_recorded();
    framebuffer_bound_ = false;
  }
}

void VKCommandBuffers::end_render_pass(const VKFrameBuffer &framebuffer)
{
  UNUSED_VARS_NDEBUG(framebuffer);
  BLI_assert(framebuffer_ == nullptr || framebuffer_ == &framebuffer);
  ensure_no_active_framebuffer();
  framebuffer_ = nullptr;
}

void VKCommandBuffers::push_constants(const VKPushConstants &push_constants,
                                      const VkPipelineLayout vk_pipeline_layout,
                                      const VkShaderStageFlags vk_shader_stages)
{
  Type type;
  if (vk_shader_stages == VK_SHADER_STAGE_COMPUTE_BIT) {
    ensure_no_draw_commands();
    type = Type::DataTransferCompute;
  }
  else {
    type = Type::Graphics;
  }

  VKCommandBuffer &command_buffer = command_buffer_get(type);
  BLI_assert(push_constants.layout_get().storage_type_get() ==
             VKPushConstants::StorageType::PUSH_CONSTANTS);
  vkCmdPushConstants(command_buffer.vk_command_buffer(),
                     vk_pipeline_layout,
                     vk_shader_stages,
                     push_constants.offset(),
                     push_constants.layout_get().size_in_bytes(),
                     push_constants.data());
  command_buffer.command_recorded();
}

void VKCommandBuffers::dispatch(int groups_x_len, int groups_y_len, int groups_z_len)
{
  ensure_no_draw_commands();

  VKCommandBuffer &command_buffer = command_buffer_get(Type::DataTransferCompute);
  vkCmdDispatch(command_buffer.vk_command_buffer(), groups_x_len, groups_y_len, groups_z_len);
  command_buffer.command_recorded();
}

void VKCommandBuffers::dispatch(VKStorageBuffer &command_storage_buffer)
{
  ensure_no_draw_commands();

  VKCommandBuffer &command_buffer = command_buffer_get(Type::DataTransferCompute);
  vkCmdDispatchIndirect(command_buffer.vk_command_buffer(), command_storage_buffer.vk_handle(), 0);
  command_buffer.command_recorded();
}

void VKCommandBuffers::copy(VKBuffer &dst_buffer,
                            VKTexture &src_texture,
                            Span<VkBufferImageCopy> regions)
{
  VKCommandBuffer &command_buffer = command_buffer_get(Type::DataTransferCompute);
  vkCmdCopyImageToBuffer(command_buffer.vk_command_buffer(),
                         src_texture.vk_image_handle(),
                         src_texture.current_layout_get(),
                         dst_buffer.vk_handle(),
                         regions.size(),
                         regions.data());
  command_buffer.command_recorded();
}

void VKCommandBuffers::copy(VKTexture &dst_texture,
                            VKBuffer &src_buffer,
                            Span<VkBufferImageCopy> regions)
{
  VKCommandBuffer &command_buffer = command_buffer_get(Type::DataTransferCompute);
  vkCmdCopyBufferToImage(command_buffer.vk_command_buffer(),
                         src_buffer.vk_handle(),
                         dst_texture.vk_image_handle(),
                         dst_texture.current_layout_get(),
                         regions.size(),
                         regions.data());
  command_buffer.command_recorded();
}

void VKCommandBuffers::copy(VKTexture &dst_texture,
                            VKTexture &src_texture,
                            Span<VkImageCopy> regions)
{
  VKCommandBuffer &command_buffer = command_buffer_get(Type::DataTransferCompute);
  vkCmdCopyImage(command_buffer.vk_command_buffer(),
                 src_texture.vk_image_handle(),
                 src_texture.current_layout_get(),
                 dst_texture.vk_image_handle(),
                 dst_texture.current_layout_get(),
                 regions.size(),
                 regions.data());
  command_buffer.command_recorded();
}

void VKCommandBuffers::copy(const VKBuffer &dst_buffer,
                            VkBuffer src_buffer,
                            Span<VkBufferCopy> regions)
{
  VKCommandBuffer &command_buffer = command_buffer_get(Type::DataTransferCompute);
  vkCmdCopyBuffer(command_buffer.vk_command_buffer(),
                  src_buffer,
                  dst_buffer.vk_handle(),
                  regions.size(),
                  regions.data());
  command_buffer.command_recorded();
}

void VKCommandBuffers::blit(VKTexture &dst_texture,
                            VKTexture &src_texture,
                            Span<VkImageBlit> regions)
{
  blit(dst_texture,
       dst_texture.current_layout_get(),
       src_texture,
       src_texture.current_layout_get(),
       regions);
}

void VKCommandBuffers::blit(VKTexture &dst_texture,
                            VkImageLayout dst_layout,
                            VKTexture &src_texture,
                            VkImageLayout src_layout,
                            Span<VkImageBlit> regions)
{
  VKCommandBuffer &command_buffer = command_buffer_get(Type::DataTransferCompute);
  vkCmdBlitImage(command_buffer.vk_command_buffer(),
                 src_texture.vk_image_handle(),
                 src_layout,
                 dst_texture.vk_image_handle(),
                 dst_layout,
                 regions.size(),
                 regions.data(),
                 VK_FILTER_NEAREST);
  command_buffer.command_recorded();
}

void VKCommandBuffers::pipeline_barrier(const VkPipelineStageFlags src_stages,
                                        const VkPipelineStageFlags dst_stages,
                                        Span<VkImageMemoryBarrier> image_memory_barriers)
{
  VKCommandBuffer &command_buffer = command_buffer_get(Type::DataTransferCompute);
  vkCmdPipelineBarrier(command_buffer.vk_command_buffer(),
                       src_stages,
                       dst_stages,
                       VK_DEPENDENCY_BY_REGION_BIT,
                       0,
                       nullptr,
                       0,
                       nullptr,
                       image_memory_barriers.size(),
                       image_memory_barriers.data());
  command_buffer.command_recorded();
}

void VKCommandBuffers::clear(VkImage vk_image,
                             VkImageLayout vk_image_layout,
                             const VkClearColorValue &vk_clear_color,
                             Span<VkImageSubresourceRange> ranges)
{
  VKCommandBuffer &command_buffer = command_buffer_get(Type::DataTransferCompute);
  vkCmdClearColorImage(command_buffer.vk_command_buffer(),
                       vk_image,
                       vk_image_layout,
                       &vk_clear_color,
                       ranges.size(),
                       ranges.data());
  command_buffer.command_recorded();
}

void VKCommandBuffers::clear(VkImage vk_image,
                             VkImageLayout vk_image_layout,
                             const VkClearDepthStencilValue &vk_clear_depth_stencil,
                             Span<VkImageSubresourceRange> ranges)
{
  VKCommandBuffer &command_buffer = command_buffer_get(Type::DataTransferCompute);
  vkCmdClearDepthStencilImage(command_buffer.vk_command_buffer(),
                              vk_image,
                              vk_image_layout,
                              &vk_clear_depth_stencil,
                              ranges.size(),
                              ranges.data());
  command_buffer.command_recorded();
}

void VKCommandBuffers::clear(Span<VkClearAttachment> attachments, Span<VkClearRect> areas)
{
  VKCommandBuffer &command_buffer = command_buffer_get(Type::Graphics);
  validate_framebuffer_exists();
  ensure_active_framebuffer();
  vkCmdClearAttachments(command_buffer.vk_command_buffer(),
                        attachments.size(),
                        attachments.data(),
                        areas.size(),
                        areas.data());
  command_buffer.command_recorded();
}

void VKCommandBuffers::fill(VKBuffer &buffer, uint32_t clear_data)
{
  VKCommandBuffer &command_buffer = command_buffer_get(Type::DataTransferCompute);
  vkCmdFillBuffer(command_buffer.vk_command_buffer(),
                  buffer.vk_handle(),
                  0,
                  buffer.size_in_bytes(),
                  clear_data);
  command_buffer.command_recorded();
}

void VKCommandBuffers::draw(int v_first, int v_count, int i_first, int i_count)
{
  validate_framebuffer_exists();
  ensure_active_framebuffer();

  VKCommandBuffer &command_buffer = command_buffer_get(Type::Graphics);
  vkCmdDraw(command_buffer.vk_command_buffer(), v_count, i_count, v_first, i_first);
  command_buffer.command_recorded();
}

void VKCommandBuffers::draw_indexed(
    int index_count, int instance_count, int first_index, int vertex_offset, int first_instance)
{
  validate_framebuffer_exists();
  ensure_active_framebuffer();

  VKCommandBuffer &command_buffer = command_buffer_get(Type::Graphics);
  vkCmdDrawIndexed(command_buffer.vk_command_buffer(),
                   index_count,
                   instance_count,
                   first_index,
                   vertex_offset,
                   first_instance);
  command_buffer.command_recorded();
}

void VKCommandBuffers::draw_indirect(const VkBuffer buffer,
                                     const VkDeviceSize offset,
                                     const uint32_t draw_count,
                                     const uint32_t stride)
{
  validate_framebuffer_exists();
  ensure_active_framebuffer();

  VKCommandBuffer &command_buffer = command_buffer_get(Type::Graphics);
  vkCmdDrawIndirect(command_buffer.vk_command_buffer(), buffer, offset, draw_count, stride);
  command_buffer.command_recorded();
}

void VKCommandBuffers::draw_indexed_indirect(const VkBuffer buffer,
                                             const VkDeviceSize offset,
                                             const uint32_t draw_count,
                                             const uint32_t stride)
{
  validate_framebuffer_exists();
  ensure_active_framebuffer();

  VKCommandBuffer &command_buffer = command_buffer_get(Type::Graphics);
  vkCmdDrawIndexedIndirect(command_buffer.vk_command_buffer(), buffer, offset, draw_count, stride);
  command_buffer.command_recorded();
}

/** \} */

}  // namespace blender::gpu
