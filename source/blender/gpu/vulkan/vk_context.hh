/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_context_private.hh"

#include "GHOST_Types.h"

#include "vk_command_buffers.hh"
#include "vk_common.hh"
#include "vk_debug.hh"
#include "vk_descriptor_pools.hh"

namespace blender::gpu {
class VKFrameBuffer;
class VKVertexAttributeObject;
class VKBatch;
class VKStateManager;

class VKContext : public Context, NonCopyable {
 private:
  VKCommandBuffers command_buffers_;
  VKDescriptorPools descriptor_pools_;
  VKDescriptorSetTracker descriptor_set_;

  VkExtent2D vk_extent_ = {};
  VkFormat swap_chain_format_ = {};
  GPUTexture *surface_texture_ = nullptr;
  void *ghost_context_;

 public:
  VKContext(void *ghost_window, void *ghost_context);
  virtual ~VKContext();

  void activate() override;
  void deactivate() override;
  void begin_frame() override;
  void end_frame() override;

  void flush() override;
  void finish() override;

  void memory_statistics_get(int *r_total_mem_kb, int *r_free_mem_kb) override;

  void debug_group_begin(const char *, int) override;
  void debug_group_end() override;
  bool debug_capture_begin(const char *title) override;
  void debug_capture_end() override;
  void *debug_capture_scope_create(const char *name) override;
  bool debug_capture_scope_begin(void *scope) override;
  void debug_capture_scope_end(void *scope) override;

  void debug_unbind_all_ubo() override{};
  void debug_unbind_all_ssbo() override{};

  bool has_active_framebuffer() const;
  void activate_framebuffer(VKFrameBuffer &framebuffer);
  void deactivate_framebuffer();
  VKFrameBuffer *active_framebuffer_get() const;

  void bind_compute_pipeline();
  void bind_graphics_pipeline(const GPUPrimType prim_type,
                              const VKVertexAttributeObject &vertex_attribute_object);
  void sync_backbuffer();

  static VKContext *get()
  {
    return static_cast<VKContext *>(Context::get());
  }

  VKCommandBuffers &command_buffers_get()
  {
    return command_buffers_;
  }

  VKDescriptorPools &descriptor_pools_get()
  {
    return descriptor_pools_;
  }

  VKDescriptorSetTracker &descriptor_set_get()
  {
    return descriptor_set_;
  }

  VKStateManager &state_manager_get() const;

  static void swap_buffers_pre_callback(const GHOST_VulkanSwapChainData *data);
  static void swap_buffers_post_callback();

 private:
  void swap_buffers_pre_handler(const GHOST_VulkanSwapChainData &data);
  void swap_buffers_post_handler();
};

BLI_INLINE bool operator==(const VKContext &a, const VKContext &b)
{
  return static_cast<const void *>(&a) == static_cast<const void *>(&b);
}

}  // namespace blender::gpu
