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
  VKBeginRenderingData node_data = {};
  const VKResourceAccessInfo &resources;
  VKBeginRenderingCreateInfo(const VKResourceAccessInfo &resources) : resources(resources) {}
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
  template<typename Node, typename Storage>
  void set_node_data(Node &node, Storage &storage, const CreateInfo &create_info)
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
    node.storage_index = storage.begin_rendering.append_and_get_index(create_info.node_data);
    /* NOTE: pointers in vk_rendering_info will be set to the correct location just before sending
     * to the command buffer. In the meantime these pointers are invalid.
     * VKRenderingAttachmentInfo's should be used instead. */
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
                      Data &data,
                      VKBoundPipelines & /*r_bound_pipelines*/) override
  {
    /* Localize pointers just before sending to the command buffer. Pointer can (and will) change
     * as they are stored in a union which is stored in a vector. When the vector reallocates,
     * the pointers will become invalid. */
    if (data.vk_rendering_info.pColorAttachments) {
      data.vk_rendering_info.pColorAttachments = data.color_attachments;
    }
    if (data.vk_rendering_info.pDepthAttachment) {
      data.vk_rendering_info.pDepthAttachment = &data.depth_attachment;
    }
    if (data.vk_rendering_info.pStencilAttachment) {
      data.vk_rendering_info.pStencilAttachment = &data.stencil_attachment;
    }
    command_buffer.begin_rendering(&data.vk_rendering_info);
  }

  /**
   * Reconfigure the vk_rendering_info to be restarted.
   *
   * When a render scope is restarted the clear/load ops needs to load in the previous stored
   * results.
   */
  static void reconfigure_for_restart(VKBeginRenderingData &begin_rendering_data)
  {
    auto reconfigure_attachment = [](VkRenderingAttachmentInfo &rendering_attachment) {
      if (ELEM(rendering_attachment.loadOp,
               VK_ATTACHMENT_LOAD_OP_CLEAR,
               VK_ATTACHMENT_LOAD_OP_DONT_CARE))
      {
        rendering_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      }
    };

    if (begin_rendering_data.vk_rendering_info.pStencilAttachment != nullptr) {
      reconfigure_attachment(begin_rendering_data.stencil_attachment);
    }
    if (begin_rendering_data.vk_rendering_info.pDepthAttachment != nullptr) {
      reconfigure_attachment(begin_rendering_data.depth_attachment);
    }
    for (VkRenderingAttachmentInfo &color_attachment : MutableSpan<VkRenderingAttachmentInfo>(
             begin_rendering_data.color_attachments,
             begin_rendering_data.vk_rendering_info.colorAttachmentCount))
    {
      reconfigure_attachment(color_attachment);
    }
  }
};

}  // namespace blender::gpu::render_graph
