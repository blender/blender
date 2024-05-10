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
struct VKClearAttachmentsData {
  uint32_t attachment_count;
  VkClearAttachment attachments[8];
  VkClearRect vk_clear_rect;
};

class VKClearAttachmentsNode : public VKNodeInfo<VKNodeType::CLEAR_ATTACHMENTS,
                                                 VKClearAttachmentsData,
                                                 VKClearAttachmentsData,
                                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                                     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                                     VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                                                 VKResourceType::IMAGE> {
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
    node.clear_attachments = create_info;
  }

  /**
   * Extract read/write resource dependencies from `create_info` and add them to `node_links`.
   */
  void build_links(VKResourceStateTracker &resources,
                   VKRenderGraphNodeLinks &node_links,
                   const CreateInfo &create_info) override
  {
    UNUSED_VARS(resources, node_links, create_info);
  }

  /**
   * Build the commands and add them to the command_buffer.
   */
  void build_commands(VKCommandBufferInterface &command_buffer,
                      const Data &data,
                      VKBoundPipelines & /*r_bound_pipelines*/) override
  {
    command_buffer.clear_attachments(
        data.attachment_count, data.attachments, 1, &data.vk_clear_rect);
  }
};
}  // namespace blender::gpu::render_graph
