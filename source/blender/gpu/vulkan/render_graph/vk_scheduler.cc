/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_scheduler.hh"
#include "vk_render_graph.hh"

#include "BLI_index_range.hh"

namespace blender::gpu::render_graph {

Span<NodeHandle> VKScheduler::select_nodes_for_image(const VKRenderGraph &render_graph,
                                                     VkImage vk_image)
{
  UNUSED_VARS(vk_image);
  select_all_nodes(render_graph);
  return result_;
}

Span<NodeHandle> VKScheduler::select_nodes_for_buffer(const VKRenderGraph &render_graph,
                                                      VkBuffer vk_buffer)
{
  UNUSED_VARS(vk_buffer);
  select_all_nodes(render_graph);
  return result_;
}

void VKScheduler::select_all_nodes(const VKRenderGraph &render_graph)
{
  /* TODO: This will not work when we extract subgraphs. When subgraphs are removed the order in
   * the render graph may not follow the order the nodes were added. */
  result_.clear();
  for (NodeHandle node_handle : render_graph.nodes_.index_range()) {
    result_.append(node_handle);
  }
}

}  // namespace blender::gpu::render_graph
