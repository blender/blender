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
struct VKUpdateBufferData {
  VkBuffer dst_buffer;
  VkDeviceSize dst_offset;
  VkDeviceSize data_size;
  void *data;
};

class VKUpdateBufferNode : public VKNodeInfo<VKNodeType::UPDATE_BUFFER,
                                             VKUpdateBufferData,
                                             VKUpdateBufferData,
                                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                                             VKResourceType::BUFFER> {
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
    node.update_buffer = create_info;
  }

  /**
   * Extract read/write resource dependencies from `create_info` and add them to `node_links`.
   */
  void build_links(VKResourceStateTracker &resources,
                   VKRenderGraphNodeLinks &node_links,
                   const CreateInfo &create_info) override
  {
    ResourceWithStamp dst_resource = resources.get_buffer_and_increase_stamp(
        create_info.dst_buffer);
    node_links.outputs.append({dst_resource, VK_ACCESS_TRANSFER_WRITE_BIT});
  }

  /**
   * Build the commands and add them to the command_buffer.
   */
  void build_commands(VKCommandBufferInterface &command_buffer,
                      Data &data,
                      VKBoundPipelines & /*r_bound_pipelines*/) override
  {
    command_buffer.update_buffer(data.dst_buffer, data.dst_offset, data.data_size, data.data);
  }

  void free_data(Data &data)
  {
    MEM_freeN(data.data);
    data.data = nullptr;
  }
};
}  // namespace blender::gpu::render_graph
