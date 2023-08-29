/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_backend.hh"

#ifdef WITH_RENDERDOC
#  include "renderdoc_api.hh"
#endif

#include "vk_common.hh"
#include "vk_device.hh"

#include "shaderc/shaderc.hpp"

namespace blender::gpu {

class VKContext;
class VKDescriptorSet;
class VKDescriptorSetTracker;

class VKBackend : public GPUBackend {
 private:
  shaderc::Compiler shaderc_compiler_;
#ifdef WITH_RENDERDOC
  renderdoc::api::Renderdoc renderdoc_api_;
#endif
  /* Global instance to device handles. */
  VKDevice device_;

 public:
  VKBackend()
  {
    platform_init();
  }

  virtual ~VKBackend()
  {
    VKBackend::platform_exit();
  }

  void delete_resources() override;

  void samplers_update() override;
  void compute_dispatch(int groups_x_len, int groups_y_len, int groups_z_len) override;
  void compute_dispatch_indirect(StorageBuf *indirect_buf) override;

  Context *context_alloc(void *ghost_window, void *ghost_context) override;

  Batch *batch_alloc() override;
  DrawList *drawlist_alloc(int list_length) override;
  Fence *fence_alloc() override;
  FrameBuffer *framebuffer_alloc(const char *name) override;
  IndexBuf *indexbuf_alloc() override;
  PixelBuffer *pixelbuf_alloc(uint size) override;
  QueryPool *querypool_alloc() override;
  Shader *shader_alloc(const char *name) override;
  Texture *texture_alloc(const char *name) override;
  UniformBuf *uniformbuf_alloc(int size, const char *name) override;
  StorageBuf *storagebuf_alloc(int size, GPUUsageType usage, const char *name) override;
  VertBuf *vertbuf_alloc() override;

  /* Render Frame Coordination --
   * Used for performing per-frame actions globally */
  void render_begin() override;
  void render_end() override;
  void render_step() override;

  bool debug_capture_begin();
  void debug_capture_end();

  shaderc::Compiler &get_shaderc_compiler();

  static VKBackend &get()
  {
    return *static_cast<VKBackend *>(GPUBackend::get());
  }

  const VKDevice &device_get() const
  {
    return device_;
  }

  static void platform_init(const VKDevice &device);
  static void capabilities_init(VKDevice &device);

 private:
  static void detect_workarounds(VKDevice &device);
  static void platform_init();
  static void platform_exit();

  /* These classes are allowed to modify the global device. */
  friend class VKContext;
  friend class VKDescriptorSet;
  friend class VKDescriptorSetTracker;
};

}  // namespace blender::gpu
