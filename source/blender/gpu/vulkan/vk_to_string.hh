/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "vk_common.hh"

namespace blender::gpu {
std::string to_string(VkImage vk_handle);
std::string to_string(VkImageView vk_handle);
std::string to_string(VkBuffer vk_handle);
std::string to_string(VkDescriptorSet vk_handle);
std::string to_string(VkPipeline vk_handle);
std::string to_string(VkPipelineLayout vk_handle);
std::string to_string(VkRenderPass vk_handle);
std::string to_string(VkFramebuffer vk_handle);

const char *to_string(VkAttachmentLoadOp vk_attachment_load_op);
const char *to_string(VkAttachmentStoreOp vk_attachment_store_op);
const char *to_string(VkFilter vk_filter);
const char *to_string(VkImageLayout vk_image_layout);
const char *to_string(VkIndexType vk_index_type);
const char *to_string(VkObjectType vk_object_type);
const char *to_string(VkPipelineBindPoint vk_pipeline_bind_point);
const char *to_string(VkResolveModeFlagBits vk_resolve_mode_flag_bits);
const char *to_string(VkSubpassContents vk_subpass_contents);
std::string to_string_vk_access_flags(VkAccessFlags vk_access_flags);
std::string to_string_vk_dependency_flags(VkDependencyFlags vk_dependency_flags);
std::string to_string_vk_image_aspect_flags(VkImageAspectFlags vk_image_aspect_flags);
std::string to_string_vk_pipeline_stage_flags(VkPipelineStageFlags vk_pipeline_stage_flags);
std::string to_string_vk_rendering_flags(VkRenderingFlags vk_rendering_flags);
std::string to_string_vk_shader_stage_flags(VkShaderStageFlags vk_shader_stage_flags);
std::string to_string(const VkBufferCopy &vk_buffer_copy, int indentation_level = 0);
std::string to_string(const VkBufferImageCopy &vk_buffer_image_copy, int indentation_level = 0);
std::string to_string(const VkBufferMemoryBarrier &vk_buffer_memory_barrier,
                      int indentation_level = 0);
std::string to_string(const VkClearAttachment &vk_clear_attachment, int indentation_level = 0);
std::string to_string(const VkClearDepthStencilValue &vk_clear_depth_stencil_value,
                      int indentation_level = 0);
std::string to_string(const VkClearRect &vk_clear_rect, int indentation_level = 0);
std::string to_string(const VkExtent2D &vk_extent2_d, int indentation_level = 0);
std::string to_string(const VkExtent3D &vk_extent3_d, int indentation_level = 0);
std::string to_string(const VkImageBlit &vk_image_blit, int indentation_level = 0);
std::string to_string(const VkImageCopy &vk_image_copy, int indentation_level = 0);
std::string to_string(const VkImageMemoryBarrier &vk_image_memory_barrier,
                      int indentation_level = 0);
std::string to_string(const VkImageSubresourceLayers &vk_image_subresource_layers,
                      int indentation_level = 0);
std::string to_string(const VkImageSubresourceRange &vk_image_subresource_range,
                      int indentation_level = 0);
std::string to_string(const VkMemoryBarrier &vk_memory_barrier, int indentation_level = 0);
std::string to_string(const VkOffset2D &vk_offset2_d, int indentation_level = 0);
std::string to_string(const VkOffset3D &vk_offset3_d, int indentation_level = 0);
std::string to_string(const VkRect2D &vk_rect2_d, int indentation_level = 0);
std::string to_string(const VkRenderPassBeginInfo &vk_render_pass_begin_info,
                      int indentation_level = 0);
std::string to_string(const VkRenderingAttachmentInfo &vk_rendering_attachment_info,
                      int indentation_level = 0);
std::string to_string(const VkRenderingInfo &vk_rendering_info, int indentation_level = 0);

}  // namespace blender::gpu
