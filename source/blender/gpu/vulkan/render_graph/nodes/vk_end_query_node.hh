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
struct VKEndQueryData {
  VkQueryPool vk_query_pool;
  uint32_t query_index;
};

/**
 * End Query Node
 *
 * - Contains logic to copy relevant data to the VKRenderGraphNode.
 * - Determine read/write resource dependencies.
 * - Add commands to a command builder.
 */
class VKEndQueryNode : public VKNodeInfo<VKNodeType::END_QUERY,
                                         VKEndQueryData,
                                         VKEndQueryData,
                                         VK_PIPELINE_STAGE_NONE,
                                         VKResourceType::IMAGE> {
 public:
  /**
   * Update the node data with the data inside create_info.
   *
   * Has been implemented as a template to ensure all node specific data
   * (`VK*Data`/`VK*CreateInfo`) types can be included in the same header file as the logic. The
   * actual node data (`VKRenderGraphNode` includes all header files.)
   */
  template<typename Node> void set_node_data(Node &node, const CreateInfo &create_info)
  {
    node.end_query = create_info;
  }

  /**
   * Extract read/write resource dependencies from `create_info` and add them to `node_links`.
   */
  void build_links(VKResourceStateTracker & /*resources*/,
                   VKRenderGraphNodeLinks & /*node_links*/,
                   const CreateInfo & /*create_info*/) override
  {
  }

  /**
   * Build the commands and add them to the command_buffer.
   */
  void build_commands(VKCommandBufferInterface &command_buffer,
                      Data &data,
                      VKBoundPipelines & /*r_bound_pipelines*/) override
  {
    command_buffer.end_query(data.vk_query_pool, data.query_index);
  }
};
}  // namespace blender::gpu::render_graph
