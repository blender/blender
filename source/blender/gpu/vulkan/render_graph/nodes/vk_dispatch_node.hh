/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "render_graph/vk_render_graph_links.hh"
#include "render_graph/vk_resource_access_info.hh"
#include "vk_node_info.hh"

namespace blender::gpu::render_graph {
/**
 * Information stored inside the render graph node. See `VKRenderGraphNode`.
 */
struct VKDispatchData {
  VKPipelineData pipeline_data;
  uint32_t group_count_x;
  uint32_t group_count_y;
  uint32_t group_count_z;
};

/**
 * Information needed to add a node to the render graph.
 */
struct VKDispatchCreateInfo : NonCopyable {
  VKDispatchData dispatch_node = {};
  const VKResourceAccessInfo &resources;
  VKDispatchCreateInfo(const VKResourceAccessInfo &resources) : resources(resources) {}
};

class VKDispatchNode : public VKNodeInfo<VKNodeType::DISPATCH,
                                         VKDispatchCreateInfo,
                                         VKDispatchData,
                                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
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
    node.dispatch = create_info.dispatch_node;
    vk_pipeline_data_copy(node.dispatch.pipeline_data, create_info.dispatch_node.pipeline_data);
  }

  /**
   * Free the pipeline data stored in the render graph node data.
   */
  void free_data(VKDispatchData &data)
  {
    vk_pipeline_data_free(data.pipeline_data);
  }

  /**
   * Extract read/write resource dependencies from `create_info` and add them to `node_links`.
   */
  void build_links(VKResourceStateTracker &resources,
                   VKRenderGraphNodeLinks &node_links,
                   const CreateInfo &create_info) override
  {
    create_info.resources.build_links(resources, node_links);
  }

  /**
   * Build the commands and add them to the command_buffer.
   */
  void build_commands(VKCommandBufferInterface &command_buffer,
                      const Data &data,
                      VKBoundPipelines &r_bound_pipelines) override
  {
    vk_pipeline_data_build_commands(
        command_buffer, data.pipeline_data, r_bound_pipelines, VK_PIPELINE_BIND_POINT_COMPUTE);
    command_buffer.dispatch(data.group_count_x, data.group_count_y, data.group_count_z);
  }
};
}  // namespace blender::gpu::render_graph
