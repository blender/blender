/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "../vk_command_buffer_wrapper.hh"
#include "../vk_render_graph_links.hh"
#include "../vk_resource_state_tracker.hh"
#include "vk_common.hh"
#include "vk_node_info.hh"

namespace blender::gpu::render_graph {
/**
 * Information stored inside the render graph node. See `VKRenderGraphNode`.
 */
struct VKCopyImageData {
  VkImage src_image;
  VkImage dst_image;
  VkImageCopy region;
};

class VKCopyImageNode : public VKNodeInfo<VKNodeType::COPY_IMAGE,
                                          VKCopyImageData,
                                          VKCopyImageData,
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
    node.copy_image = create_info;
  }

  /**
   * Extract read/write resource dependencies from `create_info` and add them to `node_links`.
   */
  void build_links(VKResourceStateTracker &resources,
                   VKRenderGraphNodeLinks &node_links,
                   const CreateInfo &create_info) override
  {
    ResourceWithStamp src_resource = resources.get_image(create_info.src_image);
    ResourceWithStamp dst_resource = resources.get_image_and_increase_stamp(create_info.dst_image);
    node_links.inputs.append({src_resource,
                              VK_ACCESS_TRANSFER_READ_BIT,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                              create_info.region.srcSubresource.aspectMask});
    node_links.outputs.append({dst_resource,
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               create_info.region.dstSubresource.aspectMask});
  }

  /**
   * Build the commands and add them to the command_buffer.
   */
  void build_commands(VKCommandBufferInterface &command_buffer,
                      const Data &data,
                      VKBoundPipelines & /*r_bound_pipelines*/) override
  {
    command_buffer.copy_image(data.src_image,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                              data.dst_image,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              1,
                              &data.region);
  }
};
}  // namespace blender::gpu::render_graph
