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
#include "vk_shader_compiler.hh"

namespace blender::gpu {

class VKContext;
class VKDescriptorSet;
class VKDescriptorSetTracker;

class VKBackend : public GPUBackend {
 private:
#ifdef WITH_RENDERDOC
  renderdoc::api::Renderdoc renderdoc_api_;
#endif

 public:
  VKShaderCompiler shader_compiler;
  /* Global instance to device handles. */
  VKDevice device;

  VKBackend()
  {
    platform_init();
  }

  virtual ~VKBackend()
  {
    VKBackend::platform_exit();
  }

  /**
   * Does the running platform contain any device that meets the minimum requirements to start the
   * Vulkan backend.
   *
   * Function is used to validate that a Blender UI can be started. It calls vulkan API commands
   * directly to ensure no parts of Blender needs to be initialized.
   */
  static bool is_supported();

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
  PixelBuffer *pixelbuf_alloc(size_t size) override;
  QueryPool *querypool_alloc() override;
  Shader *shader_alloc(const char *name) override;
  Texture *texture_alloc(const char *name) override;
  UniformBuf *uniformbuf_alloc(size_t size, const char *name) override;
  StorageBuf *storagebuf_alloc(size_t size, GPUUsageType usage, const char *name) override;
  VertBuf *vertbuf_alloc() override;

  void shader_cache_dir_clear_old() override
  {
    VKShaderCompiler::cache_dir_clear_old();
  }

  /* Render Frame Coordination --
   * Used for performing per-frame actions globally */
  void render_begin() override;
  void render_end() override;
  void render_step() override;

  bool debug_capture_begin(const char *title);
  void debug_capture_end();

  static VKBackend &get()
  {
    return *static_cast<VKBackend *>(GPUBackend::get());
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
