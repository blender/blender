/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include <sstream>

#include "vk_render_graph.hh"
#include "vk_scheduler.hh"

#include "BLI_index_range.hh"
#include "BLI_set.hh"

namespace blender::gpu::render_graph {

Span<NodeHandle> VKScheduler::select_nodes_for_image(const VKRenderGraph &render_graph,
                                                     VkImage vk_image)
{
  UNUSED_VARS(vk_image);
  select_all_nodes(render_graph);
  reorder_nodes(render_graph);
  return result_;
}

Span<NodeHandle> VKScheduler::select_nodes_for_buffer(const VKRenderGraph &render_graph,
                                                      VkBuffer vk_buffer)
{
  UNUSED_VARS(vk_buffer);
  select_all_nodes(render_graph);
  reorder_nodes(render_graph);
  return result_;
}

Span<NodeHandle> VKScheduler::select_nodes(const VKRenderGraph &render_graph)
{
  select_all_nodes(render_graph);
  reorder_nodes(render_graph);
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

/* -------------------------------------------------------------------- */
/** \name Reorder - move data transfer and dispatches outside rendering scope
 * \{ */

void VKScheduler::reorder_nodes(const VKRenderGraph &render_graph)
{
  move_transfer_and_dispatch_outside_rendering_scope(render_graph);
}

std::optional<std::pair<int64_t, int64_t>> VKScheduler::find_rendering_scope(
    const VKRenderGraph &render_graph, IndexRange search_range) const
{
  int64_t rendering_start = -1;

  for (int64_t index : search_range) {
    NodeHandle node_handle = result_[index];
    const VKRenderGraphNode &node = render_graph.nodes_[node_handle];
    if (node.type == VKNodeType::BEGIN_RENDERING) {
      rendering_start = index;
    }
    if (node.type == VKNodeType::END_RENDERING && rendering_start != -1) {
      return std::pair(rendering_start, index);
    }
  }
  BLI_assert(rendering_start == -1);

  return std::nullopt;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reorder - move data transfer and dispatches outside rendering scope
 * \{ */

void VKScheduler::move_transfer_and_dispatch_outside_rendering_scope(
    const VKRenderGraph &render_graph)
{
  foreach_rendering_scope(render_graph, [&](int64_t start_index, int64_t end_index) {
    /* Move end_rendering right after the last graphics node. */
    for (int index = end_index - 1; index >= start_index; index--) {
      NodeHandle node_handle = result_[index];
      const VKRenderGraphNode &node = render_graph.nodes_[node_handle];
      if (node_type_is_rendering(node.type)) {
        break;
      }
      std::swap(result_[end_index], result_[index]);
      end_index -= 1;
    }

    /* Move begin_rendering right before the first graphics node. */
    for (int index = start_index + 1; index < end_index; index++) {
      NodeHandle node_handle = result_[index];
      const VKRenderGraphNode &node = render_graph.nodes_[node_handle];
      if (node_type_is_rendering(node.type)) {
        break;
      }
      std::swap(result_[start_index], result_[index]);
      start_index += 1;
    }
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug
 * \{ */

void VKScheduler::debug_print(const VKRenderGraph &render_graph) const
{
  std::stringstream ss;
  int indent = 0;

  for (int index : result_.index_range()) {
    const NodeHandle node_handle = result_[index];
    const VKRenderGraphNode &node = render_graph.nodes_[node_handle];
    if (node.type == VKNodeType::END_RENDERING) {
      indent--;
    }
    for (int i = 0; i < indent; i++) {
      ss << "  ";
    }
    ss << node.type << "\n";
#if 0
    render_graph.debug_print(node_handle);
#endif
    if (node.type == VKNodeType::BEGIN_RENDERING) {
      indent++;
    }
  }
  ss << "\n";

  std::cout << ss.str();
}

/** \} */

}  // namespace blender::gpu::render_graph
