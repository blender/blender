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
 *   this VAO.
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

static thread_local Context *active_ctx = nullptr;

/* -------------------------------------------------------------------- */
/** \name gpu::Context methods
 * \{ */

namespace blender::gpu {

Context::Context()
{
  thread_ = pthread_self();
  is_active_ = false;
  matrix_state = GPU_matrix_state_create();
}

Context::~Context()
{
  GPU_matrix_state_discard(matrix_state);
  delete state_manager;
  delete front_left;
  delete back_left;
  delete front_right;
  delete back_right;
  delete imm;
}

bool Context::is_active_on_thread()
{
  return (this == active_ctx) && pthread_equal(pthread_self(), thread_);
}

Context *Context::get()
{
  return active_ctx;
}

}  // namespace blender::gpu

/** \} */

/* -------------------------------------------------------------------- */

GPUContext *GPU_context_create(void *ghost_window)
{
  if (GPUBackend::get() == nullptr) {
    /* TODO move where it make sense. */
    GPU_backend_init(GPU_BACKEND_OPENGL);
  }

  Context *ctx = GPUBackend::get()->context_alloc(ghost_window);

  GPU_context_active_set(wrap(ctx));
  return wrap(ctx);
}

/* to be called after GPU_context_active_set(ctx_to_destroy) */
void GPU_context_discard(GPUContext *ctx_)
{
  Context *ctx = unwrap(ctx_);
  delete ctx;
  active_ctx = nullptr;
}

/* ctx can be NULL */
void GPU_context_active_set(GPUContext *ctx_)
{
  Context *ctx = unwrap(ctx_);

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
  return wrap(Context::get());
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
  BLI_assert(g_backend == nullptr);

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
  g_backend = nullptr;
}

GPUBackend *GPUBackend::get()
{
  return g_backend;
}

/** \} */
