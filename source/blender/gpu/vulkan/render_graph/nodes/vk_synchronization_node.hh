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
struct VKSynchronizationData {};

/**
 * Information needed to add a node to the render graph.
 */
struct VKSynchronizationCreateInfo {
  VkImage vk_image;
  VkImageLayout vk_image_layout;
};

class VKSynchronizationNode : public VKNodeInfo<VKNodeType::SYNCHRONIZATION,
                                                VKSynchronizationCreateInfo,
                                                VKSynchronizationData,
                                                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                                VKResourceType::IMAGE | VKResourceType::BUFFER> {
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
    UNUSED_VARS(create_info);
    node.synchronization = {};
  }

  /**
   * Extract read/write resource dependencies from `create_info` and add them to `node_links`.
   */
  void build_links(VKResourceStateTracker &resources,
                   VKRenderGraphNodeLinks &node_links,
                   const CreateInfo &create_info) override
  {
    ResourceWithStamp resource = resources.get_image_and_increase_stamp(create_info.vk_image);
    node_links.outputs.append(
        {resource, VK_ACCESS_TRANSFER_WRITE_BIT, create_info.vk_image_layout});
  }

  /**
   * Build the commands and add them to the command_buffer.
   */
  void build_commands(VKCommandBufferInterface &command_buffer,
                      const Data &data,
                      VKBoundPipelines & /*r_bound_pipelines*/) override
  {
    UNUSED_VARS(command_buffer, data);
    /* Intentionally left empty: A pipeline barrier has already been send to the command buffer.
     */
  }
};
}  // namespace blender::gpu::render_graph
