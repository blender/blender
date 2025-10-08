/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_command_builder.hh"
#include "vk_backend.hh"
#include "vk_render_graph.hh"
#include "vk_to_string.hh"

#include <sstream>

namespace blender::gpu::render_graph {

/* -------------------------------------------------------------------- */
/** \name Build nodes
 * \{ */

void VKCommandBuilder::build_nodes(VKRenderGraph &render_graph,
                                   VKCommandBufferInterface &command_buffer,
                                   Span<NodeHandle> node_handles)
{
  groups_init(render_graph, node_handles);
  groups_extract_barriers(
      render_graph, node_handles, command_buffer.use_dynamic_rendering_local_read);
}

void VKCommandBuilder::record_commands(VKRenderGraph &render_graph,
                                       VKCommandBufferInterface &command_buffer,
                                       Span<NodeHandle> node_handles)
{
  groups_build_commands(render_graph, command_buffer, node_handles);
}

void VKCommandBuilder::groups_init(const VKRenderGraph &render_graph,
                                   Span<NodeHandle> node_handles)
{
  group_nodes_.clear();
  IndexRange nodes_range = node_handles.index_range();
  while (!nodes_range.is_empty()) {
    IndexRange node_group = nodes_range.slice(0, 1);
    NodeHandle node_handle = node_handles[nodes_range.first()];
    const VKRenderGraphNode &node = render_graph.nodes_[node_handle];
    while (node_type_is_rendering(node.type) && node_group.size() < nodes_range.size()) {
      NodeHandle node_handle = node_handles[nodes_range[node_group.size()]];
      const VKRenderGraphNode &node = render_graph.nodes_[node_handle];
      if (!node_type_is_rendering(node.type) || node.type == VKNodeType::BEGIN_RENDERING) {
        break;
      }
      node_group = nodes_range.slice(0, node_group.size() + 1);
    }

    group_nodes_.append(node_group);
    nodes_range = nodes_range.drop_front(node_group.size());
  }
}

void VKCommandBuilder::groups_extract_barriers(VKRenderGraph &render_graph,
                                               Span<NodeHandle> node_handles,
                                               bool use_local_read)
{
  barrier_list_.clear();
  vk_buffer_memory_barriers_.clear();
  vk_image_memory_barriers_.clear();

  ImageTracker image_tracker(*this);

  /* Extract barriers. */
  group_pre_barriers_.clear();
  group_post_barriers_.clear();
  node_pre_barriers_.resize(node_handles.size());

  /* Keep track of the post barriers that needs to be added. The pre barriers will be stored
   * directly in `barrier_list_` but may not mingle with the pre barriers. Most barriers are
   * group pre barriers. */
  Vector<Barrier> post_barriers;
  /* Keep track of the node pre barriers that needs to be added. The pre barriers will be stored
   * directly in `barrier_list_` but may not mingle with the group barriers. */
  Vector<Barrier> node_pre_barriers;

  NodeHandle rendering_scope;
  bool rendering_active = false;

  for (const int64_t group_index : group_nodes_.index_range()) {
    /* Extract the pre-barriers of this group. */
    Barriers group_pre_barriers(barrier_list_.size(), 0);
    const GroupNodes &node_group = group_nodes_[group_index];
    for (const int64_t group_node_index : node_group) {
      NodeHandle node_handle = node_handles[group_node_index];
      VKRenderGraphNode &node = render_graph.nodes_[node_handle];
      Barrier barrier = {};
      build_pipeline_barriers(
          render_graph, node_handle, node.pipeline_stage_get(), image_tracker, barrier);
      if (!barrier.is_empty()) {
#if 0
        std::cout << __func__ << ": node_group=" << group_index
                  << ", node_group_range=" << node_group.first() << "-" << node_group.last()
                  << ", node_handle=" << node_handle << ", node_type=" << node.type
                  << ", debug_group=" << render_graph.full_debug_group(node_handle) << "\n";
        std::cout << __func__ << ": " << to_string_barrier(barrier);
#endif
        barrier_list_.append(barrier);
      }
      /* Check for additional barriers when resuming rendering.
       *
       * Between suspending rendering and resuming the state/layout of resources can change and
       * require additional barriers.
       */
      if (node.type == VKNodeType::BEGIN_RENDERING) {
        /* Begin rendering scope. */
        BLI_assert(!rendering_active);
        rendering_scope = node_handle;
        rendering_active = true;
        image_tracker.begin(render_graph, node_handle);
      }

      else if (node.type == VKNodeType::END_RENDERING) {
        /* End rendering scope. */
        BLI_assert(rendering_active);
        rendering_scope = 0;
        rendering_active = false;

        /* Any specific layout changes needs to be reverted, so the global resource state tracker
         * reflects the correct state. These barriers needs to be added as node post barriers. We
         * assume that END_RENDERING is always the last node of a group. */
        Barrier barrier = {};
        image_tracker.end(barrier, use_local_read);
        if (!barrier.is_empty()) {
          post_barriers.append(barrier);
        }
      }

      else if (rendering_active && !node_type_is_within_rendering(node.type)) {
        /* Suspend active rendering scope. */
        rendering_active = false;

        /* Any specific layout changes needs to be reverted, so the global resource state tracker
         * reflects the correct state. These barriers needs to be added as node post barriers.
         */
        Barrier barrier = {};
        image_tracker.suspend(barrier, use_local_read);
        if (!barrier.is_empty()) {
          post_barriers.append(barrier);
        }
      }

      else if (!rendering_active && node_type_is_within_rendering(node.type)) {
        /* Resume rendering scope. */
        VKRenderGraphNode &rendering_node = render_graph.nodes_[rendering_scope];
        Barrier barrier = {};
        build_pipeline_barriers(render_graph,
                                rendering_scope,
                                rendering_node.pipeline_stage_get(),
                                image_tracker,
                                barrier);
        if (!barrier.is_empty()) {
          barrier_list_.append(barrier);
        }

        /* Resume layered tracking. Each layer that has an override will be transition back to
         * the layer specific image layout. */
        barrier = {};
        image_tracker.resume(barrier, use_local_read);
        if (!barrier.is_empty()) {
          barrier_list_.append(barrier);
        }

        rendering_active = true;
      }

      /* Extract pre barriers for nodes. */
      if (use_local_read && node_type_is_within_rendering(node.type) &&
          node_has_input_attachments(render_graph, node_handle))
      {
        Barrier barrier = {};
        build_pipeline_barriers(
            render_graph, node_handle, node.pipeline_stage_get(), image_tracker, barrier, true);
        if (!barrier.is_empty()) {
          node_pre_barriers.append(barrier);
        }
      }
    }
    if (rendering_active) {
      /* Suspend layered image tracker. When active the next group will always be a compute/data
       * transfer group.
       *
       * Any specific layout changes needs to be reverted, so the global resource state tracker
       * reflects the correct state. These barriers needs to be added as node post barriers.
       */
      Barrier barrier = {};
      image_tracker.suspend(barrier, use_local_read);
      if (!barrier.is_empty()) {
        post_barriers.append(barrier);
      }
      rendering_active = false;
    }

    /* Update the group pre and post barriers. Pre barriers are already stored in the
     * barrier_list_. The post barriers are appended after the pre barriers. */
    int64_t barrier_list_size = barrier_list_.size();
    group_pre_barriers_.append(group_pre_barriers.with_new_end(barrier_list_size));
    barrier_list_.extend(std::move(post_barriers));
    group_post_barriers_.append(
        IndexRange::from_begin_end(barrier_list_size, barrier_list_.size()));
    if (!node_pre_barriers.is_empty()) {
      barrier_list_size = barrier_list_.size();
      barrier_list_.extend(std::move(node_pre_barriers));
      /* Shift all node pre barrier references to the new location in the barrier_list_. */
      for (const int64_t group_node_index : node_group) {
        NodeHandle node_handle = node_handles[group_node_index];
        if (!node_pre_barriers_[node_handle].is_empty()) {
          node_pre_barriers_[node_handle].from_begin_size(
              node_pre_barriers_[node_handle].start() + barrier_list_size, 1);
        }
      }
    }
  }

  BLI_assert(group_pre_barriers_.size() == group_nodes_.size());
  BLI_assert(group_post_barriers_.size() == group_nodes_.size());
}

void VKCommandBuilder::groups_build_commands(VKRenderGraph &render_graph,
                                             VKCommandBufferInterface &command_buffer,
                                             Span<NodeHandle> node_handles)
{
  DebugGroups debug_groups = {};
  VKBoundPipelines active_pipelines = {};

  NodeHandle rendering_scope = 0;
  bool rendering_active = false;

  for (int64_t group_index : group_nodes_.index_range()) {
    IndexRange group_nodes = group_nodes_[group_index];
    Span<NodeHandle> group_node_handles = node_handles.slice(group_nodes);

    /* Record group pre barriers. */
    for (BarrierIndex barrier_index : group_pre_barriers_[group_index]) {
      BLI_assert_msg(!rendering_active,
                     "Pre group barriers must be executed outside a rendering scope.");
      Barrier &barrier = barrier_list_[barrier_index];
#if 0
      std::cout << __func__ << ": node_group=" << group_index
                << ", node_group_range=" << group_node_handles.first() << "-"
                << group_node_handles.last() << ", pre_barrier=(" << to_string_barrier(barrier)
                << ")\n";
#endif
      send_pipeline_barriers(command_buffer, barrier, false);
    }

    /* Record group node commands. */
    for (NodeHandle node_handle : group_node_handles) {
      VKRenderGraphNode &node = render_graph.nodes_[node_handle];

      if (G.debug & G_DEBUG_GPU) {
        activate_debug_group(render_graph, command_buffer, debug_groups, node_handle);
      }

      if (node.type == VKNodeType::BEGIN_RENDERING) {
        rendering_scope = node_handle;
        rendering_active = true;
      }

      else if (node.type == VKNodeType::END_RENDERING) {
        rendering_active = false;
      }
      else if (node_type_is_within_rendering(node.type)) {
        if (!rendering_active) {
          /* Restart rendering scope. */
          VKRenderGraphNode &rendering_node = render_graph.nodes_[rendering_scope];
          VKBeginRenderingNode::reconfigure_for_restart(
              render_graph.storage_.begin_rendering[rendering_node.storage_index]);
          rendering_node.build_commands(command_buffer, render_graph.storage_, active_pipelines);
          rendering_active = true;
        }
      }

      /* Record group node barriers. (VK_EXT_dynamic_rendering_local_read) */
      for (BarrierIndex node_pre_barrier_index : node_pre_barriers_[node_handle]) {
        Barrier &barrier = barrier_list_[node_pre_barrier_index];
#if 0
      std::cout << __func__ << ": node_group=" << group_index
                << ", node_group_range=" << group_node_handles.first() << "-"
                << group_node_handles.last() << ", node_pre_barrier=(" << to_string_barrier(barrier)
                << ")\n";
#endif
        /* TODO: Barrier should already contain the changes for local read. */
        send_pipeline_barriers(command_buffer, barrier, true);
      }

#if 0
      std::cout << __func__ << ": node_group=" << group_index
                << ", node_group_range=" << group_node_handles.first() << "-"
                << group_node_handles.last() << ", node_handle=" << node_handle
                << ", node_type=" << node.type
                << ", debug group=" << render_graph.full_debug_group(node_handle) << "\n";
#endif
      node.build_commands(command_buffer, render_graph.storage_, active_pipelines);
    }

    if (rendering_active) {
      /* Suspend rendering as the next node group will contain data transfer/dispatch commands.
       */
      rendering_active = false;
      command_buffer.end_rendering();
    }

    /* Record group post barriers. */
    for (BarrierIndex barrier_index : group_post_barriers_[group_index]) {
      BLI_assert_msg(!rendering_active,
                     "Post group barriers must be executed outside a rendering scope.");
      Barrier &barrier = barrier_list_[barrier_index];
#if 0
      std::cout << __func__ << ": node_group=" << group_index
                << ", node_group_range=" << group_node_handles.first() << "-"
                << group_node_handles.last() << ", post_barrier=(" << to_string_barrier(barrier)
                << ")\n";
#endif
      send_pipeline_barriers(command_buffer, barrier, false);
    }
  }

  finish_debug_groups(command_buffer, debug_groups);
}

bool VKCommandBuilder::node_has_input_attachments(const VKRenderGraph &render_graph,
                                                  NodeHandle node)
{
  const VKRenderGraphNodeLinks &links = render_graph.links_[node];
  const Vector<VKRenderGraphLink> &inputs = links.inputs;
  return std::any_of(inputs.begin(), inputs.end(), [](const VKRenderGraphLink &input) {
    return input.vk_access_flags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
  });
}

void VKCommandBuilder::activate_debug_group(VKRenderGraph &render_graph,
                                            VKCommandBufferInterface &command_buffer,
                                            DebugGroups &debug_groups,
                                            NodeHandle node_handle)
{
  VKRenderGraph::DebugGroupID debug_group = render_graph.debug_.node_group_map[node_handle];
  if (debug_group == debug_groups.active_debug_group_id) {
    return;
  }

  /* Determine the number of pops and pushes that will happen on the debug stack. */
  int num_ends = 0;
  int num_begins = 0;

  if (debug_group == -1) {
    num_ends = debug_groups.debug_level;
  }
  else {
    Vector<VKRenderGraph::DebugGroupNameID> &to_group =
        render_graph.debug_.used_groups[debug_group];
    if (debug_groups.active_debug_group_id != -1) {
      Vector<VKRenderGraph::DebugGroupNameID> &from_group =
          render_graph.debug_.used_groups[debug_groups.active_debug_group_id];

      num_ends = max_ii(from_group.size() - to_group.size(), 0);
      int num_checks = min_ii(from_group.size(), to_group.size());
      for (int index : IndexRange(num_checks)) {
        if (from_group[index] != to_group[index]) {
          num_ends += num_checks - index;
          break;
        }
      }
    }

    num_begins = to_group.size() - (debug_groups.debug_level - num_ends);
  }

  /* Perform the pops from the debug stack. */
  for (int index = 0; index < num_ends; index++) {
    command_buffer.end_debug_utils_label();
  }
  debug_groups.debug_level -= num_ends;

  /* Perform the pushes to the debug stack. */
  if (num_begins > 0) {
    Vector<VKRenderGraph::DebugGroupNameID> &to_group =
        render_graph.debug_.used_groups[debug_group];
    VkDebugUtilsLabelEXT debug_utils_label = {};
    debug_utils_label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    for (int index : IndexRange(debug_groups.debug_level, num_begins)) {
      const VKRenderGraph::DebugGroup &debug_group = render_graph.debug_.groups[to_group[index]];
      debug_utils_label.pLabelName = debug_group.name.c_str();
      copy_v4_v4(debug_utils_label.color, debug_group.color);
      command_buffer.begin_debug_utils_label(&debug_utils_label);
    }
  }

  debug_groups.debug_level += num_begins;
  debug_groups.active_debug_group_id = debug_group;
}

void VKCommandBuilder::finish_debug_groups(VKCommandBufferInterface &command_buffer,
                                           DebugGroups &debug_groups)
{
  for (int i = 0; i < debug_groups.debug_level; i++) {
    command_buffer.end_debug_utils_label();
  }
  debug_groups.debug_level = 0;
}

void VKCommandBuilder::build_pipeline_barriers(VKRenderGraph &render_graph,
                                               NodeHandle node_handle,
                                               VkPipelineStageFlags pipeline_stage,
                                               ImageTracker &image_tracker,
                                               Barrier &r_barrier,
                                               bool within_rendering)
{
  reset_barriers(r_barrier);
  add_image_barriers(
      render_graph, node_handle, pipeline_stage, image_tracker, r_barrier, within_rendering);
  add_buffer_barriers(render_graph, node_handle, pipeline_stage, r_barrier);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pipeline barriers
 * \{ */

void VKCommandBuilder::reset_barriers(Barrier &r_barrier)
{
  r_barrier.dst_stage_mask = r_barrier.src_stage_mask = VK_PIPELINE_STAGE_NONE;
}

void VKCommandBuilder::send_pipeline_barriers(VKCommandBufferInterface &command_buffer,
                                              const Barrier &barrier,
                                              bool within_rendering)
{
  if (barrier.is_empty()) {
    return;
  }

  /* When no resources have been used, we can start the barrier at the top of the pipeline.
   * It is not allowed to set it to None. */
  /* TODO: VK_KHR_synchronization2 allows setting src_stage_mask to NONE. */
  /* When no resources have been used, we can start the barrier at the top of the pipeline.
   * It is not allowed to set it to None. */
  VkPipelineStageFlags src_stage_mask = (barrier.src_stage_mask == VK_PIPELINE_STAGE_NONE) ?
                                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT :
                                            VkPipelineStageFlagBits(barrier.src_stage_mask);

  VkPipelineStageFlags dst_stage_mask = barrier.dst_stage_mask;
  /* TODO: this should be done during barrier extraction making within_rendering obsolete. */
  if (within_rendering) {
    /* See: VUID - `vkCmdPipelineBarrier` - `srcStageMask` - 09556
     * If `vkCmdPipelineBarrier` is called within a render pass instance started with
     * `vkCmdBeginRendering`, this command must only specify frame-buffer-space stages in
     * `srcStageMask` and `dstStageMask`. */
    src_stage_mask = dst_stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                      VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  }

  Span<VkBufferMemoryBarrier> buffer_barriers = vk_buffer_memory_barriers_.as_span().slice(
      barrier.buffer_memory_barriers);
  Span<VkImageMemoryBarrier> image_barriers = vk_image_memory_barriers_.as_span().slice(
      barrier.image_memory_barriers);

  command_buffer.pipeline_barrier(src_stage_mask,
                                  dst_stage_mask,
                                  VK_DEPENDENCY_BY_REGION_BIT,
                                  0,
                                  nullptr,
                                  buffer_barriers.size(),
                                  buffer_barriers.data(),
                                  image_barriers.size(),
                                  image_barriers.data());
}

void VKCommandBuilder::add_buffer_barriers(VKRenderGraph &render_graph,
                                           NodeHandle node_handle,
                                           VkPipelineStageFlags node_stages,
                                           Barrier &r_barrier)
{
  r_barrier.buffer_memory_barriers = IndexRange(vk_buffer_memory_barriers_.size(), 0);
  add_buffer_read_barriers(render_graph, node_handle, node_stages, r_barrier);
  add_buffer_write_barriers(render_graph, node_handle, node_stages, r_barrier);
  r_barrier.buffer_memory_barriers = r_barrier.buffer_memory_barriers.with_new_end(
      vk_buffer_memory_barriers_.size());
}

void VKCommandBuilder::add_buffer_read_barriers(VKRenderGraph &render_graph,
                                                NodeHandle node_handle,
                                                VkPipelineStageFlags node_stages,
                                                Barrier &r_barrier)
{
  for (const VKRenderGraphLink &link : render_graph.links_[node_handle].inputs) {
    if (!link.is_link_to_buffer()) {
      continue;
    }
    const ResourceWithStamp &versioned_resource = link.resource;
    VKResourceStateTracker::Resource &resource = render_graph.resources_.resources_.lookup(
        versioned_resource.handle);
    VKResourceBarrierState &resource_state = resource.barrier_state;
    const bool is_first_read = resource_state.is_new_stamp();
    if (!is_first_read &&
        (resource_state.vk_access & link.vk_access_flags) == link.vk_access_flags &&
        (resource_state.vk_pipeline_stages & node_stages) == node_stages)
    {
      /* Has already been covered in a previous call no need to add this one. */
      continue;
    }

    const VkAccessFlags wait_access = resource_state.vk_access;

    r_barrier.src_stage_mask |= resource_state.vk_pipeline_stages;
    r_barrier.dst_stage_mask |= node_stages;

    if (is_first_read) {
      resource_state.vk_access = link.vk_access_flags;
      resource_state.vk_pipeline_stages = node_stages;
    }
    else {
      resource_state.vk_access |= link.vk_access_flags;
      resource_state.vk_pipeline_stages |= node_stages;
    }

    add_buffer_barrier(resource.buffer.vk_buffer, r_barrier, wait_access, link.vk_access_flags);
  }
}

void VKCommandBuilder::add_buffer_write_barriers(VKRenderGraph &render_graph,
                                                 NodeHandle node_handle,
                                                 VkPipelineStageFlags node_stages,
                                                 Barrier &r_barrier)
{
  for (const VKRenderGraphLink link : render_graph.links_[node_handle].outputs) {
    if (!link.is_link_to_buffer()) {
      continue;
    }
    const ResourceWithStamp &versioned_resource = link.resource;
    VKResourceStateTracker::Resource &resource = render_graph.resources_.resources_.lookup(
        versioned_resource.handle);
    VKResourceBarrierState &resource_state = resource.barrier_state;
    const VkAccessFlags wait_access = resource_state.vk_access;

    r_barrier.src_stage_mask |= resource_state.vk_pipeline_stages;
    r_barrier.dst_stage_mask |= node_stages;

    resource_state.vk_access = link.vk_access_flags;
    resource_state.vk_pipeline_stages = node_stages;

    if (wait_access != VK_ACCESS_NONE) {
      add_buffer_barrier(resource.buffer.vk_buffer, r_barrier, wait_access, link.vk_access_flags);
    }
  }
}

void VKCommandBuilder::add_buffer_barrier(VkBuffer vk_buffer,
                                          Barrier &r_barrier,
                                          VkAccessFlags src_access_mask,
                                          VkAccessFlags dst_access_mask)
{
  for (VkBufferMemoryBarrier &vk_buffer_memory_barrier :
       vk_buffer_memory_barriers_.as_mutable_span().drop_front(
           r_barrier.buffer_memory_barriers.start()))
  {
    if (vk_buffer_memory_barrier.buffer == vk_buffer) {
      /* When registering read/write buffers, it can be that the node internally requires
       * read/write. In this case we adjust the dstAccessMask of the read barrier. */
      if ((vk_buffer_memory_barrier.dstAccessMask & src_access_mask) == src_access_mask) {
        vk_buffer_memory_barrier.dstAccessMask |= dst_access_mask;
        return;
      }
      /* When re-registering resources we can skip if access mask already contain all the flags.
       */
      if ((vk_buffer_memory_barrier.dstAccessMask & dst_access_mask) == dst_access_mask &&
          (vk_buffer_memory_barrier.srcAccessMask & src_access_mask) == src_access_mask)
      {
        return;
      }
    }
  }

  vk_buffer_memory_barriers_.append({VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                                     nullptr,
                                     src_access_mask,
                                     dst_access_mask,
                                     VK_QUEUE_FAMILY_IGNORED,
                                     VK_QUEUE_FAMILY_IGNORED,
                                     vk_buffer,
                                     0,
                                     VK_WHOLE_SIZE});
}

void VKCommandBuilder::add_image_barriers(VKRenderGraph &render_graph,
                                          NodeHandle node_handle,
                                          VkPipelineStageFlags node_stages,
                                          ImageTracker &image_tracker,
                                          Barrier &r_barrier,
                                          bool within_rendering)
{
  r_barrier.image_memory_barriers = IndexRange(vk_image_memory_barriers_.size(), 0);
  add_image_read_barriers(
      render_graph, node_handle, node_stages, image_tracker, r_barrier, within_rendering);
  add_image_write_barriers(
      render_graph, node_handle, node_stages, image_tracker, r_barrier, within_rendering);
  r_barrier.image_memory_barriers = r_barrier.image_memory_barriers.with_new_end(
      vk_image_memory_barriers_.size());
}

void VKCommandBuilder::add_image_read_barriers(VKRenderGraph &render_graph,
                                               NodeHandle node_handle,
                                               VkPipelineStageFlags node_stages,
                                               ImageTracker &image_tracker,
                                               Barrier &r_barrier,
                                               bool within_rendering)
{
  for (const VKRenderGraphLink &link : render_graph.links_[node_handle].inputs) {
    if (link.is_link_to_buffer()) {
      continue;
    }
    const ResourceWithStamp &versioned_resource = link.resource;
    VKResourceStateTracker::Resource &resource = render_graph.resources_.resources_.lookup(
        versioned_resource.handle);
    VKResourceBarrierState &resource_state = resource.barrier_state;
    const bool is_first_read = resource_state.is_new_stamp();
    if ((!is_first_read) &&
        (resource_state.vk_access & link.vk_access_flags) == link.vk_access_flags &&
        (resource_state.vk_pipeline_stages & node_stages) == node_stages &&
        resource_state.image_layout == link.vk_image_layout)
    {
      /* Has already been covered in previous barrier no need to add this one. */
      continue;
    }
    if (within_rendering && link.vk_image_layout != VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR) {
      /* Allow only local read barriers inside rendering scope */
      continue;
    }

    if (resource_state.image_layout != link.vk_image_layout &&
        image_tracker.contains(resource.image.vk_image))
    {
      image_tracker.update(resource.image.vk_image,
                           link.subimage,
                           resource_state.image_layout,
                           link.vk_image_layout,
                           r_barrier);
      continue;
    }

    VkAccessFlags wait_access = resource_state.vk_access;

    r_barrier.src_stage_mask |= resource_state.vk_pipeline_stages;
    r_barrier.dst_stage_mask |= node_stages;

    if (is_first_read) {
      resource_state.vk_access = link.vk_access_flags;
      resource_state.vk_pipeline_stages = node_stages;
    }
    else {
      resource_state.vk_access |= link.vk_access_flags;
      resource_state.vk_pipeline_stages |= node_stages;
    }

    add_image_barrier(resource.image.vk_image,
                      r_barrier,
                      wait_access,
                      link.vk_access_flags,
                      resource_state.image_layout,
                      link.vk_image_layout,
                      link.vk_image_aspect,
                      {});
    resource_state.image_layout = link.vk_image_layout;
  }
}

void VKCommandBuilder::add_image_write_barriers(VKRenderGraph &render_graph,
                                                NodeHandle node_handle,
                                                VkPipelineStageFlags node_stages,
                                                ImageTracker &image_tracker,
                                                Barrier &r_barrier,
                                                bool within_rendering)
{
  for (const VKRenderGraphLink link : render_graph.links_[node_handle].outputs) {
    if (link.is_link_to_buffer()) {
      continue;
    }
    const ResourceWithStamp &versioned_resource = link.resource;
    VKResourceStateTracker::Resource &resource = render_graph.resources_.resources_.lookup(
        versioned_resource.handle);
    VKResourceBarrierState &resource_state = resource.barrier_state;
    const VkAccessFlags wait_access = resource_state.vk_access;
    if (within_rendering && link.vk_image_layout != VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR) {
      /* Allow only local read barriers inside rendering scope */
      continue;
    }
    if (image_tracker.contains(resource.image.vk_image) &&
        resource_state.image_layout != link.vk_image_layout)
    {
      image_tracker.update(resource.image.vk_image,
                           link.subimage,
                           resource_state.image_layout,
                           link.vk_image_layout,
                           r_barrier);

      continue;
    }

    r_barrier.src_stage_mask |= resource_state.vk_pipeline_stages;
    r_barrier.dst_stage_mask |= node_stages;

    resource_state.vk_access = link.vk_access_flags;
    resource_state.vk_pipeline_stages = node_stages;

    if (wait_access != VK_ACCESS_NONE || link.vk_image_layout != resource_state.image_layout) {
      add_image_barrier(resource.image.vk_image,
                        r_barrier,
                        wait_access,
                        link.vk_access_flags,
                        resource_state.image_layout,
                        link.vk_image_layout,
                        link.vk_image_aspect,
                        {});
      resource_state.image_layout = link.vk_image_layout;
    }
  }
}

void VKCommandBuilder::add_image_barrier(VkImage vk_image,
                                         Barrier &r_barrier,
                                         VkAccessFlags src_access_mask,
                                         VkAccessFlags dst_access_mask,
                                         VkImageLayout old_layout,
                                         VkImageLayout new_layout,
                                         VkImageAspectFlags aspect_mask,
                                         const VKSubImageRange &subimage)
{
  BLI_assert(aspect_mask != VK_IMAGE_ASPECT_NONE);
  for (VkImageMemoryBarrier &vk_image_memory_barrier :
       vk_image_memory_barriers_.as_mutable_span().drop_front(
           r_barrier.image_memory_barriers.start()))
  {
    if (vk_image_memory_barrier.image == vk_image) {
      /* When registering read/write buffers, it can be that the node internally requires
       * read/write. In this case we adjust the dstAccessMask of the read barrier. An example is
       * EEVEE update HIZ compute shader and shadow tagging. */
      if ((vk_image_memory_barrier.dstAccessMask & src_access_mask) == src_access_mask) {
        vk_image_memory_barrier.dstAccessMask |= dst_access_mask;
        return;
      }
      /* When re-registering resources we can skip if access mask already contain all the flags.
       */
      if ((vk_image_memory_barrier.dstAccessMask & dst_access_mask) == dst_access_mask &&
          (vk_image_memory_barrier.srcAccessMask & src_access_mask) == src_access_mask &&
          old_layout == new_layout)
      {
        return;
      }
    }
  }

  vk_image_memory_barriers_.append({VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                    nullptr,
                                    src_access_mask,
                                    dst_access_mask,
                                    old_layout,
                                    new_layout,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    vk_image,
                                    {aspect_mask,
                                     subimage.mipmap_level,
                                     subimage.mipmap_count,
                                     subimage.layer_base,
                                     subimage.layer_count}});
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sub-resource tracking
 * \{ */

void VKCommandBuilder::ImageTracker::begin(const VKRenderGraph &render_graph,
                                           NodeHandle node_handle)
{
  BLI_assert(render_graph.nodes_[node_handle].type == VKNodeType::BEGIN_RENDERING);
  tracked_attachments.clear();
  changes.clear();

  const VKRenderGraphNodeLinks &links = render_graph.links_[node_handle];
  for (const VKRenderGraphLink &link : links.outputs) {
    VKResourceStateTracker::Resource &resource = render_graph.resources_.resources_.lookup(
        link.resource.handle);
    if (resource.use_subresource_tracking()) {
      tracked_attachments.add(resource.image.vk_image);
    }
  }
}

void VKCommandBuilder::ImageTracker::update(VkImage vk_image,
                                            const VKSubImageRange &subimage,
                                            VkImageLayout old_layout,
                                            VkImageLayout new_layout,
                                            Barrier &r_barrier)
{
  for (const SubImageChange &change : changes) {
    if (change.vk_image == vk_image && ((subimage.layer_count != VK_REMAINING_ARRAY_LAYERS &&
                                         change.subimage.layer_base == subimage.layer_base) ||
                                        (subimage.mipmap_count != VK_REMAINING_MIP_LEVELS &&
                                         change.subimage.mipmap_level != subimage.mipmap_level)))
    {
      BLI_assert_msg(
          change.vk_image_layout == new_layout,
          "We don't support more that one change of the same subimage multiple times during a "
          "rendering scope.");
      /* Early exit as layer is in correct layout. This is a normal case as we expect multiple
       * draw commands to take place during a rendering scope with the same layer access. */
      return;
    }
  }

  changes.append({vk_image, new_layout, subimage});

  /* We should be able to do better. BOTTOM/TOP is really a worst case barrier. */
  r_barrier.src_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  r_barrier.dst_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  command_builder.add_image_barrier(vk_image,
                                    r_barrier,
                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
                                        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                        VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                                    old_layout,
                                    new_layout,
                                    VK_IMAGE_ASPECT_COLOR_BIT,
                                    subimage);
}

void VKCommandBuilder::ImageTracker::end(Barrier &r_barrier, bool use_local_read)
{
  suspend(r_barrier, use_local_read);
  tracked_attachments.clear();
  changes.clear();
}

void VKCommandBuilder::ImageTracker::suspend(Barrier &r_barrier, bool use_local_read)

{
  if (changes.is_empty()) {
    return;
  }

  command_builder.reset_barriers(r_barrier);
  /* We should be able to do better. BOTTOM/TOP is really a worst case barrier. */
  r_barrier.src_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  r_barrier.dst_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  int64_t start_index = command_builder.vk_image_memory_barriers_.size();
  r_barrier.image_memory_barriers = IndexRange::from_begin_size(start_index, 0);
  for (const SubImageChange &change : changes) {
    command_builder.add_image_barrier(
        change.vk_image,
        r_barrier,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
            VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
            VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
        change.vk_image_layout,
        use_local_read ? VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR :
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_ASPECT_COLOR_BIT,
        change.subimage);
    r_barrier.image_memory_barriers = r_barrier.image_memory_barriers.with_new_end(
        command_builder.vk_image_memory_barriers_.size());

#if 0
    std::cout << __func__ << ": transition layout image=" << binding.vk_image
              << ", layer=" << binding.layer << ", count=" << binding.layer_count
              << ", from_layout=" << to_string(binding.vk_image_layout)
              << ", to_layout=" << to_string(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) << "\n";
#endif
  }
}

void VKCommandBuilder::ImageTracker::resume(Barrier &r_barrier, bool use_local_read)
{
  if (changes.is_empty()) {
    return;
  }

  command_builder.reset_barriers(r_barrier);
  /* We should be able to do better. BOTTOM/TOP is really a worst case barrier. */
  r_barrier.src_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  r_barrier.dst_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  int64_t start_index = command_builder.vk_image_memory_barriers_.size();
  r_barrier.image_memory_barriers = IndexRange::from_begin_size(start_index, 0);

  for (const SubImageChange &change : changes) {
    command_builder.add_image_barrier(
        change.vk_image,
        r_barrier,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
            VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
            VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
        use_local_read ? VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR :
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        change.vk_image_layout,
        VK_IMAGE_ASPECT_COLOR_BIT,
        change.subimage);
#if 0
    std::cout << __func__ << ": transition layout image=" << binding.vk_image
              << ", layer=" << binding.layer << ", count=" << binding.layer_count
              << ", from_layout=" << to_string(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
              << ", to_layout=" << to_string(binding.vk_image_layout) << "\n";
#endif
  }
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Debugging tools
 * \{ */

std::string VKCommandBuilder::to_string_barrier(const Barrier &barrier)
{
  std::stringstream ss;
  ss << "src_stage_mask=" << to_string_vk_pipeline_stage_flags(barrier.src_stage_mask)
     << ", dst_stage_mask=" << to_string_vk_pipeline_stage_flags(barrier.dst_stage_mask) << "\n";
  for (const VkBufferMemoryBarrier &buffer_memory_barrier :
       vk_buffer_memory_barriers_.as_span().slice(barrier.buffer_memory_barriers))
  {
    ss << "  - src_access_mask=" << to_string_vk_access_flags(buffer_memory_barrier.srcAccessMask)
       << ", dst_access_mask=" << to_string_vk_access_flags(buffer_memory_barrier.dstAccessMask)
       << ", vk_buffer=" << to_string(buffer_memory_barrier.buffer) << "\n";
  }

  for (const VkImageMemoryBarrier &image_memory_barrier :
       vk_image_memory_barriers_.as_span().slice(barrier.image_memory_barriers))
  {
    ss << "  - src_access_mask=" << to_string_vk_access_flags(image_memory_barrier.srcAccessMask)
       << ", dst_access_mask=" << to_string_vk_access_flags(image_memory_barrier.dstAccessMask)
       << ", vk_image=" << to_string(image_memory_barrier.image)
       << ", old_layout=" << to_string(image_memory_barrier.oldLayout)
       << ", new_layout=" << to_string(image_memory_barrier.newLayout)
       << ", subresource_range=" << to_string(image_memory_barrier.subresourceRange, 2) << "\n";
  }

  return ss.str();
}

/** \} */

}  // namespace blender::gpu::render_graph
