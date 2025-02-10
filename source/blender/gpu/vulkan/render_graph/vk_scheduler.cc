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

Span<NodeHandle> VKScheduler::select_nodes(const VKRenderGraph &render_graph)
{
  result_.clear();
  for (NodeHandle node_handle : render_graph.nodes_.index_range()) {
    result_.append(node_handle);
  }
  reorder_nodes(render_graph);
  return result_;
}

/* -------------------------------------------------------------------- */
/** \name Reorder - move data transfer and dispatches outside rendering scope
 * \{ */

void VKScheduler::reorder_nodes(const VKRenderGraph &render_graph)
{
  move_initial_transfer_to_start(render_graph);
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
/** \name Reorder - Move initial data transfers to the start
 * \{ */

void VKScheduler::move_initial_transfer_to_start(const VKRenderGraph &render_graph)
{
  Vector<NodeHandle> data_transfers;
  Vector<NodeHandle> other_nodes;

  data_transfers.reserve(result_.size());
  other_nodes.reserve(result_.size());

  for (const int64_t index : result_.index_range()) {
    NodeHandle node_handle = result_[index];
    const VKRenderGraphNode &node = render_graph.nodes_[node_handle];
    if (ELEM(node.type,
             VKNodeType::COPY_BUFFER,
             VKNodeType::COPY_BUFFER_TO_IMAGE,
             VKNodeType::COPY_IMAGE_TO_BUFFER))
    {
      const VKRenderGraphNodeLinks &links = render_graph.links_[node_handle];
      if (links.inputs[0].resource.stamp == 0 && links.outputs[0].resource.stamp == 0) {
        data_transfers.append(index);
        continue;
      }
    }
    if (ELEM(node.type, VKNodeType::FILL_BUFFER, VKNodeType::UPDATE_BUFFER)) {
      const VKRenderGraphNodeLinks &links = render_graph.links_[node_handle];
      if (links.outputs[0].resource.stamp == 0) {
        data_transfers.append(index);
        continue;
      }
    }

    other_nodes.append(index);
  }

  MutableSpan<NodeHandle> store_data_transfers = result_.as_mutable_span().slice(
      0, data_transfers.size());
  MutableSpan<NodeHandle> store_other = result_.as_mutable_span().slice(data_transfers.size(),
                                                                        other_nodes.size());
  store_data_transfers.copy_from(data_transfers);
  store_other.copy_from(other_nodes);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reorder - move data transfer and dispatches outside rendering scope
 * \{ */

void VKScheduler::move_transfer_and_dispatch_outside_rendering_scope(
    const VKRenderGraph &render_graph)
{
  Vector<NodeHandle> pre_rendering_scope;
  Vector<NodeHandle> rendering_scope;
  Set<ResourceHandle> used_buffers;

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

    /* Move buffer update buffer commands to before the rendering scope, unless the buffer is
     * already being used by a draw command. Images modification could also be moved outside the
     * rendering scope, but it is more tricky as they could also be attached to the frame-buffer.
     */
    pre_rendering_scope.clear();
    rendering_scope.clear();
    used_buffers.clear();

    for (int index = start_index + 1; index < end_index; index++) {
      NodeHandle node_handle = result_[index];
      const VKRenderGraphNode &node = render_graph.nodes_[node_handle];
      /* Should we add this node to the rendering scope. This is only done when we need to reorder
       * nodes. In that case the rendering_scope has already an item and we should add this node to
       * or the rendering scope or before the rendering scope. Adding nodes before rendering scope
       * is done in the VKNodeType::UPDATE_BUFFER branch. */
      bool add_to_rendering_scope = !rendering_scope.is_empty();
      if (node.type == VKNodeType::UPDATE_BUFFER) {
        /* Checking the node links to reduce potential locking the resource mutex. */
        if (!used_buffers.contains(render_graph.links_[node_handle].outputs[0].resource.handle)) {
          /* Buffer isn't used by this rendering scope so we can safely move it before the
           * rendering scope begins. */
          pre_rendering_scope.append(node_handle);
          add_to_rendering_scope = false;
          /* When this is the first time we move a node before the rendering we should start
           * building up the rendering scope as well. This is postponed so we can safe some cycles
           * when no nodes needs to be moved at all. */
          if (rendering_scope.is_empty()) {
            rendering_scope.extend(Span<NodeHandle>(&result_[start_index], index - start_index));
          }
        }
      }
      if (add_to_rendering_scope) {
        /* When rendering scope has an item we are rewriting the execution order and need to track
         * what should be inside the rendering scope. */
        rendering_scope.append(node_handle);
      }

      /* Any read/write to buffer resources should be added to used_buffers in order to detect if
       * it is safe to move a node before the rendering scope. */
      const VKRenderGraphNodeLinks &links = render_graph.links_[node_handle];
      for (const VKRenderGraphLink &input : links.inputs) {
        if (input.is_link_to_buffer()) {
          used_buffers.add(input.resource.handle);
        }
      }
      for (const VKRenderGraphLink &output : links.outputs) {
        if (output.is_link_to_buffer()) {
          used_buffers.add(output.resource.handle);
        }
      }
    }

    /* When pre_rendering_scope has an item we want to rewrite the order.
     * The number of nodes are not changed, so we can do this inline. */
    if (!pre_rendering_scope.is_empty()) {
      MutableSpan<NodeHandle> store_none_rendering = result_.as_mutable_span().slice(
          start_index, pre_rendering_scope.size());
      MutableSpan<NodeHandle> store_rendering = result_.as_mutable_span().slice(
          start_index + pre_rendering_scope.size(), rendering_scope.size());
      store_none_rendering.copy_from(pre_rendering_scope);
      store_rendering.copy_from(rendering_scope);
      start_index += pre_rendering_scope.size();
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
