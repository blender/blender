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
struct VKBeginRenderingData {
  VkRenderingAttachmentInfo color_attachments[8];
  VkRenderingAttachmentInfo depth_attachment;
  VkRenderingAttachmentInfo stencil_attachment;
  VkRenderingInfoKHR vk_rendering_info;
};

struct VKBeginRenderingCreateInfo {
  VKBeginRenderingData node_data;
  const VKResourceAccessInfo &resources;
  VKBeginRenderingCreateInfo(const VKResourceAccessInfo &resources) : resources(resources)
  {
    /* Using memset as MSVC didn't clear the color_attachments array. */
    memset(&node_data, 0, sizeof(node_data));
  }
};

/**
 * Begin rendering node
 *
 * - Contains logic to copy relevant data to the VKRenderGraphNode.
 * - Determine read/write resource dependencies.
 * - Add commands to a command builder.
 */
class VKBeginRenderingNode : public VKNodeInfo<VKNodeType::BEGIN_RENDERING,
                                               VKBeginRenderingCreateInfo,
                                               VKBeginRenderingData,
                                               VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                                               VKResourceType::IMAGE> {
 public:
  /**
   * Update the node data with the data inside create_info.
   *
   * Has been implemented as a template to ensure all node specific data
   * (`VK*Data`/`VK*CreateInfo`) types can be included in the same header file as the logic. The
   * actual node data (`VKRenderGraphNode` includes all header files.)
   */
  template<typename Node> void set_node_data(Node &node, const CreateInfo &create_info)
  {
    BLI_assert_msg(ELEM(create_info.node_data.vk_rendering_info.pColorAttachments,
                        nullptr,
                        create_info.node_data.color_attachments),
                   "When create_info.node_data.vk_rendering_info.pColorAttachments points to "
                   "something, it should point to create_info.node_data.color_attachments.");
    BLI_assert_msg(ELEM(create_info.node_data.vk_rendering_info.pDepthAttachment,
                        nullptr,
                        &create_info.node_data.depth_attachment),
                   "When create_info.node_data.vk_rendering_info.pDepthAttachment points to "
                   "something, it should point to create_info.node_data.depth_attachment.");
    BLI_assert_msg(ELEM(create_info.node_data.vk_rendering_info.pStencilAttachment,
                        nullptr,
                        &create_info.node_data.stencil_attachment),
                   "When create_info.node_data.vk_rendering_info.pStencilAttachment points to "
                   "something, it should point to create_info.node_data.stencil_attachment.");
    node.begin_rendering = create_info.node_data;
    /* Localize pointers when set.*/
    if (node.begin_rendering.vk_rendering_info.pColorAttachments) {
      node.begin_rendering.vk_rendering_info.pColorAttachments =
          node.begin_rendering.color_attachments;
    }
    if (node.begin_rendering.vk_rendering_info.pDepthAttachment) {
      node.begin_rendering.vk_rendering_info.pDepthAttachment =
          &node.begin_rendering.depth_attachment;
    }
    if (node.begin_rendering.vk_rendering_info.pStencilAttachment) {
      node.begin_rendering.vk_rendering_info.pStencilAttachment =
          &node.begin_rendering.stencil_attachment;
    }
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
                      VKBoundPipelines & /*r_bound_pipelines*/) override
  {
    command_buffer.begin_rendering(&data.vk_rendering_info);
  }
};
}  // namespace blender::gpu::render_graph
