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
struct VKCopyBufferToImageData {
  VkBuffer src_buffer;
  VkImage dst_image;
  VkBufferImageCopy region;
};

struct VKCopyBufferToImageCreateInfo {
  VKCopyBufferToImageData node_data;
  VkImageAspectFlags vk_image_aspects;
};

class VKCopyBufferToImageNode : public VKNodeInfo<VKNodeType::COPY_BUFFER_TO_IMAGE,
                                                  VKCopyBufferToImageCreateInfo,
                                                  VKCopyBufferToImageData,
                                                  VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                  VKResourceType::IMAGE | VKResourceType::BUFFER> {
 public:
  /**
   * Update the node data with the data inside create_info.
   *
   * Has been implemented as a template to ensure all node specific data
   * (`VK*Data`/`VK*CreateInfo`) types can be included in the same header file as the logic. The
   * actual node data (`VKRenderGraphNode` includes all header files.)
   */
  template<typename Node, typename Storage>
  static void set_node_data(Node &node, Storage &storage, const CreateInfo &create_info)
  {
    node.storage_index = storage.copy_buffer_to_image.append_and_get_index(create_info.node_data);
  }

  /**
   * Extract read/write resource dependencies from `create_info` and add them to `node_links`.
   */
  void build_links(VKResourceStateTracker &resources,
                   VKRenderGraphNodeLinks &node_links,
                   const CreateInfo &create_info) override
  {
    ResourceWithStamp src_resource = resources.get_buffer(create_info.node_data.src_buffer);
    ResourceWithStamp dst_resource = resources.get_image_and_increase_stamp(
        create_info.node_data.dst_image);
    node_links.inputs.append(
        {src_resource, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED});
    node_links.outputs.append({dst_resource,
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               create_info.vk_image_aspects});
  }

  /**
   * Build the commands and add them to the command_buffer.
   */
  void build_commands(VKCommandBufferInterface &command_buffer,
                      Data &data,
                      VKBoundPipelines & /*r_bound_pipelines*/) override
  {
    command_buffer.copy_buffer_to_image(
        data.src_buffer, data.dst_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &data.region);
  }
};
}  // namespace blender::gpu::render_graph
