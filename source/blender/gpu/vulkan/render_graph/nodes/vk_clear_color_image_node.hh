/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "vk_node_info.hh"

namespace blender::gpu::render_graph {

/**
 * Information stored inside the render graph node. See `VKRenderGraphNode`.
 */
struct VKClearColorImageData {
  VkImage vk_image;
  VkClearColorValue vk_clear_color_value;
  VkImageSubresourceRange vk_image_subresource_range;
};

class VKClearColorImageNode : public VKNodeInfo<VKNodeType::CLEAR_COLOR_IMAGE,
                                                VKClearColorImageData,
                                                VKClearColorImageData,
                                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                VKResourceType::IMAGE> {
 public:
  /**
   * Update the node data with the data inside create_info.
   *
   * Has been implemented as a template to ensure all node specific data
   * (`VK*Data`/`VK*CreateInfo`) types can be included in the same header file as the logic. The
   * actual node data (`VKRenderGraphNode` includes all header files.)
   */
  template<typename Node> static void set_node_data(Node &node, const CreateInfo &create_info)
  {
    node.clear_color_image = create_info;
  }

  /**
   * Extract read/write resource dependencies from `create_info` and add them to `node_links`.
   */
  void build_links(VKResourceStateTracker &resources,
                   VKRenderGraphNodeLinks &node_links,
                   const CreateInfo &create_info) override
  {
    ResourceWithStamp resource = resources.get_image_and_increase_stamp(create_info.vk_image);
    node_links.outputs.append({resource,
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_IMAGE_ASPECT_COLOR_BIT});
  }

  /**
   * Build the commands and add them to the command_buffer.
   */
  void build_commands(VKCommandBufferInterface &command_buffer,
                      const Data &data,
                      VKBoundPipelines & /*r_bound_pipelines*/) override
  {
    command_buffer.clear_color_image(data.vk_image,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     &data.vk_clear_color_value,
                                     1,
                                     &data.vk_image_subresource_range);
  }
};
}  // namespace blender::gpu::render_graph
