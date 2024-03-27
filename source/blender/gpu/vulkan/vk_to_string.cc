/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include <sstream>

#include "vk_to_string.hh"

namespace blender::gpu {
const char *to_string(const VkFilter vk_filter)
{
  switch (vk_filter) {
    case VK_FILTER_NEAREST:
      return STRINGIFY(VK_FILTER_NEAREST);

    case VK_FILTER_LINEAR:
      return STRINGIFY(VK_FILTER_LINEAR);

    default:
      break;
  }
  return STRINGIFY_ARG(vk_filter);
}

const char *to_string(const VkImageLayout vk_image_layout)
{
  switch (vk_image_layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      return STRINGIFY(VK_IMAGE_LAYOUT_UNDEFINED);

    case VK_IMAGE_LAYOUT_GENERAL:
      return STRINGIFY(VK_IMAGE_LAYOUT_GENERAL);

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return STRINGIFY(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      return STRINGIFY(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
      return STRINGIFY(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return STRINGIFY(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return STRINGIFY(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return STRINGIFY(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    case VK_IMAGE_LAYOUT_PREINITIALIZED:
      return STRINGIFY(VK_IMAGE_LAYOUT_PREINITIALIZED);

    /* Extensions for VK_VERSION_1_1. */
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
      return STRINGIFY(VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL);

    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
      return STRINGIFY(VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL);

    /* Extensions for VK_VERSION_1_2. */
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
      return STRINGIFY(VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
      return STRINGIFY(VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);

    case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
      return STRINGIFY(VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL);

    case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
      return STRINGIFY(VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL);

    /* Extensions for VK_KHR_swapchain. */
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      return STRINGIFY(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    default:
      break;
  }
  return STRINGIFY_ARG(vk_image_layout);
}

const char *to_string(const VkIndexType vk_index_type)
{
  switch (vk_index_type) {
    case VK_INDEX_TYPE_UINT16:
      return STRINGIFY(VK_INDEX_TYPE_UINT16);

    case VK_INDEX_TYPE_UINT32:
      return STRINGIFY(VK_INDEX_TYPE_UINT32);

    default:
      break;
  }
  return STRINGIFY_ARG(vk_index_type);
}

const char *to_string(const VkObjectType vk_object_type)
{
  switch (vk_object_type) {
    case VK_OBJECT_TYPE_UNKNOWN:
      return STRINGIFY(VK_OBJECT_TYPE_UNKNOWN);

    case VK_OBJECT_TYPE_INSTANCE:
      return STRINGIFY(VK_OBJECT_TYPE_INSTANCE);

    case VK_OBJECT_TYPE_PHYSICAL_DEVICE:
      return STRINGIFY(VK_OBJECT_TYPE_PHYSICAL_DEVICE);

    case VK_OBJECT_TYPE_DEVICE:
      return STRINGIFY(VK_OBJECT_TYPE_DEVICE);

    case VK_OBJECT_TYPE_QUEUE:
      return STRINGIFY(VK_OBJECT_TYPE_QUEUE);

    case VK_OBJECT_TYPE_SEMAPHORE:
      return STRINGIFY(VK_OBJECT_TYPE_SEMAPHORE);

    case VK_OBJECT_TYPE_COMMAND_BUFFER:
      return STRINGIFY(VK_OBJECT_TYPE_COMMAND_BUFFER);

    case VK_OBJECT_TYPE_FENCE:
      return STRINGIFY(VK_OBJECT_TYPE_FENCE);

    case VK_OBJECT_TYPE_DEVICE_MEMORY:
      return STRINGIFY(VK_OBJECT_TYPE_DEVICE_MEMORY);

    case VK_OBJECT_TYPE_BUFFER:
      return STRINGIFY(VK_OBJECT_TYPE_BUFFER);

    case VK_OBJECT_TYPE_IMAGE:
      return STRINGIFY(VK_OBJECT_TYPE_IMAGE);

    case VK_OBJECT_TYPE_EVENT:
      return STRINGIFY(VK_OBJECT_TYPE_EVENT);

    case VK_OBJECT_TYPE_QUERY_POOL:
      return STRINGIFY(VK_OBJECT_TYPE_QUERY_POOL);

    case VK_OBJECT_TYPE_BUFFER_VIEW:
      return STRINGIFY(VK_OBJECT_TYPE_BUFFER_VIEW);

    case VK_OBJECT_TYPE_IMAGE_VIEW:
      return STRINGIFY(VK_OBJECT_TYPE_IMAGE_VIEW);

    case VK_OBJECT_TYPE_SHADER_MODULE:
      return STRINGIFY(VK_OBJECT_TYPE_SHADER_MODULE);

    case VK_OBJECT_TYPE_PIPELINE_CACHE:
      return STRINGIFY(VK_OBJECT_TYPE_PIPELINE_CACHE);

    case VK_OBJECT_TYPE_PIPELINE_LAYOUT:
      return STRINGIFY(VK_OBJECT_TYPE_PIPELINE_LAYOUT);

    case VK_OBJECT_TYPE_RENDER_PASS:
      return STRINGIFY(VK_OBJECT_TYPE_RENDER_PASS);

    case VK_OBJECT_TYPE_PIPELINE:
      return STRINGIFY(VK_OBJECT_TYPE_PIPELINE);

    case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
      return STRINGIFY(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT);

    case VK_OBJECT_TYPE_SAMPLER:
      return STRINGIFY(VK_OBJECT_TYPE_SAMPLER);

    case VK_OBJECT_TYPE_DESCRIPTOR_POOL:
      return STRINGIFY(VK_OBJECT_TYPE_DESCRIPTOR_POOL);

    case VK_OBJECT_TYPE_DESCRIPTOR_SET:
      return STRINGIFY(VK_OBJECT_TYPE_DESCRIPTOR_SET);

    case VK_OBJECT_TYPE_FRAMEBUFFER:
      return STRINGIFY(VK_OBJECT_TYPE_FRAMEBUFFER);

    case VK_OBJECT_TYPE_COMMAND_POOL:
      return STRINGIFY(VK_OBJECT_TYPE_COMMAND_POOL);

    /* Extensions for VK_VERSION_1_1. */
    case VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION:
      return STRINGIFY(VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION);

    case VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE:
      return STRINGIFY(VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE);

    /* Extensions for VK_KHR_swapchain. */
    case VK_OBJECT_TYPE_SWAPCHAIN_KHR:
      return STRINGIFY(VK_OBJECT_TYPE_SWAPCHAIN_KHR);

    default:
      break;
  }
  return STRINGIFY_ARG(vk_object_type);
}

const char *to_string(const VkPipelineBindPoint vk_pipeline_bind_point)
{
  switch (vk_pipeline_bind_point) {
    case VK_PIPELINE_BIND_POINT_GRAPHICS:
      return STRINGIFY(VK_PIPELINE_BIND_POINT_GRAPHICS);

    case VK_PIPELINE_BIND_POINT_COMPUTE:
      return STRINGIFY(VK_PIPELINE_BIND_POINT_COMPUTE);

    default:
      break;
  }
  return STRINGIFY_ARG(vk_pipeline_bind_point);
}

const char *to_string(const VkSubpassContents vk_subpass_contents)
{
  switch (vk_subpass_contents) {
    case VK_SUBPASS_CONTENTS_INLINE:
      return STRINGIFY(VK_SUBPASS_CONTENTS_INLINE);

    case VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS:
      return STRINGIFY(VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

    default:
      break;
  }
  return STRINGIFY_ARG(vk_subpass_contents);
}

std::string to_string_vk_access_flags(const VkAccessFlags vk_access_flags)
{
  std::stringstream ss;

  if (vk_access_flags & VK_ACCESS_INDIRECT_COMMAND_READ_BIT) {
    ss << STRINGIFY(VK_ACCESS_INDIRECT_COMMAND_READ_BIT) << ", ";
  }
  if (vk_access_flags & VK_ACCESS_INDEX_READ_BIT) {
    ss << STRINGIFY(VK_ACCESS_INDEX_READ_BIT) << ", ";
  }
  if (vk_access_flags & VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT) {
    ss << STRINGIFY(VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT) << ", ";
  }
  if (vk_access_flags & VK_ACCESS_UNIFORM_READ_BIT) {
    ss << STRINGIFY(VK_ACCESS_UNIFORM_READ_BIT) << ", ";
  }
  if (vk_access_flags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) {
    ss << STRINGIFY(VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) << ", ";
  }
  if (vk_access_flags & VK_ACCESS_SHADER_READ_BIT) {
    ss << STRINGIFY(VK_ACCESS_SHADER_READ_BIT) << ", ";
  }
  if (vk_access_flags & VK_ACCESS_SHADER_WRITE_BIT) {
    ss << STRINGIFY(VK_ACCESS_SHADER_WRITE_BIT) << ", ";
  }
  if (vk_access_flags & VK_ACCESS_COLOR_ATTACHMENT_READ_BIT) {
    ss << STRINGIFY(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT) << ", ";
  }
  if (vk_access_flags & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) {
    ss << STRINGIFY(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) << ", ";
  }
  if (vk_access_flags & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT) {
    ss << STRINGIFY(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT) << ", ";
  }
  if (vk_access_flags & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT) {
    ss << STRINGIFY(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT) << ", ";
  }
  if (vk_access_flags & VK_ACCESS_TRANSFER_READ_BIT) {
    ss << STRINGIFY(VK_ACCESS_TRANSFER_READ_BIT) << ", ";
  }
  if (vk_access_flags & VK_ACCESS_TRANSFER_WRITE_BIT) {
    ss << STRINGIFY(VK_ACCESS_TRANSFER_WRITE_BIT) << ", ";
  }
  if (vk_access_flags & VK_ACCESS_HOST_READ_BIT) {
    ss << STRINGIFY(VK_ACCESS_HOST_READ_BIT) << ", ";
  }
  if (vk_access_flags & VK_ACCESS_HOST_WRITE_BIT) {
    ss << STRINGIFY(VK_ACCESS_HOST_WRITE_BIT) << ", ";
  }
  if (vk_access_flags & VK_ACCESS_MEMORY_READ_BIT) {
    ss << STRINGIFY(VK_ACCESS_MEMORY_READ_BIT) << ", ";
  }
  if (vk_access_flags & VK_ACCESS_MEMORY_WRITE_BIT) {
    ss << STRINGIFY(VK_ACCESS_MEMORY_WRITE_BIT) << ", ";
  }

  std::string result = ss.str();
  if (result.size() >= 2) {
    result.erase(result.size() - 2, 2);
  }
  return result;
}

std::string to_string_vk_dependency_flags(const VkDependencyFlags vk_dependency_flags)
{
  std::stringstream ss;

  if (vk_dependency_flags & VK_DEPENDENCY_BY_REGION_BIT) {
    ss << STRINGIFY(VK_DEPENDENCY_BY_REGION_BIT) << ", ";
  }
  /* Extensions for VK_VERSION_1_1. */
  if (vk_dependency_flags & VK_DEPENDENCY_DEVICE_GROUP_BIT) {
    ss << STRINGIFY(VK_DEPENDENCY_DEVICE_GROUP_BIT) << ", ";
  }
  if (vk_dependency_flags & VK_DEPENDENCY_VIEW_LOCAL_BIT) {
    ss << STRINGIFY(VK_DEPENDENCY_VIEW_LOCAL_BIT) << ", ";
  }

  std::string result = ss.str();
  if (result.size() >= 2) {
    result.erase(result.size() - 2, 2);
  }
  return result;
}

std::string to_string_vk_image_aspect_flags(const VkImageAspectFlags vk_image_aspect_flags)
{
  std::stringstream ss;

  if (vk_image_aspect_flags & VK_IMAGE_ASPECT_COLOR_BIT) {
    ss << STRINGIFY(VK_IMAGE_ASPECT_COLOR_BIT) << ", ";
  }
  if (vk_image_aspect_flags & VK_IMAGE_ASPECT_DEPTH_BIT) {
    ss << STRINGIFY(VK_IMAGE_ASPECT_DEPTH_BIT) << ", ";
  }
  if (vk_image_aspect_flags & VK_IMAGE_ASPECT_STENCIL_BIT) {
    ss << STRINGIFY(VK_IMAGE_ASPECT_STENCIL_BIT) << ", ";
  }
  if (vk_image_aspect_flags & VK_IMAGE_ASPECT_METADATA_BIT) {
    ss << STRINGIFY(VK_IMAGE_ASPECT_METADATA_BIT) << ", ";
  }
  /* Extensions for VK_VERSION_1_1. */
  if (vk_image_aspect_flags & VK_IMAGE_ASPECT_PLANE_0_BIT) {
    ss << STRINGIFY(VK_IMAGE_ASPECT_PLANE_0_BIT) << ", ";
  }
  if (vk_image_aspect_flags & VK_IMAGE_ASPECT_PLANE_1_BIT) {
    ss << STRINGIFY(VK_IMAGE_ASPECT_PLANE_1_BIT) << ", ";
  }
  if (vk_image_aspect_flags & VK_IMAGE_ASPECT_PLANE_2_BIT) {
    ss << STRINGIFY(VK_IMAGE_ASPECT_PLANE_2_BIT) << ", ";
  }

  std::string result = ss.str();
  if (result.size() >= 2) {
    result.erase(result.size() - 2, 2);
  }
  return result;
}

std::string to_string_vk_pipeline_stage_flags(const VkPipelineStageFlags vk_pipeline_stage_flags)
{
  std::stringstream ss;

  if (vk_pipeline_stage_flags & VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT) {
    ss << STRINGIFY(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT) << ", ";
  }
  if (vk_pipeline_stage_flags & VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT) {
    ss << STRINGIFY(VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT) << ", ";
  }
  if (vk_pipeline_stage_flags & VK_PIPELINE_STAGE_VERTEX_INPUT_BIT) {
    ss << STRINGIFY(VK_PIPELINE_STAGE_VERTEX_INPUT_BIT) << ", ";
  }
  if (vk_pipeline_stage_flags & VK_PIPELINE_STAGE_VERTEX_SHADER_BIT) {
    ss << STRINGIFY(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT) << ", ";
  }
  if (vk_pipeline_stage_flags & VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT) {
    ss << STRINGIFY(VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT) << ", ";
  }
  if (vk_pipeline_stage_flags & VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT) {
    ss << STRINGIFY(VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT) << ", ";
  }
  if (vk_pipeline_stage_flags & VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT) {
    ss << STRINGIFY(VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT) << ", ";
  }
  if (vk_pipeline_stage_flags & VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) {
    ss << STRINGIFY(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) << ", ";
  }
  if (vk_pipeline_stage_flags & VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT) {
    ss << STRINGIFY(VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT) << ", ";
  }
  if (vk_pipeline_stage_flags & VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT) {
    ss << STRINGIFY(VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT) << ", ";
  }
  if (vk_pipeline_stage_flags & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) {
    ss << STRINGIFY(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) << ", ";
  }
  if (vk_pipeline_stage_flags & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) {
    ss << STRINGIFY(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) << ", ";
  }
  if (vk_pipeline_stage_flags & VK_PIPELINE_STAGE_TRANSFER_BIT) {
    ss << STRINGIFY(VK_PIPELINE_STAGE_TRANSFER_BIT) << ", ";
  }
  if (vk_pipeline_stage_flags & VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT) {
    ss << STRINGIFY(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT) << ", ";
  }
  if (vk_pipeline_stage_flags & VK_PIPELINE_STAGE_HOST_BIT) {
    ss << STRINGIFY(VK_PIPELINE_STAGE_HOST_BIT) << ", ";
  }
  if (vk_pipeline_stage_flags & VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT) {
    ss << STRINGIFY(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT) << ", ";
  }
  if (vk_pipeline_stage_flags & VK_PIPELINE_STAGE_ALL_COMMANDS_BIT) {
    ss << STRINGIFY(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT) << ", ";
  }

  std::string result = ss.str();
  if (result.size() >= 2) {
    result.erase(result.size() - 2, 2);
  }
  return result;
}

std::string to_string_vk_shader_stage_flags(const VkShaderStageFlags vk_shader_stage_flags)
{
  std::stringstream ss;

  if (vk_shader_stage_flags & VK_SHADER_STAGE_VERTEX_BIT) {
    ss << STRINGIFY(VK_SHADER_STAGE_VERTEX_BIT) << ", ";
  }
  if (vk_shader_stage_flags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) {
    ss << STRINGIFY(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) << ", ";
  }
  if (vk_shader_stage_flags & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) {
    ss << STRINGIFY(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) << ", ";
  }
  if (vk_shader_stage_flags & VK_SHADER_STAGE_GEOMETRY_BIT) {
    ss << STRINGIFY(VK_SHADER_STAGE_GEOMETRY_BIT) << ", ";
  }
  if (vk_shader_stage_flags & VK_SHADER_STAGE_FRAGMENT_BIT) {
    ss << STRINGIFY(VK_SHADER_STAGE_FRAGMENT_BIT) << ", ";
  }
  if (vk_shader_stage_flags & VK_SHADER_STAGE_COMPUTE_BIT) {
    ss << STRINGIFY(VK_SHADER_STAGE_COMPUTE_BIT) << ", ";
  }
  if (vk_shader_stage_flags & VK_SHADER_STAGE_ALL_GRAPHICS) {
    ss << STRINGIFY(VK_SHADER_STAGE_ALL_GRAPHICS) << ", ";
  }
  if (vk_shader_stage_flags & VK_SHADER_STAGE_ALL) {
    ss << STRINGIFY(VK_SHADER_STAGE_ALL) << ", ";
  }

  std::string result = ss.str();
  if (result.size() >= 2) {
    result.erase(result.size() - 2, 2);
  }
  return result;
}

std::string to_string(const VkBufferCopy &vk_buffer_copy, int indentation_level)
{
  UNUSED_VARS(indentation_level);
  std::stringstream ss;
  ss << "src_offset=" << vk_buffer_copy.srcOffset;
  ss << ", dst_offset=" << vk_buffer_copy.dstOffset;
  ss << ", size=" << vk_buffer_copy.size;

  return ss.str();
}

std::string to_string(const VkBufferImageCopy &vk_buffer_image_copy, int indentation_level)
{
  std::stringstream ss;
  ss << "buffer_offset=" << vk_buffer_image_copy.bufferOffset;
  ss << ", buffer_row_length=" << vk_buffer_image_copy.bufferRowLength;
  ss << ", buffer_image_height=" << vk_buffer_image_copy.bufferImageHeight;
  ss << ", image_subresource="
     << "\n";
  ss << std::string(indentation_level * 2 + 2, ' ')
     << to_string(vk_buffer_image_copy.imageSubresource, indentation_level + 1);
  ss << std::string(indentation_level * 2, ' ');
  ss << ", image_offset="
     << "\n";
  ss << std::string(indentation_level * 2 + 2, ' ')
     << to_string(vk_buffer_image_copy.imageOffset, indentation_level + 1);
  ss << std::string(indentation_level * 2, ' ');
  ss << ", image_extent="
     << "\n";
  ss << std::string(indentation_level * 2 + 2, ' ')
     << to_string(vk_buffer_image_copy.imageExtent, indentation_level + 1);
  ss << std::string(indentation_level * 2, ' ');

  return ss.str();
}

std::string to_string(const VkBufferMemoryBarrier &vk_buffer_memory_barrier, int indentation_level)
{
  UNUSED_VARS(indentation_level);
  std::stringstream ss;
  ss << "src_access_mask=" << to_string_vk_access_flags(vk_buffer_memory_barrier.srcAccessMask);
  ss << ", dst_access_mask=" << to_string_vk_access_flags(vk_buffer_memory_barrier.dstAccessMask);
  ss << ", buffer=" << vk_buffer_memory_barrier.buffer;
  ss << ", offset=" << vk_buffer_memory_barrier.offset;
  ss << ", size=" << vk_buffer_memory_barrier.size;

  return ss.str();
}

std::string to_string(const VkClearAttachment &vk_clear_attachment, int indentation_level)
{
  UNUSED_VARS(indentation_level);
  std::stringstream ss;
  ss << "aspect_mask=" << to_string_vk_image_aspect_flags(vk_clear_attachment.aspectMask);
  ss << ", color_attachment=" << vk_clear_attachment.colorAttachment;

  return ss.str();
}

std::string to_string(const VkClearDepthStencilValue &vk_clear_depth_stencil_value,
                      int indentation_level)
{
  UNUSED_VARS(indentation_level);
  std::stringstream ss;
  ss << "depth=" << vk_clear_depth_stencil_value.depth;
  ss << ", stencil=" << vk_clear_depth_stencil_value.stencil;

  return ss.str();
}

std::string to_string(const VkClearRect &vk_clear_rect, int indentation_level)
{
  std::stringstream ss;
  ss << "rect="
     << "\n";
  ss << std::string(indentation_level * 2 + 2, ' ')
     << to_string(vk_clear_rect.rect, indentation_level + 1);
  ss << std::string(indentation_level * 2, ' ');
  ss << ", base_array_layer=" << vk_clear_rect.baseArrayLayer;
  ss << ", layer_count=" << vk_clear_rect.layerCount;

  return ss.str();
}

std::string to_string(const VkExtent2D &vk_extent2_d, int indentation_level)
{
  UNUSED_VARS(indentation_level);
  std::stringstream ss;
  ss << "width=" << vk_extent2_d.width;
  ss << ", height=" << vk_extent2_d.height;

  return ss.str();
}

std::string to_string(const VkExtent3D &vk_extent3_d, int indentation_level)
{
  UNUSED_VARS(indentation_level);
  std::stringstream ss;
  ss << "width=" << vk_extent3_d.width;
  ss << ", height=" << vk_extent3_d.height;
  ss << ", depth=" << vk_extent3_d.depth;

  return ss.str();
}

std::string to_string(const VkImageBlit &vk_image_blit, int indentation_level)
{
  std::stringstream ss;
  ss << "src_subresource="
     << "\n";
  ss << std::string(indentation_level * 2 + 2, ' ')
     << to_string(vk_image_blit.srcSubresource, indentation_level + 1);
  ss << std::string(indentation_level * 2, ' ');
  ss << ", dst_subresource="
     << "\n";
  ss << std::string(indentation_level * 2 + 2, ' ')
     << to_string(vk_image_blit.dstSubresource, indentation_level + 1);
  ss << std::string(indentation_level * 2, ' ');

  return ss.str();
}

std::string to_string(const VkImageCopy &vk_image_copy, int indentation_level)
{
  std::stringstream ss;
  ss << "src_subresource="
     << "\n";
  ss << std::string(indentation_level * 2 + 2, ' ')
     << to_string(vk_image_copy.srcSubresource, indentation_level + 1);
  ss << std::string(indentation_level * 2, ' ');
  ss << ", src_offset="
     << "\n";
  ss << std::string(indentation_level * 2 + 2, ' ')
     << to_string(vk_image_copy.srcOffset, indentation_level + 1);
  ss << std::string(indentation_level * 2, ' ');
  ss << ", dst_subresource="
     << "\n";
  ss << std::string(indentation_level * 2 + 2, ' ')
     << to_string(vk_image_copy.dstSubresource, indentation_level + 1);
  ss << std::string(indentation_level * 2, ' ');
  ss << ", dst_offset="
     << "\n";
  ss << std::string(indentation_level * 2 + 2, ' ')
     << to_string(vk_image_copy.dstOffset, indentation_level + 1);
  ss << std::string(indentation_level * 2, ' ');
  ss << ", extent="
     << "\n";
  ss << std::string(indentation_level * 2 + 2, ' ')
     << to_string(vk_image_copy.extent, indentation_level + 1);
  ss << std::string(indentation_level * 2, ' ');

  return ss.str();
}

std::string to_string(const VkImageMemoryBarrier &vk_image_memory_barrier, int indentation_level)
{
  std::stringstream ss;
  ss << "src_access_mask=" << to_string_vk_access_flags(vk_image_memory_barrier.srcAccessMask);
  ss << ", dst_access_mask=" << to_string_vk_access_flags(vk_image_memory_barrier.dstAccessMask);
  ss << ", old_layout=" << to_string(vk_image_memory_barrier.oldLayout);
  ss << ", new_layout=" << to_string(vk_image_memory_barrier.newLayout);
  ss << ", image=" << vk_image_memory_barrier.image;
  ss << ", subresource_range="
     << "\n";
  ss << std::string(indentation_level * 2 + 2, ' ')
     << to_string(vk_image_memory_barrier.subresourceRange, indentation_level + 1);
  ss << std::string(indentation_level * 2, ' ');

  return ss.str();
}

std::string to_string(const VkImageSubresourceLayers &vk_image_subresource_layers,
                      int indentation_level)
{
  UNUSED_VARS(indentation_level);
  std::stringstream ss;
  ss << "aspect_mask=" << to_string_vk_image_aspect_flags(vk_image_subresource_layers.aspectMask);
  ss << ", mip_level=" << vk_image_subresource_layers.mipLevel;
  ss << ", base_array_layer=" << vk_image_subresource_layers.baseArrayLayer;
  ss << ", layer_count=" << vk_image_subresource_layers.layerCount;

  return ss.str();
}

std::string to_string(const VkImageSubresourceRange &vk_image_subresource_range,
                      int indentation_level)
{
  UNUSED_VARS(indentation_level);
  std::stringstream ss;
  ss << "aspect_mask=" << to_string_vk_image_aspect_flags(vk_image_subresource_range.aspectMask);
  ss << ", base_mip_level=" << vk_image_subresource_range.baseMipLevel;
  ss << ", level_count=" << vk_image_subresource_range.levelCount;
  ss << ", base_array_layer=" << vk_image_subresource_range.baseArrayLayer;
  ss << ", layer_count=" << vk_image_subresource_range.layerCount;

  return ss.str();
}

std::string to_string(const VkMemoryBarrier &vk_memory_barrier, int indentation_level)
{
  UNUSED_VARS(indentation_level);
  std::stringstream ss;
  ss << "src_access_mask=" << to_string_vk_access_flags(vk_memory_barrier.srcAccessMask);
  ss << ", dst_access_mask=" << to_string_vk_access_flags(vk_memory_barrier.dstAccessMask);

  return ss.str();
}

std::string to_string(const VkOffset2D &vk_offset2_d, int indentation_level)
{
  UNUSED_VARS(indentation_level);
  std::stringstream ss;
  ss << "x=" << vk_offset2_d.x;
  ss << ", y=" << vk_offset2_d.y;

  return ss.str();
}

std::string to_string(const VkOffset3D &vk_offset3_d, int indentation_level)
{
  UNUSED_VARS(indentation_level);
  std::stringstream ss;
  ss << "x=" << vk_offset3_d.x;
  ss << ", y=" << vk_offset3_d.y;
  ss << ", z=" << vk_offset3_d.z;

  return ss.str();
}

std::string to_string(const VkRect2D &vk_rect2_d, int indentation_level)
{
  std::stringstream ss;
  ss << "offset="
     << "\n";
  ss << std::string(indentation_level * 2 + 2, ' ')
     << to_string(vk_rect2_d.offset, indentation_level + 1);
  ss << std::string(indentation_level * 2, ' ');
  ss << ", extent="
     << "\n";
  ss << std::string(indentation_level * 2 + 2, ' ')
     << to_string(vk_rect2_d.extent, indentation_level + 1);
  ss << std::string(indentation_level * 2, ' ');

  return ss.str();
}

std::string to_string(const VkRenderPassBeginInfo &vk_render_pass_begin_info,
                      int indentation_level)
{
  std::stringstream ss;
  ss << "render_pass=" << vk_render_pass_begin_info.renderPass;
  ss << ", framebuffer=" << vk_render_pass_begin_info.framebuffer;
  ss << ", render_area="
     << "\n";
  ss << std::string(indentation_level * 2 + 2, ' ')
     << to_string(vk_render_pass_begin_info.renderArea, indentation_level + 1);
  ss << std::string(indentation_level * 2, ' ');
  ss << ", clear_value_count=" << vk_render_pass_begin_info.clearValueCount;
  ss << ", p_clear_values=" << vk_render_pass_begin_info.pClearValues;

  return ss.str();
}

}  // namespace blender::gpu
