/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_enum_flags.hh"

#include "gpu_context_private.hh"

#include "GHOST_Types.h"

#include "render_graph/vk_render_graph.hh"
#include "vk_common.hh"
#include "vk_debug.hh"
#include "vk_descriptor_pools.hh"
#include "vk_resource_pool.hh"
#include "vk_streaming_buffer.hh"

namespace blender::gpu {
class VKFrameBuffer;
class VKVertexAttributeObject;
class VKBatch;
class VKStateManager;
class VKShader;
class VKThreadData;
class VKDevice;

enum RenderGraphFlushFlags {
  NONE = 0,
  RENEW_RENDER_GRAPH = 1 << 0,
  SUBMIT = 1 << 1,
  WAIT_FOR_COMPLETION = 1 << 2,
};
ENUM_OPERATORS(RenderGraphFlushFlags);

class VKContext : public Context, NonCopyable {
  friend class VKDevice;

 private:
  VkExtent2D vk_extent_ = {};
  VkSurfaceFormatKHR swap_chain_format_ = {};
  gpu::Texture *surface_texture_ = nullptr;
  void *ghost_context_;

  Vector<std::unique_ptr<VKStreamingBuffer>> streaming_buffers_;

  /* Reusable data. Stored inside context to limit reallocations. */
  render_graph::VKResourceAccessInfo access_info_ = {};

  std::optional<std::reference_wrapper<VKThreadData>> thread_data_;
  std::optional<std::reference_wrapper<render_graph::VKRenderGraph>> render_graph_;

  /* Active shader specialization constants state. */
  shader::SpecializationConstants constants_state_;

  /* Debug scope timings. Adapted form GLContext::TimeQuery.
   * Only supports CPU timings for now. */
  struct ScopeTimings {
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Nanoseconds = std::chrono::nanoseconds;

    std::string name;
    bool finished;
    TimePoint cpu_start, cpu_end;
  };
  Vector<ScopeTimings> scope_timings;

  void process_frame_timings();

 public:
  VKDiscardPool discard_pool;

  const render_graph::VKRenderGraph &render_graph() const
  {
    return render_graph_.value().get();
  }
  render_graph::VKRenderGraph &render_graph()
  {
    return render_graph_.value().get();
  }

  VKContext(void *ghost_window, void *ghost_context);
  virtual ~VKContext();

  void activate() override;
  void deactivate() override;
  void begin_frame() override;
  void end_frame() override;

  void flush() override;

  TimelineValue flush_render_graph(
      RenderGraphFlushFlags flags,
      VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_NONE,
      VkSemaphore wait_semaphore = VK_NULL_HANDLE,
      VkSemaphore signal_semaphore = VK_NULL_HANDLE,
      VkFence signal_fence = VK_NULL_HANDLE);
  void finish() override;

  void memory_statistics_get(int *r_total_mem_kb, int *r_free_mem_kb) override;

  void debug_group_begin(const char *, int) override;
  void debug_group_end() override;
  bool debug_capture_begin(const char *title) override;
  void debug_capture_end() override;
  void *debug_capture_scope_create(const char *name) override;
  bool debug_capture_scope_begin(void *scope) override;
  void debug_capture_scope_end(void *scope) override;

  void debug_unbind_all_ubo() override;
  void debug_unbind_all_ssbo() override;

  bool has_active_framebuffer() const;
  void activate_framebuffer(VKFrameBuffer &framebuffer);
  void deactivate_framebuffer();
  VKFrameBuffer *active_framebuffer_get() const;

  /**
   * Ensure that the active framebuffer isn't rendering.
   *
   * Between `vkCmdBeginRendering` and `vkCmdEndRendering` the framebuffer is rendering. Dispatch
   * and transfer commands cannot be called between these commands. They can call this method to
   * ensure that the framebuffer is outside these calls.
   */
  void rendering_end();

  render_graph::VKResourceAccessInfo &reset_and_get_access_info();

  /**
   * Update the give shader data with the current state of the context.
   */
  void update_pipeline_data(render_graph::VKPipelineData &r_pipeline_data);
  void update_pipeline_data(GPUPrimType primitive,
                            VKVertexAttributeObject &vao,
                            render_graph::VKPipelineDataGraphics &r_pipeline_data);

  void sync_backbuffer();

  static VKContext *get()
  {
    return static_cast<VKContext *>(Context::get());
  }

  VKDescriptorPools &descriptor_pools_get();
  VKDescriptorSetTracker &descriptor_set_get();
  VKStateManager &state_manager_get() const;

  static void swap_buffer_draw_callback(const GHOST_VulkanSwapChainData *data);
  static void swap_buffer_acquired_callback();
  static void openxr_acquire_framebuffer_image_callback(GHOST_VulkanOpenXRData *data);
  static void openxr_release_framebuffer_image_callback(GHOST_VulkanOpenXRData *data);

  void specialization_constants_set(const shader::SpecializationConstants *constants_state);

  std::unique_ptr<VKStreamingBuffer> &get_or_create_streaming_buffer(
      VKBuffer &buffer, VkDeviceSize min_offset_alignment);

 private:
  void swap_buffer_draw_handler(const GHOST_VulkanSwapChainData &data);
  void swap_buffer_acquired_handler();

  void openxr_acquire_framebuffer_image_handler(GHOST_VulkanOpenXRData &data);
  void openxr_release_framebuffer_image_handler(GHOST_VulkanOpenXRData &data);

  void update_pipeline_data(VKShader &shader,
                            VkPipeline vk_pipeline,
                            render_graph::VKPipelineData &r_pipeline_data);
};

BLI_INLINE bool operator==(const VKContext &a, const VKContext &b)
{
  return static_cast<const void *>(&a) == static_cast<const void *>(&b);
}

}  // namespace blender::gpu
