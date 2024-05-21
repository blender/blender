/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_render_graph.hh"

namespace blender::gpu::render_graph {

VKRenderGraph::VKRenderGraph(std::unique_ptr<VKCommandBufferInterface> command_buffer,
                             VKResourceStateTracker &resources)
    : command_buffer_(std::move(command_buffer)), resources_(resources)
{
}

void VKRenderGraph::free_data()
{
  command_buffer_.reset();
}

void VKRenderGraph::remove_nodes(Span<NodeHandle> node_handles)
{
  UNUSED_VARS_NDEBUG(node_handles);
  BLI_assert_msg(node_handles.size() == nodes_.size(),
                 "Currently only supporting removing all nodes. The VKScheduler doesn't walk the "
                 "nodes, and will use incorrect ordering when not all nodes are removed. This "
                 "needs to be fixed when implementing a better scheduler.");
  links_.clear();
  for (VKRenderGraphNode &node : nodes_) {
    node.free_data();
  }
  nodes_.clear();

  debug_.node_group_map.clear();
  debug_.used_groups.clear();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Submit graph
 * \{ */

void VKRenderGraph::submit_for_present(VkImage vk_swapchain_image)
{
  /* Needs to be executed at forehand as `add_node` also locks the mutex. */
  VKSynchronizationNode::CreateInfo synchronization = {};
  synchronization.vk_image = vk_swapchain_image;
  synchronization.vk_image_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  synchronization.vk_image_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  add_node<VKSynchronizationNode>(synchronization);

  std::scoped_lock lock(resources_.mutex);
  Span<NodeHandle> node_handles = scheduler_.select_nodes_for_image(*this, vk_swapchain_image);
  command_builder_.build_nodes(*this, *command_buffer_, node_handles);
  /* TODO: To improve performance it could be better to return a semaphore. This semaphore can be
   * passed in the swapchain to ensure GPU synchronization. This also require a second semaphore to
   * pause drawing until the swapchain has completed its drawing phase.
   *
   * Currently using CPU synchronization for safety. */
  command_buffer_->submit_with_cpu_synchronization();
  remove_nodes(node_handles);
  command_buffer_->wait_for_cpu_synchronization();
}

void VKRenderGraph::submit_buffer_for_read(VkBuffer vk_buffer)
{
  std::scoped_lock lock(resources_.mutex);
  Span<NodeHandle> node_handles = scheduler_.select_nodes_for_buffer(*this, vk_buffer);
  command_builder_.build_nodes(*this, *command_buffer_, node_handles);
  command_buffer_->submit_with_cpu_synchronization();
  remove_nodes(node_handles);
  command_buffer_->wait_for_cpu_synchronization();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug
 * \{ */

void VKRenderGraph::debug_group_begin(const char *name)
{
  debug_.group_stack.append(name);
  debug_.group_used = false;
}

void VKRenderGraph::debug_group_end()
{
  debug_.group_stack.pop_last();
  debug_.group_used = false;
}

/** \} */

}  // namespace blender::gpu::render_graph
