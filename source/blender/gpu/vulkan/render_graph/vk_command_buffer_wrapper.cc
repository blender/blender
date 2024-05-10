/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_command_buffer_wrapper.hh"
#include "vk_backend.hh"
#include "vk_memory.hh"

namespace blender::gpu::render_graph {
VKCommandBufferWrapper::VKCommandBufferWrapper()
{
  vk_command_pool_create_info_ = {};
  vk_command_pool_create_info_.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  vk_command_pool_create_info_.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  vk_command_pool_create_info_.queueFamilyIndex = 0;

  vk_command_buffer_allocate_info_ = {};
  vk_command_buffer_allocate_info_.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  vk_command_buffer_allocate_info_.commandPool = VK_NULL_HANDLE;
  vk_command_buffer_allocate_info_.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  vk_command_buffer_allocate_info_.commandBufferCount = 1;

  vk_command_buffer_begin_info_ = {};
  vk_command_buffer_begin_info_.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  vk_fence_create_info_ = {};
  vk_fence_create_info_.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  vk_fence_create_info_.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  vk_submit_info_ = {};
  vk_submit_info_.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  vk_submit_info_.waitSemaphoreCount = 0;
  vk_submit_info_.pWaitSemaphores = nullptr;
  vk_submit_info_.pWaitDstStageMask = nullptr;
  vk_submit_info_.commandBufferCount = 1;
  vk_submit_info_.pCommandBuffers = &vk_command_buffer_;
  vk_submit_info_.signalSemaphoreCount = 0;
  vk_submit_info_.pSignalSemaphores = nullptr;
}

VKCommandBufferWrapper::~VKCommandBufferWrapper()
{
  VK_ALLOCATION_CALLBACKS;
  VKDevice &device = VKBackend::get().device_get();

  if (vk_command_pool_ != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device.device_get(), vk_command_pool_, vk_allocation_callbacks);
    vk_command_pool_ = VK_NULL_HANDLE;
  }
  if (vk_fence_ != VK_NULL_HANDLE) {
    vkDestroyFence(device.device_get(), vk_fence_, vk_allocation_callbacks);
    vk_fence_ = VK_NULL_HANDLE;
  }
}

void VKCommandBufferWrapper::begin_recording()
{
  VK_ALLOCATION_CALLBACKS;
  VKDevice &device = VKBackend::get().device_get();
  if (vk_command_pool_ == VK_NULL_HANDLE) {
    vk_command_pool_create_info_.queueFamilyIndex = device.queue_family_get();
    vkCreateCommandPool(device.device_get(),
                        &vk_command_pool_create_info_,
                        vk_allocation_callbacks,
                        &vk_command_pool_);
    vk_command_buffer_allocate_info_.commandPool = vk_command_pool_;
    vk_command_pool_create_info_.queueFamilyIndex = 0;
  }
  if (vk_fence_ == VK_NULL_HANDLE) {
    vkCreateFence(
        device.device_get(), &vk_fence_create_info_, vk_allocation_callbacks, &vk_fence_);
  }
  BLI_assert(vk_command_buffer_ == VK_NULL_HANDLE);
  vkAllocateCommandBuffers(
      device.device_get(), &vk_command_buffer_allocate_info_, &vk_command_buffer_);

  vkBeginCommandBuffer(vk_command_buffer_, &vk_command_buffer_begin_info_);
}

void VKCommandBufferWrapper::end_recording()
{
  vkEndCommandBuffer(vk_command_buffer_);
}

void VKCommandBufferWrapper::submit_with_cpu_synchronization()
{
  VKDevice &device = VKBackend::get().device_get();
  vkResetFences(device.device_get(), 1, &vk_fence_);
  vkQueueSubmit(device.queue_get(), 1, &vk_submit_info_, vk_fence_);
  vk_command_buffer_ = VK_NULL_HANDLE;
}

void VKCommandBufferWrapper::wait_for_cpu_synchronization()
{
  VKDevice &device = VKBackend::get().device_get();
  while (vkWaitForFences(device.device_get(), 1, &vk_fence_, true, UINT64_MAX) == VK_TIMEOUT) {
  }
}

void VKCommandBufferWrapper::bind_pipeline(VkPipelineBindPoint pipeline_bind_point,
                                           VkPipeline pipeline)
{
  vkCmdBindPipeline(vk_command_buffer_, pipeline_bind_point, pipeline);
}

void VKCommandBufferWrapper::bind_descriptor_sets(VkPipelineBindPoint pipeline_bind_point,
                                                  VkPipelineLayout layout,
                                                  uint32_t first_set,
                                                  uint32_t descriptor_set_count,
                                                  const VkDescriptorSet *p_descriptor_sets,
                                                  uint32_t dynamic_offset_count,
                                                  const uint32_t *p_dynamic_offsets)
{
  vkCmdBindDescriptorSets(vk_command_buffer_,
                          pipeline_bind_point,
                          layout,
                          first_set,
                          descriptor_set_count,
                          p_descriptor_sets,
                          dynamic_offset_count,
                          p_dynamic_offsets);
}

void VKCommandBufferWrapper::bind_index_buffer(VkBuffer buffer,
                                               VkDeviceSize offset,
                                               VkIndexType index_type)
{
  vkCmdBindIndexBuffer(vk_command_buffer_, buffer, offset, index_type);
}

void VKCommandBufferWrapper::bind_vertex_buffers(uint32_t first_binding,
                                                 uint32_t binding_count,
                                                 const VkBuffer *p_buffers,
                                                 const VkDeviceSize *p_offsets)
{
  vkCmdBindVertexBuffers(vk_command_buffer_, first_binding, binding_count, p_buffers, p_offsets);
}

void VKCommandBufferWrapper::draw(uint32_t vertex_count,
                                  uint32_t instance_count,
                                  uint32_t first_vertex,
                                  uint32_t first_instance)
{
  vkCmdDraw(vk_command_buffer_, vertex_count, instance_count, first_vertex, first_instance);
}

void VKCommandBufferWrapper::draw_indexed(uint32_t index_count,
                                          uint32_t instance_count,
                                          uint32_t first_index,
                                          int32_t vertex_offset,
                                          uint32_t first_instance)
{
  vkCmdDrawIndexed(
      vk_command_buffer_, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void VKCommandBufferWrapper::draw_indirect(VkBuffer buffer,
                                           VkDeviceSize offset,
                                           uint32_t draw_count,
                                           uint32_t stride)
{
  vkCmdDrawIndirect(vk_command_buffer_, buffer, offset, draw_count, stride);
}

void VKCommandBufferWrapper::draw_indexed_indirect(VkBuffer buffer,
                                                   VkDeviceSize offset,
                                                   uint32_t draw_count,
                                                   uint32_t stride)
{
  vkCmdDrawIndexedIndirect(vk_command_buffer_, buffer, offset, draw_count, stride);
}

void VKCommandBufferWrapper::dispatch(uint32_t group_count_x,
                                      uint32_t group_count_y,
                                      uint32_t group_count_z)
{
  vkCmdDispatch(vk_command_buffer_, group_count_x, group_count_y, group_count_z);
}

void VKCommandBufferWrapper::dispatch_indirect(VkBuffer buffer, VkDeviceSize offset)
{
  vkCmdDispatchIndirect(vk_command_buffer_, buffer, offset);
}

void VKCommandBufferWrapper::copy_buffer(VkBuffer src_buffer,
                                         VkBuffer dst_buffer,
                                         uint32_t region_count,
                                         const VkBufferCopy *p_regions)
{
  vkCmdCopyBuffer(vk_command_buffer_, src_buffer, dst_buffer, region_count, p_regions);
}

void VKCommandBufferWrapper::copy_image(VkImage src_image,
                                        VkImageLayout src_image_layout,
                                        VkImage dst_image,
                                        VkImageLayout dst_image_layout,
                                        uint32_t region_count,
                                        const VkImageCopy *p_regions)
{
  vkCmdCopyImage(vk_command_buffer_,
                 src_image,
                 src_image_layout,
                 dst_image,
                 dst_image_layout,
                 region_count,
                 p_regions);
}

void VKCommandBufferWrapper::blit_image(VkImage src_image,
                                        VkImageLayout src_image_layout,
                                        VkImage dst_image,
                                        VkImageLayout dst_image_layout,
                                        uint32_t region_count,
                                        const VkImageBlit *p_regions,
                                        VkFilter filter)
{
  vkCmdBlitImage(vk_command_buffer_,
                 src_image,
                 src_image_layout,
                 dst_image,
                 dst_image_layout,
                 region_count,
                 p_regions,
                 filter);
}

void VKCommandBufferWrapper::copy_buffer_to_image(VkBuffer src_buffer,
                                                  VkImage dst_image,
                                                  VkImageLayout dst_image_layout,
                                                  uint32_t region_count,
                                                  const VkBufferImageCopy *p_regions)
{
  vkCmdCopyBufferToImage(
      vk_command_buffer_, src_buffer, dst_image, dst_image_layout, region_count, p_regions);
}

void VKCommandBufferWrapper::copy_image_to_buffer(VkImage src_image,
                                                  VkImageLayout src_image_layout,
                                                  VkBuffer dst_buffer,
                                                  uint32_t region_count,
                                                  const VkBufferImageCopy *p_regions)
{
  vkCmdCopyImageToBuffer(
      vk_command_buffer_, src_image, src_image_layout, dst_buffer, region_count, p_regions);
}

void VKCommandBufferWrapper::fill_buffer(VkBuffer dst_buffer,
                                         VkDeviceSize dst_offset,
                                         VkDeviceSize size,
                                         uint32_t data)
{
  vkCmdFillBuffer(vk_command_buffer_, dst_buffer, dst_offset, size, data);
}

void VKCommandBufferWrapper::clear_color_image(VkImage image,
                                               VkImageLayout image_layout,
                                               const VkClearColorValue *p_color,
                                               uint32_t range_count,
                                               const VkImageSubresourceRange *p_ranges)
{
  vkCmdClearColorImage(vk_command_buffer_, image, image_layout, p_color, range_count, p_ranges);
}
void VKCommandBufferWrapper::clear_depth_stencil_image(
    VkImage image,
    VkImageLayout image_layout,
    const VkClearDepthStencilValue *p_depth_stencil,
    uint32_t range_count,
    const VkImageSubresourceRange *p_ranges)
{
  vkCmdClearDepthStencilImage(
      vk_command_buffer_, image, image_layout, p_depth_stencil, range_count, p_ranges);
}

void VKCommandBufferWrapper::clear_attachments(uint32_t attachment_count,
                                               const VkClearAttachment *p_attachments,
                                               uint32_t rect_count,
                                               const VkClearRect *p_rects)
{
  vkCmdClearAttachments(vk_command_buffer_, attachment_count, p_attachments, rect_count, p_rects);
}

void VKCommandBufferWrapper::pipeline_barrier(
    VkPipelineStageFlags src_stage_mask,
    VkPipelineStageFlags dst_stage_mask,
    VkDependencyFlags dependency_flags,
    uint32_t memory_barrier_count,
    const VkMemoryBarrier *p_memory_barriers,
    uint32_t buffer_memory_barrier_count,
    const VkBufferMemoryBarrier *p_buffer_memory_barriers,
    uint32_t image_memory_barrier_count,
    const VkImageMemoryBarrier *p_image_memory_barriers)
{
  vkCmdPipelineBarrier(vk_command_buffer_,
                       src_stage_mask,
                       dst_stage_mask,
                       dependency_flags,
                       memory_barrier_count,
                       p_memory_barriers,
                       buffer_memory_barrier_count,
                       p_buffer_memory_barriers,
                       image_memory_barrier_count,
                       p_image_memory_barriers);
}

void VKCommandBufferWrapper::push_constants(VkPipelineLayout layout,
                                            VkShaderStageFlags stage_flags,
                                            uint32_t offset,
                                            uint32_t size,
                                            const void *p_values)
{
  vkCmdPushConstants(vk_command_buffer_, layout, stage_flags, offset, size, p_values);
}

void VKCommandBufferWrapper::begin_rendering(const VkRenderingInfo *p_rendering_info)
{
  const VKDevice &device = VKBackend::get().device_get();
  BLI_assert(device.functions.vkCmdBeginRendering);
  device.functions.vkCmdBeginRendering(vk_command_buffer_, p_rendering_info);
}

void VKCommandBufferWrapper::end_rendering()
{
  const VKDevice &device = VKBackend::get().device_get();
  BLI_assert(device.functions.vkCmdEndRendering);
  device.functions.vkCmdEndRendering(vk_command_buffer_);
}

}  // namespace blender::gpu::render_graph
