/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_resource_access_info.hh"
#include "vk_render_graph_links.hh"
#include "vk_resource_state_tracker.hh"

namespace blender::gpu::render_graph {

VkImageLayout VKImageAccess::to_vk_image_layout() const
{
  if (vk_access_flags & (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) {
    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }

  if (vk_access_flags &
      (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT))
  {
    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }

  if (vk_access_flags &
      (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT))
  {
    return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  }
  BLI_assert_unreachable();
  return VK_IMAGE_LAYOUT_UNDEFINED;
}

/** Which access flags are considered for read access. */
static constexpr VkAccessFlags VK_ACCESS_READ_MASK = VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
                                                     VK_ACCESS_INDEX_READ_BIT |
                                                     VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
                                                     VK_ACCESS_UNIFORM_READ_BIT |
                                                     VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
                                                     VK_ACCESS_SHADER_READ_BIT |
                                                     VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                                     VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                                     VK_ACCESS_TRANSFER_READ_BIT |
                                                     VK_ACCESS_HOST_READ_BIT;

/** Which access flags are considered for write access. */
static constexpr VkAccessFlags VK_ACCESS_WRITE_MASK =
    VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |
    VK_ACCESS_HOST_WRITE_BIT;

void VKResourceAccessInfo::build_links(VKResourceStateTracker &resources,
                                       VKRenderGraphNodeLinks &node_links) const
{
  for (const VKBufferAccess &buffer_access : buffers) {
    VkAccessFlags read_access = buffer_access.vk_access_flags & VK_ACCESS_READ_MASK;
    if (read_access != VK_ACCESS_NONE) {
      ResourceWithStamp versioned_resource = resources.get_buffer(buffer_access.vk_buffer);
      node_links.inputs.append({versioned_resource, read_access, VK_IMAGE_LAYOUT_UNDEFINED});
    }

    VkAccessFlags write_access = buffer_access.vk_access_flags & VK_ACCESS_WRITE_MASK;
    if (write_access != VK_ACCESS_NONE) {
      ResourceWithStamp versioned_resource = resources.get_buffer_and_increase_version(
          buffer_access.vk_buffer);
      node_links.outputs.append({versioned_resource, write_access, VK_IMAGE_LAYOUT_UNDEFINED});
    }
  }

  for (const VKImageAccess &image_access : images) {
    VkImageLayout image_layout = image_access.to_vk_image_layout();
    VkAccessFlags read_access = image_access.vk_access_flags & VK_ACCESS_READ_MASK;
    if (read_access != VK_ACCESS_NONE) {
      ResourceWithStamp versioned_resource = resources.get_image(image_access.vk_image);
      node_links.inputs.append(
          {versioned_resource, read_access, image_layout, image_access.vk_image_aspect});
    }

    VkAccessFlags write_access = image_access.vk_access_flags & VK_ACCESS_WRITE_MASK;
    if (write_access != VK_ACCESS_NONE) {
      ResourceWithStamp versioned_resource = resources.get_image_and_increase_stamp(
          image_access.vk_image);
      node_links.outputs.append(
          {versioned_resource, write_access, image_layout, image_access.vk_image_aspect});
    }
  }
}

void VKResourceAccessInfo::reset()
{
  images.clear();
  buffers.clear();
}

}  // namespace blender::gpu::render_graph
