/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPUBackend derived class contain allocators that do not need a context bound.
 * The backend is init at startup and is accessible using GPU_backend_get() */

#pragma once

#include "GPU_vertex_buffer.h"

namespace blender {
namespace gpu {

class Context;

class Batch;
class DrawList;
class Fence;
class FrameBuffer;
class IndexBuf;
class PixelBuffer;
class QueryPool;
class Shader;
class Texture;
class UniformBuf;
class StorageBuf;
class VertBuf;

class GPUBackend {
 public:
  virtual ~GPUBackend() = default;
  virtual void delete_resources() = 0;

  static GPUBackend *get();

  virtual void samplers_update() = 0;
  virtual void compute_dispatch(int groups_x_len, int groups_y_len, int groups_z_len) = 0;
  virtual void compute_dispatch_indirect(StorageBuf *indirect_buf) = 0;

  virtual Context *context_alloc(void *ghost_window, void *ghost_context) = 0;

  virtual Batch *batch_alloc() = 0;
  virtual DrawList *drawlist_alloc(int list_length) = 0;
  virtual Fence *fence_alloc() = 0;
  virtual FrameBuffer *framebuffer_alloc(const char *name) = 0;
  virtual IndexBuf *indexbuf_alloc() = 0;
  virtual PixelBuffer *pixelbuf_alloc(uint size) = 0;
  virtual QueryPool *querypool_alloc() = 0;
  virtual Shader *shader_alloc(const char *name) = 0;
  virtual Texture *texture_alloc(const char *name) = 0;
  virtual UniformBuf *uniformbuf_alloc(int size, const char *name) = 0;
  virtual StorageBuf *storagebuf_alloc(int size, GPUUsageType usage, const char *name) = 0;
  virtual VertBuf *vertbuf_alloc() = 0;

  /* Render Frame Coordination --
   * Used for performing per-frame actions globally */
  virtual void render_begin() = 0;
  virtual void render_end() = 0;
  virtual void render_step() = 0;
};

}  // namespace gpu
}  // namespace blender
