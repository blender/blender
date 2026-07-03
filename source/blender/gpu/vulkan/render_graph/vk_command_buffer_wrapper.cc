/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_command_buffer_wrapper.hh"
#include "vk_backend.hh"
#include "vk_device.hh"

namespace blender::gpu::render_graph {
VKCommandBufferWrapper::VKCommandBufferWrapper(VkCommandBuffer vk_command_buffer,
                                               const VolkDeviceTable &functions,
                                               const VKExtensions &extensions)
    : vk_command_buffer_(vk_command_buffer), functions(&functions)
{
  use_dynamic_rendering_local_read = extensions.dynamic_rendering_local_read;
}

void VKCommandBufferWrapper::begin_recording()
{
  VkCommandBufferBeginInfo vk_command_buffer_begin_info = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      nullptr,
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      nullptr};
  functions->vkBeginCommandBuffer(vk_command_buffer_, &vk_command_buffer_begin_info);
}

void VKCommandBufferWrapper::end_recording()
{
  functions->vkEndCommandBuffer(vk_command_buffer_);
}

void VKCommandBufferWrapper::bind_pipeline(VkPipelineBindPoint pipeline_bind_point,
                                           VkPipeline pipeline)
{
  functions->vkCmdBindPipeline(vk_command_buffer_, pipeline_bind_point, pipeline);
}

void VKCommandBufferWrapper::bind_descriptor_sets(VkPipelineBindPoint pipeline_bind_point,
                                                  VkPipelineLayout layout,
                                                  uint32_t first_set,
                                                  uint32_t descriptor_set_count,
                                                  const VkDescriptorSet *p_descriptor_sets,
                                                  uint32_t dynamic_offset_count,
                                                  const uint32_t *p_dynamic_offsets)
{
  functions->vkCmdBindDescriptorSets(vk_command_buffer_,
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
  functions->vkCmdBindIndexBuffer(vk_command_buffer_, buffer, offset, index_type);
}

void VKCommandBufferWrapper::bind_vertex_buffers(uint32_t first_binding,
                                                 uint32_t binding_count,
                                                 const VkBuffer *p_buffers,
                                                 const VkDeviceSize *p_offsets)
{
  functions->vkCmdBindVertexBuffers(
      vk_command_buffer_, first_binding, binding_count, p_buffers, p_offsets);
}

void VKCommandBufferWrapper::draw(uint32_t vertex_count,
                                  uint32_t instance_count,
                                  uint32_t first_vertex,
                                  uint32_t first_instance)
{
  functions->vkCmdDraw(
      vk_command_buffer_, vertex_count, instance_count, first_vertex, first_instance);
}

void VKCommandBufferWrapper::draw_indexed(uint32_t index_count,
                                          uint32_t instance_count,
                                          uint32_t first_index,
                                          int32_t vertex_offset,
                                          uint32_t first_instance)
{
  functions->vkCmdDrawIndexed(
      vk_command_buffer_, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void VKCommandBufferWrapper::draw_indirect(VkBuffer buffer,
                                           VkDeviceSize offset,
                                           uint32_t draw_count,
                                           uint32_t stride)
{
  functions->vkCmdDrawIndirect(vk_command_buffer_, buffer, offset, draw_count, stride);
}

void VKCommandBufferWrapper::draw_indexed_indirect(VkBuffer buffer,
                                                   VkDeviceSize offset,
                                                   uint32_t draw_count,
                                                   uint32_t stride)
{
  functions->vkCmdDrawIndexedIndirect(vk_command_buffer_, buffer, offset, draw_count, stride);
}

void VKCommandBufferWrapper::dispatch(uint32_t group_count_x,
                                      uint32_t group_count_y,
                                      uint32_t group_count_z)
{
  functions->vkCmdDispatch(vk_command_buffer_, group_count_x, group_count_y, group_count_z);
}

void VKCommandBufferWrapper::dispatch_indirect(VkBuffer buffer, VkDeviceSize offset)
{
  functions->vkCmdDispatchIndirect(vk_command_buffer_, buffer, offset);
}

void VKCommandBufferWrapper::update_buffer(VkBuffer dst_buffer,
                                           VkDeviceSize dst_offset,
                                           VkDeviceSize data_size,
                                           const void *p_data)
{
  functions->vkCmdUpdateBuffer(vk_command_buffer_, dst_buffer, dst_offset, data_size, p_data);
}

void VKCommandBufferWrapper::copy_buffer(VkBuffer src_buffer,
                                         VkBuffer dst_buffer,
                                         uint32_t region_count,
                                         const VkBufferCopy *p_regions)
{
  functions->vkCmdCopyBuffer(vk_command_buffer_, src_buffer, dst_buffer, region_count, p_regions);
}

void VKCommandBufferWrapper::copy_image(VkImage src_image,
                                        VkImageLayout src_image_layout,
                                        VkImage dst_image,
                                        VkImageLayout dst_image_layout,
                                        uint32_t region_count,
                                        const VkImageCopy *p_regions)
{
  functions->vkCmdCopyImage(vk_command_buffer_,
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
  functions->vkCmdBlitImage(vk_command_buffer_,
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
  functions->vkCmdCopyBufferToImage(
      vk_command_buffer_, src_buffer, dst_image, dst_image_layout, region_count, p_regions);
}

void VKCommandBufferWrapper::copy_image_to_buffer(VkImage src_image,
                                                  VkImageLayout src_image_layout,
                                                  VkBuffer dst_buffer,
                                                  uint32_t region_count,
                                                  const VkBufferImageCopy *p_regions)
{
  functions->vkCmdCopyImageToBuffer(
      vk_command_buffer_, src_image, src_image_layout, dst_buffer, region_count, p_regions);
}

void VKCommandBufferWrapper::fill_buffer(VkBuffer dst_buffer,
                                         VkDeviceSize dst_offset,
                                         VkDeviceSize size,
                                         uint32_t data)
{
  functions->vkCmdFillBuffer(vk_command_buffer_, dst_buffer, dst_offset, size, data);
}

void VKCommandBufferWrapper::clear_color_image(VkImage image,
                                               VkImageLayout image_layout,
                                               const VkClearColorValue *p_color,
                                               uint32_t range_count,
                                               const VkImageSubresourceRange *p_ranges)
{
  functions->vkCmdClearColorImage(
      vk_command_buffer_, image, image_layout, p_color, range_count, p_ranges);
}
void VKCommandBufferWrapper::clear_depth_stencil_image(
    VkImage image,
    VkImageLayout image_layout,
    const VkClearDepthStencilValue *p_depth_stencil,
    uint32_t range_count,
    const VkImageSubresourceRange *p_ranges)
{
  functions->vkCmdClearDepthStencilImage(
      vk_command_buffer_, image, image_layout, p_depth_stencil, range_count, p_ranges);
}

void VKCommandBufferWrapper::clear_attachments(uint32_t attachment_count,
                                               const VkClearAttachment *p_attachments,
                                               uint32_t rect_count,
                                               const VkClearRect *p_rects)
{
  functions->vkCmdClearAttachments(
      vk_command_buffer_, attachment_count, p_attachments, rect_count, p_rects);
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
  functions->vkCmdPipelineBarrier(vk_command_buffer_,
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
  functions->vkCmdPushConstants(vk_command_buffer_, layout, stage_flags, offset, size, p_values);
}

void VKCommandBufferWrapper::set_viewport(const Vector<VkViewport> viewports)
{
  functions->vkCmdSetViewport(vk_command_buffer_, 0, viewports.size(), viewports.data());
}

void VKCommandBufferWrapper::set_scissor(const Vector<VkRect2D> scissors)
{
  functions->vkCmdSetScissor(vk_command_buffer_, 0, scissors.size(), scissors.data());
}
void VKCommandBufferWrapper::set_line_width(const float line_width)
{
  functions->vkCmdSetLineWidth(vk_command_buffer_, line_width);
}
void VKCommandBufferWrapper::set_stencil_compare_mask(const uint32_t compare_mask)
{
  functions->vkCmdSetStencilCompareMask(
      vk_command_buffer_, VK_STENCIL_FACE_FRONT_AND_BACK, compare_mask);
}
void VKCommandBufferWrapper::set_stencil_write_mask(const uint32_t write_mask)
{
  functions->vkCmdSetStencilWriteMask(
      vk_command_buffer_, VK_STENCIL_FACE_FRONT_AND_BACK, write_mask);
}
void VKCommandBufferWrapper::set_stencil_reference(const uint32_t reference)
{
  functions->vkCmdSetStencilReference(
      vk_command_buffer_, VK_STENCIL_FACE_FRONT_AND_BACK, reference);
}
void VKCommandBufferWrapper::set_front_face(const VkFrontFace front_face)
{
  const VKDevice &device = VKBackend::get().device;
  BLI_assert(device.functions.vkCmdSetFrontFaceEXT);
  device.functions.vkCmdSetFrontFaceEXT(vk_command_buffer_, front_face);
}
void VKCommandBufferWrapper::set_vertex_input(
    Span<VkVertexInputBindingDescription2EXT> vertex_binding_descriptions,
    Span<VkVertexInputAttributeDescription2EXT> vertex_attribute_descriptions)
{
  const VKDevice &device = VKBackend::get().device;
  BLI_assert(device.functions.vkCmdSetVertexInputEXT);
  device.functions.vkCmdSetVertexInputEXT(vk_command_buffer_,
                                          vertex_binding_descriptions.size(),
                                          vertex_binding_descriptions.data(),
                                          vertex_attribute_descriptions.size(),
                                          vertex_attribute_descriptions.data());
}

void VKCommandBufferWrapper::begin_rendering(const VkRenderingInfo *p_rendering_info)
{
  BLI_assert(functions->vkCmdBeginRenderingKHR);
  functions->vkCmdBeginRenderingKHR(vk_command_buffer_, p_rendering_info);
}

void VKCommandBufferWrapper::end_rendering()
{
  BLI_assert(functions->vkCmdEndRenderingKHR);
  functions->vkCmdEndRenderingKHR(vk_command_buffer_);
}

void VKCommandBufferWrapper::begin_query(VkQueryPool vk_query_pool,
                                         uint32_t query_index,
                                         VkQueryControlFlags vk_query_control_flags)
{
  functions->vkCmdBeginQuery(
      vk_command_buffer_, vk_query_pool, query_index, vk_query_control_flags);
}

void VKCommandBufferWrapper::end_query(VkQueryPool vk_query_pool, uint32_t query_index)
{
  functions->vkCmdEndQuery(vk_command_buffer_, vk_query_pool, query_index);
}

void VKCommandBufferWrapper::reset_query_pool(VkQueryPool vk_query_pool,
                                              uint32_t first_query,
                                              uint32_t query_count)
{
  functions->vkCmdResetQueryPool(vk_command_buffer_, vk_query_pool, first_query, query_count);
}

void VKCommandBufferWrapper::begin_debug_utils_label(
    const VkDebugUtilsLabelEXT *vk_debug_utils_label)
{
  if (vkCmdBeginDebugUtilsLabelEXT) {
    vkCmdBeginDebugUtilsLabelEXT(vk_command_buffer_, vk_debug_utils_label);
  }
}

void VKCommandBufferWrapper::end_debug_utils_label()
{
  if (vkCmdEndDebugUtilsLabelEXT) {
    vkCmdEndDebugUtilsLabelEXT(vk_command_buffer_);
  }
}

/* VK_KHR_ray_tracing */
void VKCommandBufferWrapper::build_acceleration_structure(
    const VkAccelerationStructureBuildGeometryInfoKHR *p_infos,
    const VkAccelerationStructureBuildRangeInfoKHR *p_build_range_infos)
{
  const VKDevice &device = VKBackend::get().device;
  device.functions.vkCmdBuildAccelerationStructuresKHR(
      vk_command_buffer_, 1, p_infos, &p_build_range_infos);
}

}  // namespace blender::gpu::render_graph
