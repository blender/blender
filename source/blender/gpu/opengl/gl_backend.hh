/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2020, Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_backend.hh"

#include "BLI_vector.hh"

#include "gl_batch.hh"
#include "gl_compute.hh"
#include "gl_context.hh"
#include "gl_drawlist.hh"
#include "gl_framebuffer.hh"
#include "gl_index_buffer.hh"
#include "gl_query.hh"
#include "gl_shader.hh"
#include "gl_texture.hh"
#include "gl_uniform_buffer.hh"
#include "gl_vertex_buffer.hh"

namespace blender {
namespace gpu {

class GLBackend : public GPUBackend {
 private:
  GLSharedOrphanLists shared_orphan_list_;

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
    GLTexture::samplers_free();

    GLBackend::platform_exit();
  }

  static GLBackend *get()
  {
    return static_cast<GLBackend *>(GPUBackend::get());
  }

  void samplers_update() override
  {
    GLTexture::samplers_update();
  };

  Context *context_alloc(void *ghost_window) override
  {
    return new GLContext(ghost_window, shared_orphan_list_);
  };

  Batch *batch_alloc() override
  {
    return new GLBatch();
  };

  DrawList *drawlist_alloc(int list_length) override
  {
    return new GLDrawList(list_length);
  };

  FrameBuffer *framebuffer_alloc(const char *name) override
  {
    return new GLFrameBuffer(name);
  };

  IndexBuf *indexbuf_alloc() override
  {
    return new GLIndexBuf();
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

  UniformBuf *uniformbuf_alloc(int size, const char *name) override
  {
    return new GLUniformBuf(size, name);
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

 private:
  static void platform_init();
  static void platform_exit();

  static void capabilities_init();
};

}  // namespace gpu
}  // namespace blender
