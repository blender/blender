/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * The scheduler is responsible to find and reorder the nodes in the render graph to update an
 * image or buffer to its latest content and state.
 */

#pragma once

#include "BLI_vector.hh"

#include "vk_common.hh"
#include "vk_render_graph_node.hh"

namespace blender::gpu::render_graph {
class VKRenderGraph;

/**
 * VKScheduler is responsible for selecting and reordering of nodes in the render graph. This
 * selection and order is used to convert the nodes to commands and submitting it to the GPU.
 *
 * This scheduler selects all nodes in the order they were added to the render graph.
 *
 * This is an initial implementation and should be enhanced for:
 * - Moving data transfer and compute before drawing, when they are scheduled between drawing nodes
 *   that use the same pipeline.
 * - Only select the nodes that are only needed for the given vk_image/vk_buffer. When performing
 *   read-backs of buffers should be done with as least as possible nodes as they can block
 *   drawing. It is better to do handle most nodes just before presenting the image. This would
 *   lead to less CPU locks.
 * - Pruning branches that are not linked to anything. EEVEE can add debug commands that would
 *   eventually not been displayed on screen. These branches should be pruned. The challenge is
 *   that we need to know for certain that it isn't used in a not submitted part of the graph.
 *
 * TODO: Walking the render graph isn't implemented yet.
 * The idea is to have a `Map<ResourceWithStamp, Vector<NodeHandle>> consumers` and
 * `Map<ResourceWithStamp, NodeHandle> producers`.
 * These attributes can be stored in the render graph and created when building the
 * links, or can be created inside the VKScheduler as a variable. The exact detail which one would
 * be better is unclear as there aren't any users yet. At the moment the scheduler would need them
 * we need to figure out the best way to store and retrieve the consumers/producers.
 */
class VKScheduler {
 private:
  /**
   * Results of `select_nodes_for_image`, `select_nodes_for_buffer` are cached in this instance to
   * reduce memory operations.
   */
  Vector<NodeHandle> result_;

 public:
  /**
   * Determine which nodes of the render graph should be selected and in what order they should
   * be executed to update the given vk_image to its latest content and state.
   *
   * NOTE: Currently will select all nodes.
   * NOTE: Result becomes invalid by the next call to VKScheduler.
   */
  [[nodiscard]] Span<NodeHandle> select_nodes(const VKRenderGraph &render_graph);

 private:
  void reorder_nodes(const VKRenderGraph &render_graph);

  /**
   * When a data transfer command writes to a resource which is initial it can be grouped at the
   * beginning of the render graph.
   *
   * This reduces context switches when executing commands on the GPU.
   */
  void move_initial_transfer_to_start(const VKRenderGraph &render_graph);

  /**
   * Any data transfer or dispatch nodes should be scheduled before or after a rendering scope.
   *
   * - Data transfer and dispatch nodes at the beginning are scheduled before
   *   the rendering begin.
   * - Data transfer and dispatch nodes at the end are scheduled after the
   *   rendering end.
   * - Data transfer and dispatch nodes in between draw commands will be pushed
   *   to the beginning if they are not yet being used.
   * - When used the rendering will be suspended and the data transfer/dispatch nodes are
   *   scheduled between the suspended rendering and when the suspended rendering is
   *   continued.
   *
   * NOTE: Clearing attachments is considered a rendering command as specified by the vulkan
   * specification.
   */
  void move_transfer_and_dispatch_outside_rendering_scope(const VKRenderGraph &render_graph);

  /**
   * Find the first rendering scope inside the given search range of the result_.
   */
  std::optional<std::pair<int64_t, int64_t>> find_rendering_scope(
      const VKRenderGraph &render_graph, IndexRange search_range) const;
  template<typename FuncT>
  void foreach_rendering_scope(const VKRenderGraph &render_graph, const FuncT &func) const
  {
    for (std::optional<std::pair<int64_t, int64_t>> rendering_scope =
             find_rendering_scope(render_graph, IndexRange(result_.index_range()));
         rendering_scope.has_value();
         rendering_scope =
             find_rendering_scope(render_graph,
                                  IndexRange(rendering_scope.value().second + 1,
                                             result_.size() - rendering_scope.value().second - 1)))
    {
      func(rendering_scope.value().first, rendering_scope.value().second);
    }
  }

  void debug_print(const VKRenderGraph &render_graph) const;
};

}  // namespace blender::gpu::render_graph
