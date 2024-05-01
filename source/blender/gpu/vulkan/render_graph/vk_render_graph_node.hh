/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "nodes/vk_blit_image_node.hh"
#include "nodes/vk_clear_color_image_node.hh"
#include "nodes/vk_clear_depth_stencil_image_node.hh"
#include "nodes/vk_copy_buffer_node.hh"
#include "nodes/vk_copy_buffer_to_image_node.hh"
#include "nodes/vk_copy_image_node.hh"
#include "nodes/vk_copy_image_to_buffer_node.hh"
#include "nodes/vk_dispatch_indirect_node.hh"
#include "nodes/vk_dispatch_node.hh"
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
    VKClearColorImageNode::Data clear_color_image;
    VKClearDepthStencilImageNode::Data clear_depth_stencil_image;
    VKCopyBufferNode::Data copy_buffer;
    VKCopyBufferToImageNode::Data copy_buffer_to_image;
    VKCopyImageNode::Data copy_image;
    VKCopyImageToBufferNode::Data copy_image_to_buffer;
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
      case VKNodeType::CLEAR_COLOR_IMAGE:
        return VKClearColorImageNode::pipeline_stage;
      case VKNodeType::CLEAR_DEPTH_STENCIL_IMAGE:
        return VKClearDepthStencilImageNode::pipeline_stage;
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
                      VKBoundPipelines &r_bound_pipelines) const
  {
    switch (type) {
      case VKNodeType::UNUSED: {
        break;
      }

      case VKNodeType::CLEAR_COLOR_IMAGE: {
        VKClearColorImageNode node_info;
        node_info.build_commands(command_buffer, clear_color_image, r_bound_pipelines);
        break;
      }

      case VKNodeType::CLEAR_DEPTH_STENCIL_IMAGE: {
        VKClearDepthStencilImageNode node_info;
        node_info.build_commands(command_buffer, clear_depth_stencil_image, r_bound_pipelines);
        break;
      }

      case VKNodeType::FILL_BUFFER: {
        VKFillBufferNode node_info;
        node_info.build_commands(command_buffer, fill_buffer, r_bound_pipelines);
        break;
      }

      case VKNodeType::COPY_BUFFER: {
        VKCopyBufferNode node_info;
        node_info.build_commands(command_buffer, copy_buffer, r_bound_pipelines);
        break;
      }

      case VKNodeType::COPY_BUFFER_TO_IMAGE: {
        VKCopyBufferToImageNode node_info;
        node_info.build_commands(command_buffer, copy_buffer_to_image, r_bound_pipelines);
        break;
      }

      case VKNodeType::COPY_IMAGE: {
        VKCopyImageNode node_info;
        node_info.build_commands(command_buffer, copy_image, r_bound_pipelines);
        break;
      }

      case VKNodeType::COPY_IMAGE_TO_BUFFER: {
        VKCopyImageToBufferNode node_info;
        node_info.build_commands(command_buffer, copy_image_to_buffer, r_bound_pipelines);
        break;
      }

      case VKNodeType::BLIT_IMAGE: {
        VKBlitImageNode node_info;
        node_info.build_commands(command_buffer, blit_image, r_bound_pipelines);
        break;
      }

      case VKNodeType::SYNCHRONIZATION: {
        VKSynchronizationNode node_info;
        node_info.build_commands(command_buffer, synchronization, r_bound_pipelines);
        break;
      }

      case VKNodeType::DISPATCH: {
        VKDispatchNode node_info;
        node_info.build_commands(command_buffer, dispatch, r_bound_pipelines);
        break;
      }

      case VKNodeType::DISPATCH_INDIRECT: {
        VKDispatchIndirectNode node_info;
        node_info.build_commands(command_buffer, dispatch_indirect, r_bound_pipelines);
        break;
      }
    }
  }

  /**
   * Free data kept by the node
   */
  void free_data()
  {
    switch (type) {
      case VKNodeType::DISPATCH: {
        VKDispatchNode node_info;
        node_info.free_data(dispatch);
        break;
      }

      case VKNodeType::DISPATCH_INDIRECT: {
        VKDispatchIndirectNode node_info;
        node_info.free_data(dispatch_indirect);
        break;
      }

      case VKNodeType::UNUSED:
      case VKNodeType::CLEAR_COLOR_IMAGE:
      case VKNodeType::CLEAR_DEPTH_STENCIL_IMAGE:
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
