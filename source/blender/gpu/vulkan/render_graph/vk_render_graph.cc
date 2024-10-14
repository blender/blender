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
  submission_id.reset();
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
  submission_id.next();
  remove_nodes(node_handles);
  command_buffer_->wait_for_cpu_synchronization();
}

void VKRenderGraph::submit_buffer_for_read(VkBuffer vk_buffer)
{
  std::scoped_lock lock(resources_.mutex);
  Span<NodeHandle> node_handles = scheduler_.select_nodes_for_buffer(*this, vk_buffer);
  command_builder_.build_nodes(*this, *command_buffer_, node_handles);
  command_buffer_->submit_with_cpu_synchronization();
  submission_id.next();
  remove_nodes(node_handles);
  command_buffer_->wait_for_cpu_synchronization();
}

void VKRenderGraph::submit()
{
  /* Using `VK_NULL_HANDLE` will select the default VkFence of the command buffer. */
  submit_synchronization_event(VK_NULL_HANDLE);
  wait_synchronization_event(VK_NULL_HANDLE);
}

void VKRenderGraph::submit_synchronization_event(VkFence vk_fence)
{
  std::scoped_lock lock(resources_.mutex);
  Span<NodeHandle> node_handles = scheduler_.select_nodes(*this);
  command_builder_.build_nodes(*this, *command_buffer_, node_handles);
  command_buffer_->submit_with_cpu_synchronization(vk_fence);
  submission_id.next();
  remove_nodes(node_handles);
}

void VKRenderGraph::wait_synchronization_event(VkFence vk_fence)
{
  command_buffer_->wait_for_cpu_synchronization(vk_fence);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug
 * \{ */

void VKRenderGraph::debug_group_begin(const char *name)
{
  DebugGroupNameID name_id = debug_.group_names.index_of_or_add(std::string(name));
  debug_.group_stack.append(name_id);
  debug_.group_used = false;
}

void VKRenderGraph::debug_group_end()
{
  debug_.group_stack.pop_last();
  debug_.group_used = false;
}

void VKRenderGraph::debug_print(NodeHandle node_handle) const
{
  std::ostream &os = std::cout;
  os << "NODE:\n";
  const VKRenderGraphNode &node = nodes_[node_handle];
  os << "  type:" << node.type << "\n";

  const VKRenderGraphNodeLinks &links = links_[node_handle];
  os << " inputs:\n";
  for (const VKRenderGraphLink &link : links.inputs) {
    os << "  ";
    link.debug_print(os, resources_);
    os << "\n";
  }
  os << " outputs:\n";
  for (const VKRenderGraphLink &link : links.outputs) {
    os << "  ";
    link.debug_print(os, resources_);
    os << "\n";
  }
}

/** \} */

}  // namespace blender::gpu::render_graph
