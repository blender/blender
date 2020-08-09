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

#include "BLI_assert.h"
#include "BLI_utildefines.h"

#include "GPU_framebuffer.h"

#include "GHOST_C-api.h"

#include "gpu_context_private.hh"

#include "gl_backend.hh" /* TODO remove */
#include "gl_context.hh"

using namespace blender;
using namespace blender::gpu;

/* -------------------------------------------------------------------- */
/** \name Constructor / Destructor
 * \{ */

GLContext::GLContext(void *ghost_window, GLSharedOrphanLists &shared_orphan_list)
    : shared_orphan_list_(shared_orphan_list)
{
  default_framebuffer_ = ghost_window ?
                             GHOST_GetDefaultOpenGLFramebuffer((GHOST_WindowHandle)ghost_window) :
                             0;

  glGenVertexArrays(1, &default_vao_);

  float data[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  glGenBuffers(1, &default_attr_vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, default_attr_vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

GLContext::~GLContext()
{
  BLI_assert(orphaned_framebuffers_.is_empty());
  BLI_assert(orphaned_vertarrays_.is_empty());
  /* For now don't allow GPUFrameBuffers to be reuse in another context. */
  BLI_assert(framebuffers_.is_empty());
  /* Delete vaos so the batch can be reused in another context. */
  for (GPUBatch *batch : batches_) {
    GPU_batch_vao_cache_clear(batch);
  }
  glDeleteVertexArrays(1, &default_vao_);
  glDeleteBuffers(1, &default_attr_vbo_);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Activate / Deactivate context
 * \{ */

void GLContext::activate(void)
{
  /* Make sure no other context is already bound to this thread. */
  BLI_assert(is_active_ == false);

  is_active_ = true;
  thread_ = pthread_self();

  /* Clear accumulated orphans. */
  orphans_clear();
}

void GLContext::deactivate(void)
{
  is_active_ = false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Safe object deletion
 *
 * GPU objects can be freed when the context is not bound.
 * In this case we delay the deletion until the context is bound again.
 * \{ */

void GLSharedOrphanLists::orphans_clear(void)
{
  /* Check if any context is active on this thread! */
  BLI_assert(GPU_context_active_get());

  lists_mutex.lock();
  if (!buffers.is_empty()) {
    glDeleteBuffers((uint)buffers.size(), buffers.data());
    buffers.clear();
  }
  if (!textures.is_empty()) {
    glDeleteTextures((uint)textures.size(), textures.data());
    textures.clear();
  }
  lists_mutex.unlock();
};

void GLContext::orphans_clear(void)
{
  /* Check if context has been activated by another thread! */
  BLI_assert(this->is_active_on_thread());

  lists_mutex_.lock();
  if (!orphaned_vertarrays_.is_empty()) {
    glDeleteVertexArrays((uint)orphaned_vertarrays_.size(), orphaned_vertarrays_.data());
    orphaned_vertarrays_.clear();
  }
  if (!orphaned_framebuffers_.is_empty()) {
    glDeleteFramebuffers((uint)orphaned_framebuffers_.size(), orphaned_framebuffers_.data());
    orphaned_framebuffers_.clear();
  }
  lists_mutex_.unlock();

  shared_orphan_list_.orphans_clear();
};

void GLContext::orphans_add(Vector<GLuint> &orphan_list, std::mutex &list_mutex, GLuint id)
{
  list_mutex.lock();
  orphan_list.append(id);
  list_mutex.unlock();
}

void GLContext::vao_free(GLuint vao_id)
{
  if (this == GPU_context_active_get()) {
    glDeleteVertexArrays(1, &vao_id);
  }
  else {
    orphans_add(orphaned_vertarrays_, lists_mutex_, vao_id);
  }
}

void GLContext::fbo_free(GLuint fbo_id)
{
  if (this == GPU_context_active_get()) {
    glDeleteFramebuffers(1, &fbo_id);
  }
  else {
    orphans_add(orphaned_framebuffers_, lists_mutex_, fbo_id);
  }
}

void GLBackend::buf_free(GLuint buf_id)
{
  /* Any context can free. */
  if (GPU_context_active_get()) {
    glDeleteBuffers(1, &buf_id);
  }
  else {
    orphans_add(shared_orphan_list_.buffers, shared_orphan_list_.lists_mutex, buf_id);
  }
}

void GLBackend::tex_free(GLuint tex_id)
{
  /* Any context can free. */
  if (GPU_context_active_get()) {
    glDeleteTextures(1, &tex_id);
  }
  else {
    orphans_add(shared_orphan_list_.textures, shared_orphan_list_.lists_mutex, tex_id);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Linked object deletion
 *
 * These objects contain data that are stored per context. We
 * need to do some cleanup if they are used accross context or if context
 * is discarded.
 * \{ */

void GLContext::batch_register(struct GPUBatch *batch)
{
  lists_mutex_.lock();
  batches_.add(batch);
  lists_mutex_.unlock();
}

void GLContext::batch_unregister(struct GPUBatch *batch)
{
  /* vao_cache_clear() can acquire lists_mutex_ so avoid deadlock. */
  // reinterpret_cast<GLBatch *>(batch)->vao_cache_clear();

  lists_mutex_.lock();
  batches_.remove(batch);
  lists_mutex_.unlock();
}

void GLContext::framebuffer_register(struct GPUFrameBuffer *fb)
{
#ifdef DEBUG
  lists_mutex_.lock();
  framebuffers_.add(fb);
  lists_mutex_.unlock();
#else
  UNUSED_VARS(fb);
#endif
}

void GLContext::framebuffer_unregister(struct GPUFrameBuffer *fb)
{
#ifdef DEBUG
  lists_mutex_.lock();
  framebuffers_.remove(fb);
  lists_mutex_.unlock();
#else
  UNUSED_VARS(fb);
#endif
}

/** \} */
