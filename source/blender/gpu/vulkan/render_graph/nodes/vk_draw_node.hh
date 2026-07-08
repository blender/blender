/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "render_graph/nodes/vk_pipeline_data.hh"
#include "render_graph/vk_resource_access_info.hh"
#include "vk_node_info.hh"

namespace blender::gpu::render_graph {

/**
 * Information stored inside the render graph node. See `VKRenderGraphNode`.
 */
struct VKDrawData {
  VKPipelineDataGraphics graphics;
  VKVertexBufferBindings vertex_buffers;
  uint32_t vertex_count;
  uint32_t instance_count;
  uint32_t first_vertex;
  uint32_t first_instance;

  void reset()
  {
    graphics.reset();
    vertex_buffers = {};
    vertex_count = 0;
    instance_count = 0;
    first_vertex = 0;
    first_instance = 0;
  }
};

struct VKDrawCreateInfo {
  const VKResourceAccessInfo &resources;
  VKDrawCreateInfo(const VKResourceAccessInfo &resources) : resources(resources) {}
};

class VKDrawNode : public VKDrawNodeInfo<VKNodeType::DRAW,
                                         VKDrawCreateInfo,
                                         VKDrawData,
                                         VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                                         VKResourceType::IMAGE | VKResourceType::BUFFER> {
 public:
  static void reset_data(Data &data)
  {
    data.reset();
  }

  template<typename Storage>
  static Data &alloc_node_data(Storage &storage, int64_t &r_storage_index)
  {
    Data &data = storage.draw.alloc(r_storage_index);
    reset_data(data);
    return data;
  }

  template<typename Storage> static Data &storage_data(Storage &storage, int64_t storage_index)
  {
    return storage.draw[storage_index];
  }

  /**
   * Extract read/write resource dependencies from `create_info` and add them to `node_links`.
   */
  void build_links(VKResourceStateTracker &resources,
                   VKRenderGraphLinks &links,
                   const CreateInfo &create_info,
                   Data &data) override
  {
    create_info.resources.build_links(resources, links);
    vk_vertex_buffer_bindings_build_links(resources, links, data.vertex_buffers);
  }

  /**
   * Build the commands and add them to the command_buffer.
   */
  void build_commands(VKCommandBufferInterface &command_buffer,
                      Data &data,
                      Span<uint8_t> storage_push_constants,
                      VKBoundPipelines &r_bound_pipelines) override
  {
    vk_pipeline_dynamic_graphics_build_commands(command_buffer, data.graphics, r_bound_pipelines);
    vk_pipeline_data_build_commands(command_buffer,
                                    data.graphics.pipeline_data,
                                    storage_push_constants,
                                    r_bound_pipelines.graphics.pipeline,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    VK_SHADER_STAGE_ALL_GRAPHICS);
    vk_vertex_buffer_bindings_build_commands(
        command_buffer, data.vertex_buffers, r_bound_pipelines.graphics.vertex_buffers);

    command_buffer.draw(
        data.vertex_count, data.instance_count, data.first_vertex, data.first_instance);
  }
};
}  // namespace blender::gpu::render_graph
