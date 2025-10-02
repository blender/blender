/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "GPU_capabilities.hh"
#include "GPU_platform.hh"

#include "gpu_backend.hh"

#include "BLI_threads.h"
#include "BLI_vector.hh"

#include "gpu_capabilities_private.hh"

#ifdef WITH_RENDERDOC
#  include "renderdoc_api.hh"
#endif

#include "gl_batch.hh"
#include "gl_compilation_subprocess.hh"
#include "gl_compute.hh"
#include "gl_context.hh"
#include "gl_framebuffer.hh"
#include "gl_index_buffer.hh"
#include "gl_query.hh"
#include "gl_shader.hh"
#include "gl_storage_buffer.hh"
#include "gl_texture.hh"
#include "gl_uniform_buffer.hh"
#include "gl_vertex_buffer.hh"

namespace blender {
namespace gpu {

class GLBackend : public GPUBackend {
 private:
  GLSharedOrphanLists shared_orphan_list_;
#ifdef WITH_RENDERDOC
  renderdoc::api::Renderdoc renderdoc_;
#endif

 public:
  GLBackend()
  {
    /* platform_init needs to go first. */
    GLBackend::platform_init();

    GLBackend::capabilities_init();
    GLTexture::samplers_init();
  }
  ~GLBackend()
  {
    GLBackend::platform_exit();
  }

  void init_resources() override
  {
    if (GCaps.use_subprocess_shader_compilations) {
      compiler_ = MEM_new<GLSubprocessShaderCompiler>(__func__);
    }
    else {
      compiler_ = MEM_new<GLShaderCompiler>(__func__);
    }
  };

  void delete_resources() override
  {
    /* Delete any resources with context active. */
    GLTexture::samplers_free();
    MEM_delete(compiler_);
  }

  static GLBackend *get()
  {
    return static_cast<GLBackend *>(GPUBackend::get());
  }

  void samplers_update() override
  {
    GLTexture::samplers_update();
  };

  Context *context_alloc(void *ghost_window, void * /*ghost_context*/) override
  {
    return new GLContext(ghost_window, shared_orphan_list_);
  };

  Batch *batch_alloc() override
  {
    return new GLBatch();
  };

  Fence *fence_alloc() override
  {
    return new GLFence();
  };

  FrameBuffer *framebuffer_alloc(const char *name) override
  {
    return new GLFrameBuffer(name);
  };

  IndexBuf *indexbuf_alloc() override
  {
    return new GLIndexBuf();
  };

  PixelBuffer *pixelbuf_alloc(size_t size) override
  {
    return new GLPixelBuffer(size);
  };

  QueryPool *querypool_alloc() override
  {
    return new GLQueryPool();
  };

  Shader *shader_alloc(const char *name) override
  {
    return new GLShader(name);
  };

  Texture *texture_alloc(const char *name) override
  {
    return new GLTexture(name);
  };

  UniformBuf *uniformbuf_alloc(size_t size, const char *name) override
  {
    return new GLUniformBuf(size, name);
  };

  StorageBuf *storagebuf_alloc(size_t size, GPUUsageType usage, const char *name) override
  {
    return new GLStorageBuf(size, usage, name);
  };

  VertBuf *vertbuf_alloc() override
  {
    return new GLVertBuf();
  };

  GLSharedOrphanLists &shared_orphan_list_get()
  {
    return shared_orphan_list_;
  };

  void compute_dispatch(int groups_x_len, int groups_y_len, int groups_z_len) override
  {
    GLContext::get()->state_manager_active_get()->apply_state();
    GLCompute::dispatch(groups_x_len, groups_y_len, groups_z_len);
  }

  void compute_dispatch_indirect(StorageBuf *indirect_buf) override
  {
    GLContext::get()->state_manager_active_get()->apply_state();

    dynamic_cast<GLStorageBuf *>(indirect_buf)->bind_as(GL_DISPATCH_INDIRECT_BUFFER);
    /* This barrier needs to be here as it only work on the currently bound indirect buffer. */
    glMemoryBarrier(GL_COMMAND_BARRIER_BIT);

    glDispatchComputeIndirect((GLintptr)0);
    /* Unbind. */
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);
  }

  void shader_cache_dir_clear_old() override
  {
#if BLI_SUBPROCESS_SUPPORT
    GL_shader_cache_dir_clear_old();
#endif
  }

  /* Render Frame Coordination */
  void render_begin() override {};
  void render_end() override {};
  void render_step(bool /*force_resource_release*/) override {};

  bool debug_capture_begin(const char *title);
  void debug_capture_end();

 private:
  static void platform_init();
  static void platform_exit();

  static void capabilities_init();
};

}  // namespace gpu
}  // namespace blender
