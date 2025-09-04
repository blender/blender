/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPUBackend derived class contain allocators that do not need a context bound.
 * The backend is init at startup and is accessible using GPU_backend_get() */

#pragma once

#include "BLI_color_types.hh"
#include "BLI_string_ref.hh"
#include "GPU_vertex_buffer.hh"

namespace blender::gpu {

class Context;

class Batch;
class Fence;
class FrameBuffer;
class IndexBuf;
class PixelBuffer;
class QueryPool;
class Shader;
class ShaderCompiler;
class Texture;
class UniformBuf;
class StorageBuf;
class VertBuf;

class GPUBackend {
 protected:
  ShaderCompiler *compiler_;

 public:
  virtual ~GPUBackend() = default;

  /* Called after the main context creation and activation. */
  virtual void init_resources() = 0;
  /* Called before the main context deletion and deactivation. */
  virtual void delete_resources() = 0;

  static GPUBackend *get();

  ShaderCompiler *get_compiler()
  {
    return compiler_;
  }

  virtual void samplers_update() = 0;
  virtual void compute_dispatch(int groups_x_len, int groups_y_len, int groups_z_len) = 0;
  virtual void compute_dispatch_indirect(StorageBuf *indirect_buf) = 0;

  virtual Context *context_alloc(void *ghost_window, void *ghost_context) = 0;

  virtual Batch *batch_alloc() = 0;
  virtual Fence *fence_alloc() = 0;
  virtual FrameBuffer *framebuffer_alloc(const char *name) = 0;
  virtual IndexBuf *indexbuf_alloc() = 0;
  virtual PixelBuffer *pixelbuf_alloc(size_t size) = 0;
  virtual QueryPool *querypool_alloc() = 0;
  virtual Shader *shader_alloc(const char *name) = 0;
  virtual Texture *texture_alloc(const char *name) = 0;
  virtual UniformBuf *uniformbuf_alloc(size_t size, const char *name) = 0;
  virtual StorageBuf *storagebuf_alloc(size_t size, GPUUsageType usage, const char *name) = 0;
  virtual VertBuf *vertbuf_alloc() = 0;
  virtual void shader_cache_dir_clear_old() = 0;

  /* Render Frame Coordination --
   * Used for performing per-frame actions globally */
  virtual void render_begin() = 0;
  virtual void render_end() = 0;
  virtual void render_step(bool force_resource_release = false) = 0;
};

namespace debug {
static blender::ColorTheme4f GPU_DEBUG_GROUP_COLOR_DEFAULT = {};

static inline ColorTheme4f get_debug_group_color(StringRefNull name)
{
  if (name == "EEVEE") {
    return ColorTheme4f(1.0, 0.5, 0.0, 1.0);
  }
  if (name == "External") {
    return ColorTheme4f(0.0, 0.0, 1.0, 1.0);
  }
  if (name == "GpencilMode") {
    return ColorTheme4f(1.0, 1.0, 0.0, 1.0);
  }
  if (name == "UV/Image") {
    return ColorTheme4f(0.0, 1.0, 1.0, 1.0);
  }
  if (name == "Overlay") {
    return ColorTheme4f(0.0, 1.0, 0.5, 1.0);
  }
  if (name == "Workbench") {
    return ColorTheme4f(0.0, 0.7, 1.0, 1.0);
  }
  if (name == "Cycles") {
    return ColorTheme4f(0.0, 0.5, 1.0, 1.0);
  }
  if (name == "BackBuffer.Blit") {
    return ColorTheme4f(0.5, 0.7, 1.0, 1.0);
  }
  if (name == "Compositor") {
    return ColorTheme4f(1.0, 0.5, 0.7, 1.0);
  }
  return GPU_DEBUG_GROUP_COLOR_DEFAULT;
}
}  // namespace debug

}  // namespace blender::gpu
