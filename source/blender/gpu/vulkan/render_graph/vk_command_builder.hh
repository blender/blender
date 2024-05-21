/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "vk_common.hh"
#include "vk_render_graph_node.hh"
#include "vk_scheduler.hh"

namespace blender::gpu::render_graph {
class VKRenderGraph;

/**
 * Build the command buffer for sending to the device queue.
 *
 * Determine which nodes needs to be scheduled, Then for each node generate the needed pipeline
 * barriers and commands.
 */
class VKCommandBuilder {
 private:
  /* Pool of VKBufferMemoryBarriers that can be reused when building barriers */
  Vector<VkBufferMemoryBarrier> vk_buffer_memory_barriers_;
  Vector<VkImageMemoryBarrier> vk_image_memory_barriers_;

  /** Template buffer memory barrier. */
  VkBufferMemoryBarrier vk_buffer_memory_barrier_;
  /** Template image memory barrier. */
  VkImageMemoryBarrier vk_image_memory_barrier_;

  struct {
    /**
     * State of the bound pipelines during command building.
     */
    VKBoundPipelines active_pipelines;

    /**
     * When building memory barriers we need to track the src_stage_mask and dst_stage_mask and
     * pass them to
     * `https://docs.vulkan.org/spec/latest/chapters/synchronization.html#vkCmdPipelineBarrier`
     *
     * NOTE: Only valid between `reset_barriers` and `send_pipeline_barriers`.
     */
    VkPipelineStageFlags src_stage_mask = VK_PIPELINE_STAGE_NONE;
    VkPipelineStageFlags dst_stage_mask = VK_PIPELINE_STAGE_NONE;

    /**
     * Index of the active debug_group. Points to an element in
     * `VKRenderGraph.debug_.used_groups`.
     */
    int64_t active_debug_group_index = -1;
    /** Current level of debug groups. (number of nested debug groups). */
    int debug_level = 0;
  } state_;

 public:
  VKCommandBuilder();

  /**
   * Build the commands of the nodes provided by the `node_handles` parameter. The commands are
   * recorded into the given `command_buffer`.
   *
   * Pre-condition:
   * - `command_buffer` must not be in initial state according to
   *   https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html#commandbuffers-lifecycle
   *
   * Post-condition:
   * - `command_buffer` will be in executable state according to
   *   https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html#commandbuffers-lifecycle
   */
  void build_nodes(VKRenderGraph &render_graph,
                   VKCommandBufferInterface &command_buffer,
                   Span<NodeHandle> node_handles);

 private:
  /**
   * Build the commands for the given node_handle and node.
   *
   * The dependencies of the node handle are checked and when needed a pipeline barrier will be
   * generated and added to the command buffer.
   *
   * Based on the node.type the correct node class will be used for adding commands to the command
   * buffer.
   */
  void build_node(VKRenderGraph &render_graph,
                  VKCommandBufferInterface &command_buffer,
                  NodeHandle node_handle,
                  VKRenderGraphNode &node);

  /**
   * Build the pipeline barriers that should be recorded before the given node handle.
   */
  void build_pipeline_barriers(VKRenderGraph &render_graph,
                               VKCommandBufferInterface &command_buffer,
                               NodeHandle node_handle,
                               VkPipelineStageFlags pipeline_stage);
  void reset_barriers();
  void send_pipeline_barriers(VKCommandBufferInterface &command_buffer);

  void add_buffer_barriers(VKRenderGraph &render_graph,
                           NodeHandle node_handle,
                           VkPipelineStageFlags node_stages);
  void add_buffer_barrier(VkBuffer vk_buffer,
                          VkAccessFlags src_access_mask,
                          VkAccessFlags dst_access_mask);
  void add_buffer_read_barriers(VKRenderGraph &render_graph,
                                NodeHandle node_handle,
                                VkPipelineStageFlags node_stages);
  void add_buffer_write_barriers(VKRenderGraph &render_graph,
                                 NodeHandle node_handle,
                                 VkPipelineStageFlags node_stages);

  void add_image_barriers(VKRenderGraph &render_graph,
                          NodeHandle node_handle,
                          VkPipelineStageFlags node_stages);
  void add_image_barrier(VkImage vk_image,
                         VkAccessFlags src_access_mask,
                         VkAccessFlags dst_access_mask,
                         VkImageLayout old_image_layout,
                         VkImageLayout new_image_layout,
                         VkImageAspectFlags aspect_mask);
  void add_image_read_barriers(VKRenderGraph &render_graph,
                               NodeHandle node_handle,
                               VkPipelineStageFlags node_stages);
  void add_image_write_barriers(VKRenderGraph &render_graph,
                                NodeHandle node_handle,
                                VkPipelineStageFlags node_stages);

  /**
   * Ensure that the debug group associated with the given node_handle is activated.
   *
   * When activating it determines how to walk from the current debug group to the to be activated
   * debug group by performing end/begin commands on the command buffer.
   *
   * This ensures that when nodes are reordered that they still appear in the right debug group.
   */
  void activate_debug_group(VKRenderGraph &render_graph,
                            VKCommandBufferInterface &command_buffer,
                            NodeHandle node_handle);

  /**
   * Make sure no debugging groups are active anymore.
   */
  void finish_debug_groups(VKCommandBufferInterface &command_buffer);
};

}  // namespace blender::gpu::render_graph
