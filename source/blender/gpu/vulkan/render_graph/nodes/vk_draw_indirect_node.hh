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
struct VKDrawIndirectData {
  VKPipelineDataGraphics graphics;
  VKVertexBufferBindings vertex_buffers;
  VKResourceWithHandle<VkBuffer> indirect_buffer;
  VkDeviceSize offset;
  uint32_t draw_count;
  uint32_t stride;

  void reset()
  {
    graphics.reset();
    vertex_buffers = {};
    indirect_buffer = {};
    offset = 0;
    draw_count = 0;
    stride = 0;
  }
};

struct VKDrawIndirectCreateInfo {
  const VKResourceAccessInfo &resources;
  VKDrawIndirectCreateInfo(const VKResourceAccessInfo &resources) : resources(resources) {}
};

class VKDrawIndirectNode : public VKDrawNodeInfo<VKNodeType::DRAW_INDIRECT,
                                                 VKDrawIndirectCreateInfo,
                                                 VKDrawIndirectData,
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
    Data &data = storage.draw_indirect.alloc(r_storage_index);
    reset_data(data);
    return data;
  }

  template<typename Storage> static Data &storage_data(Storage &storage, int64_t storage_index)
  {
    return storage.draw_indirect[storage_index];
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
    ResourceWithStamp buffer_resource = resources.get_buffer(data.indirect_buffer);
    links.buffers.append({buffer_resource, VK_ACCESS_INDIRECT_COMMAND_READ_BIT});
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

    command_buffer.draw_indirect(data.indirect_buffer, data.offset, data.draw_count, data.stride);
  }
};
}  // namespace blender::gpu::render_graph
