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
struct VKResetQueryPoolData {
  VkQueryPool vk_query_pool;
  uint32_t first_query;
  uint32_t query_count;
};

/**
 * Reset query pool.
 *
 * - Contains logic to copy relevant data to the VKRenderGraphNode.
 * - Determine read/write resource dependencies.
 * - Add commands to a command builder.
 */
class VKResetQueryPoolNode : public VKNodeInfo<VKNodeType::RESET_QUERY_POOL,
                                               VKResetQueryPoolData,
                                               VKResetQueryPoolData,
                                               VK_PIPELINE_STAGE_NONE,
                                               VKResourceType::NONE> {
 public:
  /**
   * Update the node data with the data inside create_info.
   *
   * Has been implemented as a template to ensure all node specific data
   * (`VK*Data`/`VK*CreateInfo`) types can be included in the same header file as the logic. The
   * actual node data (`VKRenderGraphNode` includes all header files.)
   */
  template<typename Node, typename Storage>
  void set_node_data(Node &node, Storage & /*storage*/, const CreateInfo &create_info)
  {
    node.reset_query_pool = create_info;
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
    command_buffer.reset_query_pool(data.vk_query_pool, data.first_query, data.query_count);
  }
};
}  // namespace blender::gpu::render_graph
