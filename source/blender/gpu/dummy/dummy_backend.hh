/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_backend.hh"
#include "gpu_capabilities_private.hh"
#include "gpu_platform_private.hh"

#include "dummy_batch.hh"
#include "dummy_context.hh"
#include "dummy_framebuffer.hh"
#include "dummy_vertex_buffer.hh"

namespace blender::gpu {

class DummyBackend : public GPUBackend {
 public:
  DummyBackend()
  {
    GPG.init(GPU_DEVICE_ANY,
             GPU_OS_ANY,
             GPU_DRIVER_ANY,
             GPU_SUPPORT_LEVEL_UNSUPPORTED,
             GPU_BACKEND_NONE,
             "Unknown",
             "",
             "");
  }
  void delete_resources() override {}
  void samplers_update() override {}
  void compute_dispatch(int /*groups_x_len*/, int /*groups_y_len*/, int /*groups_z_len*/) override
  {
  }
  void compute_dispatch_indirect(StorageBuf * /*indirect_buf*/) override {}
  Context *context_alloc(void * /*ghost_window*/, void * /*ghost_context*/) override
  {
    return new DummyContext;
  }
  Batch *batch_alloc() override
  {
    return new DummyBatch;
  }
  DrawList *drawlist_alloc(int /*list_length*/) override
  {
    return nullptr;
  }
  Fence *fence_alloc() override
  {
    return nullptr;
  }
  FrameBuffer *framebuffer_alloc(const char *name) override
  {
    return new DummyFrameBuffer(name);
  }
  IndexBuf *indexbuf_alloc() override
  {
    return nullptr;
  }
  PixelBuffer *pixelbuf_alloc(uint /*size*/) override
  {
    return nullptr;
  }
  QueryPool *querypool_alloc() override
  {
    return nullptr;
  }
  Shader *shader_alloc(const char * /*name*/) override
  {
    return nullptr;
  }
  Texture *texture_alloc(const char * /*name*/) override
  {
    return nullptr;
  }
  UniformBuf *uniformbuf_alloc(int /*size*/, const char * /*name*/) override
  {
    return nullptr;
  }
  StorageBuf *storagebuf_alloc(int /*size*/,
                               GPUUsageType /*usage*/,
                               const char * /*name*/) override
  {
    return nullptr;
  }
  VertBuf *vertbuf_alloc() override
  {
    return new DummyVertexBuffer;
  }
  void render_begin() override {}
  void render_end() override {}
  void render_step() override {}
};

}  // namespace blender::gpu
