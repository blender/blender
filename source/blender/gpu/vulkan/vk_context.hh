/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_context_private.hh"
#include "vk_command_buffer.hh"
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
  VKCommandBuffer command_buffer_;

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

  void memory_statistics_get(int *total_mem, int *free_mem) override;

  void debug_group_begin(const char *, int) override;
  void debug_group_end() override;
  bool debug_capture_begin() override;
  void debug_capture_end() override;
  void *debug_capture_scope_create(const char *name) override;
  bool debug_capture_scope_begin(void *scope) override;
  void debug_capture_scope_end(void *scope) override;

  bool has_active_framebuffer() const;
  void activate_framebuffer(VKFrameBuffer &framebuffer);
  void deactivate_framebuffer();
  VKFrameBuffer *active_framebuffer_get() const;

  void bind_compute_pipeline();
  void bind_graphics_pipeline(const GPUPrimType prim_type,
                              const VKVertexAttributeObject &vertex_attribute_object);
  void sync_backbuffer();

  static VKContext *get(void)
  {
    return static_cast<VKContext *>(Context::get());
  }

  VKCommandBuffer &command_buffer_get()
  {
    return command_buffer_;
  }

  const VKStateManager &state_manager_get() const;
  VKStateManager &state_manager_get();
};

}  // namespace blender::gpu
