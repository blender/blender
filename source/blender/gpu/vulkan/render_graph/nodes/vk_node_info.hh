/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "render_graph/vk_command_buffer_wrapper.hh"
#include "render_graph/vk_render_graph_links.hh"
#include "render_graph/vk_resource_state_tracker.hh"
#include "vk_common.hh"
#include "vk_pipeline_data.hh"

namespace blender::gpu::render_graph {

/**
 * Type of nodes of the render graph.
 */
enum class VKNodeType {
  UNUSED,
  BEGIN_RENDERING,
  END_RENDERING,
  CLEAR_ATTACHMENTS,
  CLEAR_COLOR_IMAGE,
  CLEAR_DEPTH_STENCIL_IMAGE,
  FILL_BUFFER,
  COPY_BUFFER,
  COPY_IMAGE,
  COPY_IMAGE_TO_BUFFER,
  COPY_BUFFER_TO_IMAGE,
  BLIT_IMAGE,
  DISPATCH,
  DISPATCH_INDIRECT,
  SYNCHRONIZATION,
};

/**
 * Info class for a node type.
 *
 * Nodes can be created using `NodeCreateInfo`. When a node is created the `VKNodeInfo.node_type`
 * and `VKNodeInfo.set_node_data` are used to fill a VKRenderGraphNode instance. The
 * VKRenderGraphNode is stored sequentially in the render graph. When the node is created the
 * dependencies are extracted by calling `VKNodeInfo.build_links`.
 *
 * Eventually when a node is recorded to a command buffer `VKNodeInfo.build_commands` is invoked.
 */
template<VKNodeType NodeType,
         typename NodeCreateInfo,
         typename NodeData,
         VkPipelineStageFlags PipelineStage,
         VKResourceType ResourceUsages>
class VKNodeInfo : public NonCopyable {

 public:
  using CreateInfo = NodeCreateInfo;
  using Data = NodeData;

  /**
   * Node type of this class.
   *
   * The node type used to link VKRenderGraphNode instance to a VKNodeInfo.
   */
  static constexpr VKNodeType node_type = NodeType;

  /**
   * Which pipeline stage does this command belongs to. The pipeline stage is used when generating
   * pipeline barriers.
   */
  static constexpr VkPipelineStageFlags pipeline_stage = PipelineStage;

  /**
   * Which resource types are relevant. Some code can be skipped when a node can only depend on
   * resources of a single type.
   */
  static constexpr VKResourceType resource_usages = ResourceUsages;

  /**
   * Update the node data with the data inside create_info.
   *
   * Has been implemented as a template to ensure all node specific data
   * (`Data`/`CreateInfo`) types can be included in the same header file as the logic. The
   * actual node data (`VKRenderGraphNode` includes all header files.)
   *
   * This function must be implemented by all node classes. But due to cyclic inclusion of header
   * files it is implemented as a template function.
   */
  template<typename Node> static void set_node_data(Node &node, const CreateInfo &create_info);

  /**
   * Extract read/write resource dependencies from `create_info` and add them to `node_links`.
   */
  virtual void build_links(VKResourceStateTracker &resources,
                           VKRenderGraphNodeLinks &node_links,
                           const CreateInfo &create_info) = 0;

  /**
   * Build the commands and add them to the command_buffer.
   *
   * The command buffer is passed as an interface as this is replaced by a logger when running test
   * cases. The test cases will validate the log to find out if the correct commands where added.
   */
  virtual void build_commands(VKCommandBufferInterface &command_buffer,
                              const Data &data,
                              VKBoundPipelines &r_bound_pipelines) = 0;
};
}  // namespace blender::gpu::render_graph
