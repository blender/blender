/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "nodes/vk_begin_rendering_node.hh"
#include "nodes/vk_blit_image_node.hh"
#include "nodes/vk_clear_attachments_node.hh"
#include "nodes/vk_clear_color_image_node.hh"
#include "nodes/vk_clear_depth_stencil_image_node.hh"
#include "nodes/vk_copy_buffer_node.hh"
#include "nodes/vk_copy_buffer_to_image_node.hh"
#include "nodes/vk_copy_image_node.hh"
#include "nodes/vk_copy_image_to_buffer_node.hh"
#include "nodes/vk_dispatch_indirect_node.hh"
#include "nodes/vk_dispatch_node.hh"
#include "nodes/vk_end_rendering_node.hh"
#include "nodes/vk_fill_buffer_node.hh"
#include "nodes/vk_synchronization_node.hh"

namespace blender::gpu::render_graph {

/**
 * Index of a node inside the render graph.
 */
using NodeHandle = uint64_t;

/**
 * Node stored inside a render graph.
 *
 * Node specific data in the render graph are stored in a vector to ensure that the data can be
 * prefetched and removing a level of indirection. A consequence is that we cannot use class based
 * nodes.
 */
struct VKRenderGraphNode {
  VKNodeType type;
  union {
    VKBlitImageNode::Data blit_image;
    VKBeginRenderingNode::Data begin_rendering;
    VKClearAttachmentsNode::Data clear_attachments;
    VKClearColorImageNode::Data clear_color_image;
    VKClearDepthStencilImageNode::Data clear_depth_stencil_image;
    VKCopyBufferNode::Data copy_buffer;
    VKCopyBufferToImageNode::Data copy_buffer_to_image;
    VKCopyImageNode::Data copy_image;
    VKCopyImageToBufferNode::Data copy_image_to_buffer;
    VKEndRenderingNode::Data end_rendering;
    VKDispatchNode::Data dispatch;
    VKDispatchIndirectNode::Data dispatch_indirect;
    VKFillBufferNode::Data fill_buffer;
    VKSynchronizationNode::Data synchronization;
  };

  /**
   * Set the data of the node.
   *
   * Pre-conditions:
   * - type of the node should be `VKNodeType::UNUSED`. Memory allocated for nodes are reused
   *   between consecutive use. Checking for unused node types will ensure that previous usage has
   *   been reset. Resetting is done as part of `free_data`
   */
  template<typename NodeInfo> void set_node_data(const typename NodeInfo::CreateInfo &create_info)
  {
    BLI_assert(type == VKNodeType::UNUSED);
    /* Instance of NodeInfo is needed to call virtual methods. CPP doesn't support overloading of
     * static methods.*/
    NodeInfo node_info;
    type = NodeInfo::node_type;
    node_info.set_node_data(*this, create_info);
  }

  /**
   * Build the input/output links for this.
   *
   * Newly created links are added to the `node_links` parameter.
   */
  template<typename NodeInfo>
  void build_links(VKResourceStateTracker &resources,
                   VKRenderGraphNodeLinks &node_links,
                   const typename NodeInfo::CreateInfo &create_info)
  {
    /* Instance of NodeInfo is needed to call virtual methods. CPP doesn't support overloading of
     * static methods.*/
    NodeInfo node_info;
    node_info.build_links(resources, node_links, create_info);
  }

  /**
   * Get the pipeline stage of the node.
   *
   * Pipeline stage is used to update `src/dst_stage_masks` of the VKCommandBuilder.
   */
  VkPipelineStageFlags pipeline_stage_get() const
  {
    switch (type) {
      case VKNodeType::UNUSED:
        return VK_PIPELINE_STAGE_NONE;
      case VKNodeType::BEGIN_RENDERING:
        return VKBeginRenderingNode::pipeline_stage;
      case VKNodeType::CLEAR_ATTACHMENTS:
        return VKClearAttachmentsNode::pipeline_stage;
      case VKNodeType::CLEAR_COLOR_IMAGE:
        return VKClearColorImageNode::pipeline_stage;
      case VKNodeType::CLEAR_DEPTH_STENCIL_IMAGE:
        return VKClearDepthStencilImageNode::pipeline_stage;
      case VKNodeType::END_RENDERING:
        return VKEndRenderingNode::pipeline_stage;
      case VKNodeType::FILL_BUFFER:
        return VKFillBufferNode::pipeline_stage;
      case VKNodeType::COPY_BUFFER:
        return VKCopyBufferNode::pipeline_stage;
      case VKNodeType::COPY_IMAGE:
        return VKCopyImageNode::pipeline_stage;
      case VKNodeType::COPY_IMAGE_TO_BUFFER:
        return VKCopyImageToBufferNode::pipeline_stage;
      case VKNodeType::COPY_BUFFER_TO_IMAGE:
        return VKCopyBufferToImageNode::pipeline_stage;
      case VKNodeType::BLIT_IMAGE:
        return VKBlitImageNode::pipeline_stage;
      case VKNodeType::DISPATCH:
        return VKDispatchNode::pipeline_stage;
      case VKNodeType::DISPATCH_INDIRECT:
        return VKDispatchIndirectNode::pipeline_stage;
      case VKNodeType::SYNCHRONIZATION:
        return VKSynchronizationNode::pipeline_stage;
    }
    BLI_assert_unreachable();
    return VK_PIPELINE_STAGE_NONE;
  }

  /**
   * Build commands for this node and record them in the given command_buffer.
   *
   * NOTE: Pipeline barriers should already be added. See
   * `VKCommandBuilder::build_node` and `VKCommandBuilder::build_pipeline_barriers.
   */
  void build_commands(VKCommandBufferInterface &command_buffer,
                      VKBoundPipelines &r_bound_pipelines)
  {
    switch (type) {
      case VKNodeType::UNUSED: {
        break;
      }
#define BUILD_COMMANDS(NODE_TYPE, NODE_CLASS, ATTRIBUTE_NAME) \
  case NODE_TYPE: { \
    NODE_CLASS node_info; \
    node_info.build_commands(command_buffer, ATTRIBUTE_NAME, r_bound_pipelines); \
    break; \
  }

        BUILD_COMMANDS(VKNodeType::BEGIN_RENDERING, VKBeginRenderingNode, begin_rendering)
        BUILD_COMMANDS(VKNodeType::CLEAR_ATTACHMENTS, VKClearAttachmentsNode, clear_attachments)
        BUILD_COMMANDS(VKNodeType::CLEAR_COLOR_IMAGE, VKClearColorImageNode, clear_color_image)
        BUILD_COMMANDS(VKNodeType::CLEAR_DEPTH_STENCIL_IMAGE,
                       VKClearDepthStencilImageNode,
                       clear_depth_stencil_image)
        BUILD_COMMANDS(VKNodeType::END_RENDERING, VKEndRenderingNode, end_rendering)
        BUILD_COMMANDS(VKNodeType::FILL_BUFFER, VKFillBufferNode, fill_buffer)
        BUILD_COMMANDS(VKNodeType::COPY_BUFFER, VKCopyBufferNode, copy_buffer)
        BUILD_COMMANDS(
            VKNodeType::COPY_BUFFER_TO_IMAGE, VKCopyBufferToImageNode, copy_buffer_to_image)
        BUILD_COMMANDS(VKNodeType::COPY_IMAGE, VKCopyImageNode, copy_image)
        BUILD_COMMANDS(
            VKNodeType::COPY_IMAGE_TO_BUFFER, VKCopyImageToBufferNode, copy_image_to_buffer)
        BUILD_COMMANDS(VKNodeType::BLIT_IMAGE, VKBlitImageNode, blit_image)
        BUILD_COMMANDS(VKNodeType::SYNCHRONIZATION, VKSynchronizationNode, synchronization)
        BUILD_COMMANDS(VKNodeType::DISPATCH, VKDispatchNode, dispatch)
        BUILD_COMMANDS(VKNodeType::DISPATCH_INDIRECT, VKDispatchIndirectNode, dispatch_indirect)
#undef BUILD_COMMANDS
    }
  }

  /**
   * Free data kept by the node
   */
  void free_data()
  {
    switch (type) {

#define FREE_DATA(NODE_TYPE, NODE_CLASS, ATTRIBUTE_NAME) \
  case NODE_TYPE: { \
    NODE_CLASS node_info; \
    node_info.free_data(ATTRIBUTE_NAME); \
    break; \
  }

      FREE_DATA(VKNodeType::DISPATCH, VKDispatchNode, dispatch)
      FREE_DATA(VKNodeType::DISPATCH_INDIRECT, VKDispatchIndirectNode, dispatch_indirect)
#undef FREE_DATA

      case VKNodeType::UNUSED:
      case VKNodeType::BEGIN_RENDERING:
      case VKNodeType::CLEAR_ATTACHMENTS:
      case VKNodeType::CLEAR_COLOR_IMAGE:
      case VKNodeType::CLEAR_DEPTH_STENCIL_IMAGE:
      case VKNodeType::END_RENDERING:
      case VKNodeType::FILL_BUFFER:
      case VKNodeType::COPY_BUFFER:
      case VKNodeType::COPY_IMAGE:
      case VKNodeType::COPY_IMAGE_TO_BUFFER:
      case VKNodeType::COPY_BUFFER_TO_IMAGE:
      case VKNodeType::BLIT_IMAGE:
      case VKNodeType::SYNCHRONIZATION:
        break;
    }
  }

  /**
   * Reset nodes.
   *
   * Nodes are reset so they can be reused in consecutive calls. Data allocated by the node are
   * freed. This function dispatches the free_data to the actual node implementation.
   */
  void reset()
  {
    free_data();
    memset(this, 0, sizeof(VKRenderGraphNode));
    type = VKNodeType::UNUSED;
  }
};

}  // namespace blender::gpu::render_graph
