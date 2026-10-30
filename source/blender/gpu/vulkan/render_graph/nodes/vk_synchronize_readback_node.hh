/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "vk_node_info.hh"

namespace blender::gpu::render_graph {

struct VKSynchronizeReadbackData {};

struct VKSynchronizeReadbackCreateInfo {
  VKResourceWithHandle<VkBuffer> buffer;
};

/** Make device writes to a buffer visible to the host before waiting for its submission. */
class VKSynchronizeReadbackNode : public VKNodeInfo<VKNodeType::SYNCHRONIZE_READBACK,
                                                    VKSynchronizeReadbackCreateInfo,
                                                    VKSynchronizeReadbackData,
                                                    VK_PIPELINE_STAGE_HOST_BIT,
                                                    VKResourceType::BUFFER> {
 public:
  template<typename Node, typename Storage>
  static void set_node_data(Node &node, Storage & /*storage*/, const CreateInfo & /*create_info*/)
  {
    node.synchronize_readback = {};
  }

  void build_links(VKResourceStateTracker &resources,
                   VKRenderGraphLinks &links,
                   const CreateInfo &create_info) override
  {
    /* Advance the stamp to keep later initial transfers after this synchronization point. */
    ResourceWithStamp resource = resources.get_buffer_and_increase_stamp(create_info.buffer);
    links.buffers.append({resource, VK_ACCESS_HOST_READ_BIT});
  }

  void build_commands(VKCommandBufferInterface & /*command_buffer*/,
                      Data & /*data*/,
                      Span<uint8_t> /*storage_push_constants*/,
                      VKBoundPipelines & /*r_bound_pipelines*/) override
  {
    /* The command builder generates the barrier from the buffer link and the host stage.
     * The caller must still wait for submission completion before accessing mapped memory. */
  }
};

}  // namespace blender::gpu::render_graph
