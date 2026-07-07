/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_set.hh"

#include "vk_common.hh"
#include "vk_render_graph_node.hh"
#include "vk_scheduler.hh"

namespace blender::gpu::render_graph {
class VKRenderGraph;

struct LayeredImageBinding {
  VkImage vk_image;
  VkImageLayout vk_image_layout;
  uint32_t layer;
  uint32_t layer_count;
};

/**
 * Build the command buffer for sending to the device queue.
 *
 * Determine which nodes needs to be scheduled, Then for each node generate the needed pipeline
 * barriers and commands.
 */
class VKCommandBuilder {
 private:
  /**
   * List of all extracted VkBufferMemoryBarriers. These barriers will be referenced by
   * Barrier::buffer_memory_barriers.
   */
  Vector<VkBufferMemoryBarrier> vk_buffer_memory_barriers_;
  /**
   * List of all extracted VkImageMemoryBarriers. These barriers will be referenced by
   * Barrier::image_memory_barriers.
   */
  Vector<VkImageMemoryBarrier> vk_image_memory_barriers_;

  struct Barrier {
    /** Index range into `VKCommandBuilder::vk_buffer_memory_barriers_` */
    IndexRange buffer_memory_barriers;
    /** Index range into `VKCommandBuilder::vk_image_memory_barriers_` */
    IndexRange image_memory_barriers;

    VkPipelineStageFlags src_stage_mask = VK_PIPELINE_STAGE_NONE;
    VkPipelineStageFlags dst_stage_mask = VK_PIPELINE_STAGE_NONE;

    bool is_empty() const
    {
      return buffer_memory_barriers.is_empty() && image_memory_barriers.is_empty();
    }
  };

  /**
   * An image can consist out of layers and mipmaps. In some cases each layer/mipmap can get
   * different VkImageLayouts. To not complicate the global state tracker we track sub-resources
   * only when building the barriers.
   */
  struct ImageTracker {
    /**
     * Local reference to the active command builder.
     *
     * The reference is used to add VkImageMemoryBarrier's to the command builder.
     */
    VKCommandBuilder &command_builder;
    ImageTracker(VKCommandBuilder &command_builder) : command_builder(command_builder) {}

    /**
     * All layered attachments of the last rendering scope (VKNodeType::BEGIN_RENDERING).
     *
     * When binding layer from these images we expect that they aren't used as attachment and can
     * be transitioned into a different image layout. These image layouts are stored in
     * `layered_bindings`.
     */
    Set<VkImage> tracked_attachments;

    /**
     * Keep track of the changes to images since the start of the rendering scope. This allows
     * reverting and rebuilding the correct state when ending or resuming the render scope.
     */
    struct SubImageChange {
      VkImage vk_image;
      VkImageLayout vk_image_layout;
      VKSubImageRange subimage;
    };
    Vector<SubImageChange> changes;

    /**
     * Update the layered attachments list when beginning a new render scope.
     *
     * node_handle should be a handle that points to a VKNodeType::BEGIN_RENDERING.
     * Any attachments that are layered will be added to the `tracked_attachments` list.
     */
    void begin(const VKRenderGraph &render_graph, NodeHandle node_handle);

    /**
     * Is layered tracking enabled for the given vk_image.
     */
    inline bool contains(VkImage vk_image) const
    {
      return tracked_attachments.contains(vk_image);
    }

    /**
     * Ensure the layout of a layer.
     *
     * - `old_layout` should be the expected layout of the full image.
     */
    void update(VkImage vk_image,
                const VKSubImageRange &subimage,
                VkImageLayout old_layout,
                VkImageLayout new_layout,
                Barrier &r_barrier);
    /**
     * Ensure the layout of a mipmap level.
     *
     * - `old_layout` should be the expected layout of the full image.
     */
    void update(VkImage vk_image,
                uint32_t mipmap_level,
                VkImageLayout old_layout,
                VkImageLayout new_layout,
                Barrier &r_barrier);

    /**
     * End layer tracking.
     *
     * All modified layers (layer_tracking_update) will be changed back to the image layout of
     * the texture (most likely a `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`).
     *
     * Render suspension/resuming will not work after calling this method.
     */
    void end(Barrier &r_barrier, bool use_local_read);

    /**
     * Suspend layer tracking
     *
     * Temporarily suspend layer tracking. This transits all modified layers back to its original
     * layout.
     * NOTE: Only call this method when you the rendering will be resumed, otherwise use
     * `layer_tracking_end`.
     */
    void suspend(Barrier &r_barrier, bool use_local_read);

    /**
     * Resume suspended layer tracking.
     *
     * Resume suspended layer tracking. This transits all registered layers back to its modified
     * state.
     */
    void resume(Barrier &r_barrier, bool use_local_read);
  };

  /**
   * Index range of the nodes of a group.
   *
   * The indexes are to `VKScheduler::result_` that is passed along `Span<NodeHandle> nodes` of
   * `build_nodes`.
   */
  using GroupNodes = IndexRange;

  /**
   * Index range into barrier_list_;
   */
  using Barriers = IndexRange;
  using BarrierIndex = int64_t;

  /** Per group store the indices of the nodes. */
  Vector<GroupNodes> group_nodes_;
  /** Barriers that will be recorded just before the commands of a group are recorded. */
  Vector<Barriers> group_pre_barriers_;
  /** Barriers that will be recorded after a group is recorded. */
  Vector<Barriers> group_post_barriers_;
  /**
   * Barriers that will be recorded just before the commands of a specific node are recorded. The
   * barriers are stored per NodeHandle and in the same order as `Span<NodeHandle> nodes`.
   */
  Vector<Barriers> node_pre_barriers_;

  /** List of all generated barriers. */
  Vector<Barrier> barrier_list_;

 public:
  /**
   * Build execution groups and barriers.
   * This method should be performed when the resources are locked.
   */
  void build_nodes(VKRenderGraph &render_graph,
                   VKCommandBufferInterface &command_buffer,
                   Span<NodeHandle> node_handles);

  /**
   * Record commands of the nodes provided by the `node_handles` parameter. The commands are
   * recorded into the given `command_buffer`.
   *
   * `build_nodes` needs to be called before calling with exact the same parameters.
   */
  void record_commands(VKRenderGraph &render_graph,
                       VKCommandBufferInterface &command_buffer,
                       Span<NodeHandle> node_handles);

 private:
  /**
   *  Split the node_handles in logical groups.
   *
   * A new group is created when the next node is switching from data/compute to graphics and each
   * data/compute is also put in its own group.
   */
  void groups_init(const VKRenderGraph &render_graph, Span<NodeHandle> node_handles);

  /**
   * Extract the memory/buffer/image barriers from the command groups and add them to the pre/post
   * barriers.
   *
   * This process is single threaded as resource states change during the extraction process. The
   * result of this function would allow the sub builders to be built in parallel.
   */
  void groups_extract_barriers(VKRenderGraph &render_graph,
                               Span<NodeHandle> node_handles,
                               bool supports_local_read);

  /**
   * Record all the commands for all the groups to the command buffer.
   */
  void groups_build_commands(VKRenderGraph &render_graph,
                             VKCommandBufferInterface &command_buffer,
                             Span<NodeHandle> node_handles);

  /**
   * Build the pipeline barriers that should be recorded before any other commands of the node
   * group the given node is part of is being recorded.
   */
  void build_pipeline_barriers(VKRenderGraph &render_graph,
                               NodeHandle node_handle,
                               VkPipelineStageFlags pipeline_stage,
                               ImageTracker &image_tracker,
                               Barrier &r_barrier,
                               bool within_rendering = false);
  void reset_barriers(Barrier &r_barrier);
  void send_pipeline_barriers(VKCommandBufferInterface &command_buffer,
                              const Barrier &barrier,
                              bool within_rendering);

  void add_buffer_barriers(VKRenderGraph &render_graph,
                           NodeHandle node_handle,
                           VkPipelineStageFlags node_stages,
                           Barrier &r_barrier);
  void add_buffer_barrier(VkBuffer vk_buffer,
                          Barrier &r_barrier,
                          VkAccessFlags src_access_mask,
                          VkAccessFlags dst_access_mask);
  void add_buffer_read_barriers(VKRenderGraph &render_graph,
                                NodeHandle node_handle,
                                VkPipelineStageFlags node_stages,
                                Barrier &r_barrier);
  void add_buffer_write_barriers(VKRenderGraph &render_graph,
                                 NodeHandle node_handle,
                                 VkPipelineStageFlags node_stages,
                                 Barrier &r_barrier);

  void add_image_barriers(VKRenderGraph &render_graph,
                          NodeHandle node_handle,
                          VkPipelineStageFlags node_stages,
                          ImageTracker &image_tracker,
                          Barrier &r_barrier,
                          bool within_rendering);
  void add_image_barrier(VkImage vk_image,
                         Barrier &r_barrier,
                         VkAccessFlags src_access_mask,
                         VkAccessFlags dst_access_mask,
                         VkImageLayout old_image_layout,
                         VkImageLayout new_image_layout,
                         VkImageAspectFlags aspect_mask,
                         const VKSubImageRange &subimage);
  void add_image_read_barriers(VKRenderGraph &render_graph,
                               NodeHandle node_handle,
                               VkPipelineStageFlags node_stages,
                               ImageTracker &image_tracker,
                               Barrier &r_barrier,
                               bool within_rendering);
  void add_image_write_barriers(VKRenderGraph &render_graph,
                                NodeHandle node_handle,
                                VkPipelineStageFlags node_stages,
                                ImageTracker &image_tracker,
                                Barrier &r_barrier,
                                bool within_rendering);

  struct DebugGroups {
    /**
     * Index of the active debug_group. Points to an element in
     * `VKRenderGraph.debug_.used_groups`.
     */
    int64_t active_debug_group_id = -1;
    /** Current level of debug groups. (number of nested debug groups). */
    int debug_level = 0;
  };

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
                            DebugGroups &debug_groups,
                            NodeHandle node_handle);

  /**
   * Make sure no debugging groups are active anymore.
   */
  void finish_debug_groups(VKCommandBufferInterface &command_buffer, DebugGroups &debug_groups);

 private:
  /**
   * Update the layered attachments list when beginning a new render scope.
   */
  void layer_tracking_begin(const VKRenderGraph &render_graph, NodeHandle node_handle);

  /**
   * Ensure the layout of a layer.
   *
   * - `old_layout` should be the expected layout of the full image.
   */
  void layer_tracking_update(VkImage vk_image,
                             uint32_t layer,
                             uint32_t layer_count,
                             VkImageLayout old_layout,
                             VkImageLayout new_layout);

  /**
   * End layer tracking.
   *
   * All modified layers (layer_tracking_update) will be changed back to the image layout of
   * the texture (most likely a `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`).
   *
   * Render suspension/resuming will not work after calling this method.
   */
  void layer_tracking_end(VKCommandBufferInterface &command_buffer);

  /**
   * Suspend layer tracking
   *
   * Temporarily suspend layer tracking. This transits all modified layers back to its original
   * layout.
   * NOTE: Only call this method when you the rendering will be resumed, otherwise use
   * `layer_tracking_end`.
   */
  void layer_tracking_suspend(VKCommandBufferInterface &command_buffer);

  /**
   * Resume suspended layer tracking.
   *
   * Resume suspended layer tracking. This transits all registered layers back to its modified
   * state.
   */
  void layer_tracking_resume(VKCommandBufferInterface &command_buffer);

  bool node_has_input_attachments(const VKRenderGraph &render_graph, NodeHandle node);

  std::string to_string_barrier(const Barrier &barrier);
};

}  // namespace blender::gpu::render_graph
