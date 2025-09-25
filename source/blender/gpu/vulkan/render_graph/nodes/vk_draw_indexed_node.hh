/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "render_graph/vk_resource_access_info.hh"
#include "vk_node_info.hh"

namespace blender::gpu::render_graph {

/**
 * Information stored inside the render graph node. See `VKRenderGraphNode`.
 */
struct VKDrawIndexedData {
  VKPipelineDataGraphics graphics;
  VKIndexBufferBinding index_buffer;
  VKVertexBufferBindings vertex_buffers;
  uint32_t index_count;
  uint32_t instance_count;
  uint32_t first_index;
  uint32_t vertex_offset;
  uint32_t first_instance;
};

struct VKDrawIndexedCreateInfo {
  VKDrawIndexedData node_data = {};
  const VKResourceAccessInfo &resources;
  VKDrawIndexedCreateInfo(const VKResourceAccessInfo &resources) : resources(resources) {}
};

class VKDrawIndexedNode : public VKNodeInfo<VKNodeType::DRAW_INDEXED,
                                            VKDrawIndexedCreateInfo,
                                            VKDrawIndexedData,
                                            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
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
    node.storage_index = storage.draw_indexed.append_and_get_index(create_info.node_data);
    vk_pipeline_data_copy(storage.draw_indexed[node.storage_index].graphics,
                          create_info.node_data.graphics);
  }

  /**
   * Extract read/write resource dependencies from `create_info` and add them to `node_links`.
   */
  void build_links(VKResourceStateTracker &resources,
                   VKRenderGraphNodeLinks &node_links,
                   const CreateInfo &create_info) override
  {
    create_info.resources.build_links(resources, node_links);
    vk_index_buffer_binding_build_links(resources, node_links, create_info.node_data.index_buffer);
    vk_vertex_buffer_bindings_build_links(
        resources, node_links, create_info.node_data.vertex_buffers);
  }

  /**
   * Build the commands and add them to the command_buffer.
   */
  void build_commands(VKCommandBufferInterface &command_buffer,
                      Data &data,
                      VKBoundPipelines &r_bound_pipelines) override
  {
    vk_pipeline_dynamic_graphics_build_commands(
        command_buffer, data.graphics.viewport, data.graphics.line_width, r_bound_pipelines);
    vk_pipeline_data_build_commands(command_buffer,
                                    data.graphics.pipeline_data,
                                    r_bound_pipelines.graphics.pipeline,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    VK_SHADER_STAGE_ALL_GRAPHICS);
    vk_index_buffer_binding_build_commands(
        command_buffer, data.index_buffer, r_bound_pipelines.graphics.index_buffer);
    vk_vertex_buffer_bindings_build_commands(
        command_buffer, data.vertex_buffers, r_bound_pipelines.graphics.vertex_buffers);
    command_buffer.draw_indexed(data.index_count,
                                data.instance_count,
                                data.first_index,
                                data.vertex_offset,
                                data.first_instance);
  }

  void free_data(Data &data)
  {
    vk_pipeline_data_free(data.graphics);
  }
};
}  // namespace blender::gpu::render_graph
