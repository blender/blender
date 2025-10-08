/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_resource_access_info.hh"
#include "vk_backend.hh"
#include "vk_render_graph_links.hh"
#include "vk_resource_state_tracker.hh"

namespace blender::gpu::render_graph {

VkImageLayout VKImageAccess::to_vk_image_layout(bool supports_local_read) const
{
  if (vk_access_flags & (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) {
    /* TODO: when read only use VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL */
    return VK_IMAGE_LAYOUT_GENERAL;
  }

  if (supports_local_read && vk_access_flags & (VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
                                                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT))
  {
    return VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR;
  }
  else if (vk_access_flags &
           (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT))
  {
    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }

  // TODO: Add ATTACHMENT_READ_ONLY_OPTIMAL
  if (vk_access_flags &
      (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT))
  {
    if (vk_image_aspect == VK_IMAGE_ASPECT_DEPTH_BIT) {
      return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    }
    if (vk_image_aspect == VK_IMAGE_ASPECT_STENCIL_BIT) {
      return VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
    }
    BLI_assert(vk_image_aspect == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT));
    return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  }
  BLI_assert_unreachable();
  return VK_IMAGE_LAYOUT_UNDEFINED;
}

/** Which access flags are considered for write access. */
static constexpr VkAccessFlags VK_ACCESS_WRITE_MASK =
    VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |
    VK_ACCESS_HOST_WRITE_BIT;

void VKResourceAccessInfo::build_links(VKResourceStateTracker &resources,
                                       VKRenderGraphNodeLinks &node_links) const
{
  for (const VKBufferAccess &buffer_access : buffers) {
    const bool writes_to_resource = bool(buffer_access.vk_access_flags & VK_ACCESS_WRITE_MASK);
    ResourceWithStamp versioned_resource = writes_to_resource ?
                                               resources.get_buffer_and_increase_stamp(
                                                   buffer_access.vk_buffer) :
                                               resources.get_buffer(buffer_access.vk_buffer);
    if (writes_to_resource) {
      node_links.outputs.append(
          {versioned_resource, buffer_access.vk_access_flags, VK_IMAGE_LAYOUT_UNDEFINED});
    }
    else {
      node_links.inputs.append(
          {versioned_resource, buffer_access.vk_access_flags, VK_IMAGE_LAYOUT_UNDEFINED});
    }
  }

  const bool supports_local_read = resources.use_dynamic_rendering_local_read;

  for (const VKImageAccess &image_access : images) {
    VkImageLayout image_layout = image_access.to_vk_image_layout(supports_local_read);
    const bool writes_to_resource = bool(image_access.vk_access_flags & VK_ACCESS_WRITE_MASK);
    ResourceWithStamp versioned_resource = writes_to_resource ?
                                               resources.get_image_and_increase_stamp(
                                                   image_access.vk_image) :
                                               resources.get_image(image_access.vk_image);
    if (writes_to_resource) {
      node_links.outputs.append({versioned_resource,
                                 image_access.vk_access_flags,
                                 image_layout,
                                 image_access.vk_image_aspect,
                                 image_access.subimage});
    }
    else {
      node_links.inputs.append({versioned_resource,
                                image_access.vk_access_flags,
                                image_layout,
                                image_access.vk_image_aspect,
                                image_access.subimage});
    }
  }
}

void VKResourceAccessInfo::reset()
{
  images.clear();
  buffers.clear();
}

}  // namespace blender::gpu::render_graph
