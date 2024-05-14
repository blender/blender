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
struct VKDispatchIndirectData {
  VKPipelineData pipeline_data;
  VkBuffer buffer;
  VkDeviceSize offset;
};

/**
 * Information needed to add a node to the render graph.
 */
struct VKDispatchIndirectCreateInfo : NonCopyable {
  VKDispatchIndirectData dispatch_indirect_node = {};
  const VKResourceAccessInfo &resources;
  VKDispatchIndirectCreateInfo(const VKResourceAccessInfo &resources) : resources(resources) {}
};

/* Although confusing the spec mentions that VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT should also be
 * used for dispatches.
 * (https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPipelineStageFlagBits.html)
 */
class VKDispatchIndirectNode
    : public VKNodeInfo<VKNodeType::DISPATCH_INDIRECT,
                        VKDispatchIndirectCreateInfo,
                        VKDispatchIndirectData,
                        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
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
    node.dispatch_indirect = create_info.dispatch_indirect_node;
    vk_pipeline_data_copy(node.dispatch_indirect.pipeline_data,
                          create_info.dispatch_indirect_node.pipeline_data);
  }

  /**
   * Free the pipeline data stored in the render graph node data.
   */
  void free_data(VKDispatchIndirectData &data)
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
    ResourceWithStamp buffer_resource = resources.get_buffer(
        create_info.dispatch_indirect_node.buffer);
    node_links.inputs.append({buffer_resource, VK_ACCESS_INDIRECT_COMMAND_READ_BIT});
  }

  /**
   * Build the commands and add them to the command_buffer.
   */
  void build_commands(VKCommandBufferInterface &command_buffer,
                      Data &data,
                      VKBoundPipelines &r_bound_pipelines) override
  {
    vk_pipeline_data_build_commands(command_buffer,
                                    data.pipeline_data,
                                    r_bound_pipelines,
                                    VK_PIPELINE_BIND_POINT_COMPUTE,
                                    VK_SHADER_STAGE_COMPUTE_BIT);
    command_buffer.dispatch_indirect(data.buffer, data.offset);
  }
};
}  // namespace blender::gpu::render_graph
