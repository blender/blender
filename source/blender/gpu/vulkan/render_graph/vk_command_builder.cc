/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_command_builder.hh"
#include "vk_render_graph.hh"

namespace blender::gpu::render_graph {

VKCommandBuilder::VKCommandBuilder()
{
  vk_buffer_memory_barrier_ = {};
  vk_buffer_memory_barrier_.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  vk_buffer_memory_barrier_.pNext = nullptr;
  vk_buffer_memory_barrier_.srcAccessMask = VK_ACCESS_NONE;
  vk_buffer_memory_barrier_.dstAccessMask = VK_ACCESS_NONE;
  vk_buffer_memory_barrier_.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  vk_buffer_memory_barrier_.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  vk_buffer_memory_barrier_.buffer = VK_NULL_HANDLE;
  vk_buffer_memory_barrier_.offset = 0;
  vk_buffer_memory_barrier_.size = VK_WHOLE_SIZE;

  vk_image_memory_barrier_ = {};
  vk_image_memory_barrier_.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  vk_image_memory_barrier_.pNext = nullptr;
  vk_image_memory_barrier_.srcAccessMask = VK_ACCESS_NONE;
  vk_image_memory_barrier_.dstAccessMask = VK_ACCESS_NONE;
  vk_image_memory_barrier_.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  vk_image_memory_barrier_.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  vk_image_memory_barrier_.image = VK_NULL_HANDLE;
  vk_image_memory_barrier_.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  vk_image_memory_barrier_.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  vk_image_memory_barrier_.subresourceRange.aspectMask = VK_IMAGE_ASPECT_NONE;
  vk_image_memory_barrier_.subresourceRange.baseArrayLayer = 0;
  vk_image_memory_barrier_.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
  vk_image_memory_barrier_.subresourceRange.baseMipLevel = 0;
  vk_image_memory_barrier_.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
}

/* -------------------------------------------------------------------- */
/** \name Build nodes
 * \{ */

void VKCommandBuilder::build_nodes(VKRenderGraph &render_graph,
                                   VKCommandBufferInterface &command_buffer,
                                   Span<NodeHandle> nodes)
{
  /* Swap chain images layouts needs to be reset as the image layouts are changed externally.  */
  render_graph.resources_.reset_image_layouts();

  state_.active_pipelines = {};

  command_buffer.begin_recording();
  for (NodeHandle node_handle : nodes) {
    VKRenderGraphNode &node = render_graph.nodes_[node_handle];
    build_node(render_graph, command_buffer, node_handle, node);
  }
  command_buffer.end_recording();
}

void VKCommandBuilder::build_node(VKRenderGraph &render_graph,
                                  VKCommandBufferInterface &command_buffer,
                                  NodeHandle node_handle,
                                  const VKRenderGraphNode &node)
{
  build_pipeline_barriers(render_graph, command_buffer, node_handle, node.pipeline_stage_get());
  node.build_commands(command_buffer, state_.active_pipelines);
}

void VKCommandBuilder::build_pipeline_barriers(VKRenderGraph &render_graph,
                                               VKCommandBufferInterface &command_buffer,
                                               NodeHandle node_handle,
                                               VkPipelineStageFlags pipeline_stage)
{
  reset_barriers();
  add_image_barriers(render_graph, node_handle, pipeline_stage);
  add_buffer_barriers(render_graph, node_handle, pipeline_stage);
  send_pipeline_barriers(command_buffer);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pipeline barriers
 * \{ */

void VKCommandBuilder::reset_barriers()
{
  vk_buffer_memory_barriers_.clear();
  vk_image_memory_barriers_.clear();
  state_.src_stage_mask = VK_PIPELINE_STAGE_NONE;
  state_.dst_stage_mask = VK_PIPELINE_STAGE_NONE;
}

void VKCommandBuilder::send_pipeline_barriers(VKCommandBufferInterface &command_buffer)
{
  if (vk_image_memory_barriers_.is_empty() && vk_buffer_memory_barriers_.is_empty()) {
    reset_barriers();
    return;
  }

  /* When no resources have been used, we can start the barrier at the top of the pipeline.
   * It is not allowed to set it to None. */
  /* TODO: VK_KHR_synchronization2 allows setting src_stage_mask to NONE. */
  if (state_.src_stage_mask == VK_PIPELINE_STAGE_NONE) {
    state_.src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  }

  command_buffer.pipeline_barrier(state_.src_stage_mask,
                                  state_.dst_stage_mask,
                                  VK_DEPENDENCY_BY_REGION_BIT,
                                  0,
                                  nullptr,
                                  vk_buffer_memory_barriers_.size(),
                                  vk_buffer_memory_barriers_.data(),
                                  vk_image_memory_barriers_.size(),
                                  vk_image_memory_barriers_.data());
  reset_barriers();
}

void VKCommandBuilder::add_buffer_barriers(VKRenderGraph &render_graph,
                                           NodeHandle node_handle,
                                           VkPipelineStageFlags node_stages)
{
  add_buffer_read_barriers(render_graph, node_handle, node_stages);
  add_buffer_write_barriers(render_graph, node_handle, node_stages);
}

void VKCommandBuilder::add_buffer_read_barriers(VKRenderGraph &render_graph,
                                                NodeHandle node_handle,
                                                VkPipelineStageFlags node_stages)
{
  for (const VKRenderGraphLink &link : render_graph.links_[node_handle].inputs) {
    const ResourceWithStamp &versioned_resource = link.resource;
    VKResourceStateTracker::Resource &resource = render_graph.resources_.resources_.lookup(
        versioned_resource.handle);
    if (resource.type == VKResourceType::IMAGE) {
      /* Ignore image resources. */
      continue;
    }
    VKResourceBarrierState &resource_state = resource.barrier_state;
    VkAccessFlags read_access = resource_state.read_access;
    VkAccessFlags write_access = resource_state.write_access;
    VkAccessFlags wait_access = VK_ACCESS_NONE;

    if (read_access == (read_access | link.vk_access_flags)) {
      /* Has already been covered in a previous call no need to add this one. */
      continue;
    }

    read_access |= link.vk_access_flags;
    wait_access |= write_access;
    state_.src_stage_mask |= resource_state.write_stages;
    state_.dst_stage_mask |= node_stages;

    resource_state.read_access = read_access;
    resource_state.write_access = VK_ACCESS_NONE;
    resource_state.read_stages |= node_stages;
    resource_state.write_stages = VK_PIPELINE_STAGE_NONE;

    add_buffer_barrier(resource.buffer.vk_buffer, wait_access, read_access);
  }
}

void VKCommandBuilder::add_buffer_write_barriers(VKRenderGraph &render_graph,
                                                 NodeHandle node_handle,
                                                 VkPipelineStageFlags node_stages)
{
  for (const VKRenderGraphLink link : render_graph.links_[node_handle].outputs) {
    const ResourceWithStamp &versioned_resource = link.resource;
    VKResourceStateTracker::Resource &resource = render_graph.resources_.resources_.lookup(
        versioned_resource.handle);
    if (resource.type == VKResourceType::IMAGE) {
      /* Ignore image resources. */
      continue;
    }
    VKResourceBarrierState &resource_state = resource.barrier_state;
    VkAccessFlags read_access = resource_state.read_access;
    VkAccessFlags write_access = resource_state.write_access;
    VkAccessFlags wait_access = VK_ACCESS_NONE;

    if (read_access != VK_ACCESS_NONE) {
      wait_access |= read_access;
    }
    if (read_access == VK_ACCESS_NONE && write_access != VK_ACCESS_NONE) {
      wait_access |= write_access;
    }

    state_.src_stage_mask |= resource_state.read_stages | resource_state.write_stages;
    state_.dst_stage_mask |= node_stages;

    resource_state.read_access = VK_ACCESS_NONE;
    resource_state.write_access = link.vk_access_flags;
    resource_state.read_stages = VK_PIPELINE_STAGE_NONE;
    resource_state.write_stages = node_stages;

    if (wait_access != VK_ACCESS_NONE) {
      add_buffer_barrier(resource.buffer.vk_buffer, wait_access, link.vk_access_flags);
    }
  }
}

void VKCommandBuilder::add_buffer_barrier(VkBuffer vk_buffer,
                                          VkAccessFlags src_access_mask,
                                          VkAccessFlags dst_access_mask)
{
  vk_buffer_memory_barrier_.srcAccessMask = src_access_mask;
  vk_buffer_memory_barrier_.dstAccessMask = dst_access_mask;
  vk_buffer_memory_barrier_.buffer = vk_buffer;
  vk_buffer_memory_barriers_.append(vk_buffer_memory_barrier_);
  vk_buffer_memory_barrier_.srcAccessMask = VK_ACCESS_NONE;
  vk_buffer_memory_barrier_.dstAccessMask = VK_ACCESS_NONE;
  vk_buffer_memory_barrier_.buffer = VK_NULL_HANDLE;
}

void VKCommandBuilder::add_image_barriers(VKRenderGraph &render_graph,
                                          NodeHandle node_handle,
                                          VkPipelineStageFlags node_stages)
{
  add_image_read_barriers(render_graph, node_handle, node_stages);
  add_image_write_barriers(render_graph, node_handle, node_stages);
}

void VKCommandBuilder::add_image_read_barriers(VKRenderGraph &render_graph,
                                               NodeHandle node_handle,
                                               VkPipelineStageFlags node_stages)
{
  for (const VKRenderGraphLink &link : render_graph.links_[node_handle].inputs) {
    const ResourceWithStamp &versioned_resource = link.resource;
    VKResourceStateTracker::Resource &resource = render_graph.resources_.resources_.lookup(
        versioned_resource.handle);
    if (resource.type == VKResourceType::BUFFER) {
      /* Ignore buffer resources. */
      continue;
    }
    VKResourceBarrierState &resource_state = resource.barrier_state;
    VkAccessFlags read_access = resource_state.read_access;
    VkAccessFlags write_access = resource_state.write_access;
    VkAccessFlags wait_access = VK_ACCESS_NONE;

    if (read_access == (read_access | link.vk_access_flags) &&
        resource_state.image_layout == link.vk_image_layout)
    {
      /* Has already been covered in a previous call no need to add this one. */
      continue;
    }

    read_access |= link.vk_access_flags;
    wait_access |= write_access;
    state_.src_stage_mask |= resource_state.write_stages;
    state_.dst_stage_mask |= node_stages;

    resource_state.read_access = read_access;
    resource_state.write_access = VK_ACCESS_NONE;
    resource_state.read_stages |= node_stages;
    resource_state.write_stages = VK_PIPELINE_STAGE_NONE;

    add_image_barrier(resource.image.vk_image,
                      wait_access,
                      read_access,
                      resource_state.image_layout,
                      link.vk_image_layout);
    resource_state.image_layout = link.vk_image_layout;
  }
}

void VKCommandBuilder::add_image_write_barriers(VKRenderGraph &render_graph,
                                                NodeHandle node_handle,
                                                VkPipelineStageFlags node_stages)
{
  for (const VKRenderGraphLink link : render_graph.links_[node_handle].outputs) {
    const ResourceWithStamp &versioned_resource = link.resource;
    VKResourceStateTracker::Resource &resource = render_graph.resources_.resources_.lookup(
        versioned_resource.handle);
    if (resource.type == VKResourceType::BUFFER) {
      /* Ignore buffer resources. */
      continue;
    }
    VKResourceBarrierState &resource_state = resource.barrier_state;
    VkAccessFlags read_access = resource_state.read_access;
    VkAccessFlags write_access = resource_state.write_access;
    VkAccessFlags wait_access = VK_ACCESS_NONE;

    if (read_access != VK_ACCESS_NONE) {
      wait_access |= read_access;
    }
    if (read_access == VK_ACCESS_NONE && write_access != VK_ACCESS_NONE) {
      wait_access |= write_access;
    }

    state_.src_stage_mask |= resource_state.read_stages | resource_state.write_stages;
    state_.dst_stage_mask |= node_stages;

    resource_state.read_access = VK_ACCESS_NONE;
    resource_state.write_access = link.vk_access_flags;
    resource_state.read_stages = VK_PIPELINE_STAGE_NONE;
    resource_state.write_stages = node_stages;

    if (wait_access != VK_ACCESS_NONE || link.vk_image_layout != resource_state.image_layout) {
      add_image_barrier(resource.image.vk_image,
                        wait_access,
                        link.vk_access_flags,
                        resource_state.image_layout,
                        link.vk_image_layout);
      resource_state.image_layout = link.vk_image_layout;
    }
  }
}

void VKCommandBuilder::add_image_barrier(VkImage vk_image,
                                         VkAccessFlags src_access_mask,
                                         VkAccessFlags dst_access_mask,
                                         VkImageLayout old_layout,
                                         VkImageLayout new_layout)
{
  vk_image_memory_barrier_.srcAccessMask = src_access_mask;
  vk_image_memory_barrier_.dstAccessMask = dst_access_mask;
  vk_image_memory_barrier_.image = vk_image;
  vk_image_memory_barrier_.oldLayout = old_layout;
  vk_image_memory_barrier_.newLayout = new_layout;
  /* TODO: determine the correct aspect bits. */
  vk_image_memory_barrier_.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  vk_image_memory_barriers_.append(vk_image_memory_barrier_);
  vk_image_memory_barrier_.srcAccessMask = VK_ACCESS_NONE;
  vk_image_memory_barrier_.dstAccessMask = VK_ACCESS_NONE;
  vk_image_memory_barrier_.image = VK_NULL_HANDLE;
  vk_image_memory_barrier_.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  vk_image_memory_barrier_.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  vk_image_memory_barrier_.subresourceRange.aspectMask = VK_IMAGE_ASPECT_NONE;
}

/** \} */

}  // namespace blender::gpu::render_graph
