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
 * The Original Code is Copyright (C) 2016 by Mike Erwin.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * Manage GL vertex array IDs in a thread-safe way
 * Use these instead of glGenBuffers & its friends
 * - alloc must be called from a thread that is bound
 *   to the context that will be used for drawing with
 *   this vao.
 * - free can be called from any thread
 */

/* TODO Create cmake option. */
#define WITH_OPENGL_BACKEND 1

#include "BLI_assert.h"
#include "BLI_utildefines.h"

#include "GPU_context.h"
#include "GPU_framebuffer.h"

#include "GHOST_C-api.h"

#include "gpu_backend.hh"
#include "gpu_batch_private.hh"
#include "gpu_context_private.hh"
#include "gpu_matrix_private.h"

#ifdef WITH_OPENGL_BACKEND
#  include "gl_backend.hh"
#  include "gl_context.hh"
#endif

#include <mutex>
#include <vector>

using namespace blender::gpu;

static thread_local GPUContext *active_ctx = NULL;

/* -------------------------------------------------------------------- */
/** \name GPUContext methods
 * \{ */

GPUContext::GPUContext()
{
  thread_ = pthread_self();
  is_active_ = false;
  matrix_state = GPU_matrix_state_create();
}

GPUContext::~GPUContext()
{
  GPU_matrix_state_discard(matrix_state);
  delete state_manager;
}

bool GPUContext::is_active_on_thread(void)
{
  return (this == active_ctx) && pthread_equal(pthread_self(), thread_);
}

/** \} */

/* -------------------------------------------------------------------- */

GPUContext *GPU_context_create(void *ghost_window)
{
  if (GPUBackend::get() == NULL) {
    /* TODO move where it make sense. */
    GPU_backend_init(GPU_BACKEND_OPENGL);
  }

  GPUContext *ctx = GPUBackend::get()->context_alloc(ghost_window);

  GPU_context_active_set(ctx);
  return ctx;
}

/* to be called after GPU_context_active_set(ctx_to_destroy) */
void GPU_context_discard(GPUContext *ctx)
{
  delete ctx;
  active_ctx = NULL;
}

/* ctx can be NULL */
void GPU_context_active_set(GPUContext *ctx)
{
  if (active_ctx) {
    active_ctx->deactivate();
  }

  active_ctx = ctx;

  if (ctx) {
    ctx->activate();
  }
}

GPUContext *GPU_context_active_get(void)
{
  return active_ctx;
}

GLuint GPU_vao_default(void)
{
  BLI_assert(active_ctx); /* need at least an active context */
  return static_cast<GLContext *>(active_ctx)->default_vao_;
}

GLuint GPU_framebuffer_default(void)
{
  BLI_assert(active_ctx); /* need at least an active context */
  return static_cast<GLContext *>(active_ctx)->default_framebuffer_;
}

GLuint GPU_vao_alloc(void)
{
  GLuint new_vao_id = 0;
  glGenVertexArrays(1, &new_vao_id);
  return new_vao_id;
}

GLuint GPU_fbo_alloc(void)
{
  GLuint new_fbo_id = 0;
  glGenFramebuffers(1, &new_fbo_id);
  return new_fbo_id;
}

GLuint GPU_buf_alloc(void)
{
  GLuint new_buffer_id = 0;
  glGenBuffers(1, &new_buffer_id);
  return new_buffer_id;
}

GLuint GPU_tex_alloc(void)
{
  GLuint new_texture_id = 0;
  glGenTextures(1, &new_texture_id);
  return new_texture_id;
}

void GPU_vao_free(GLuint vao_id, GPUContext *ctx)
{
  static_cast<GLContext *>(ctx)->vao_free(vao_id);
}

void GPU_fbo_free(GLuint fbo_id, GPUContext *ctx)
{
  static_cast<GLContext *>(ctx)->fbo_free(fbo_id);
}

void GPU_buf_free(GLuint buf_id)
{
  /* TODO avoid using backend */
  GPUBackend *backend = GPUBackend::get();
  static_cast<GLBackend *>(backend)->buf_free(buf_id);
}

void GPU_tex_free(GLuint tex_id)
{
  /* TODO avoid using backend */
  GPUBackend *backend = GPUBackend::get();
  static_cast<GLBackend *>(backend)->tex_free(tex_id);
}

/* GPUBatch & GPUFrameBuffer contains respectively VAO & FBO indices
 * which are not shared across contexts. So we need to keep track of
 * ownership. */

void gpu_context_add_framebuffer(GPUContext *ctx, GPUFrameBuffer *fb)
{
#ifdef DEBUG
  BLI_assert(ctx);
  static_cast<GLContext *>(ctx)->framebuffer_register(fb);
#else
  UNUSED_VARS(ctx, fb);
#endif
}

void gpu_context_remove_framebuffer(GPUContext *ctx, GPUFrameBuffer *fb)
{
#ifdef DEBUG
  BLI_assert(ctx);
  static_cast<GLContext *>(ctx)->framebuffer_unregister(fb);
#else
  UNUSED_VARS(ctx, fb);
#endif
}

void gpu_context_active_framebuffer_set(GPUContext *ctx, GPUFrameBuffer *fb)
{
  ctx->current_fbo = fb;
}

GPUFrameBuffer *gpu_context_active_framebuffer_get(GPUContext *ctx)
{
  return ctx->current_fbo;
}

struct GPUMatrixState *gpu_context_active_matrix_state_get()
{
  BLI_assert(active_ctx);
  return active_ctx->matrix_state;
}

/* -------------------------------------------------------------------- */
/** \name Main context global mutex
 *
 * Used to avoid crash on some old drivers.
 * \{ */

static std::mutex main_context_mutex;

void GPU_context_main_lock(void)
{
  main_context_mutex.lock();
}

void GPU_context_main_unlock(void)
{
  main_context_mutex.unlock();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Backend selection
 * \{ */

static GPUBackend *g_backend;

void GPU_backend_init(eGPUBackendType backend_type)
{
  BLI_assert(g_backend == NULL);

  switch (backend_type) {
#if WITH_OPENGL_BACKEND
    case GPU_BACKEND_OPENGL:
      g_backend = new GLBackend;
      break;
#endif
    default:
      BLI_assert(0);
      break;
  }
}

void GPU_backend_exit(void)
{
  /* TODO assert no resource left. Currently UI textures are still not freed in their context
   * correctly. */
  delete g_backend;
}

GPUBackend *GPUBackend::get(void)
{
  return g_backend;
}

/** \} */
