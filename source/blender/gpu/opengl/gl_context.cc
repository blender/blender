/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_assert.h"
#include "BLI_utildefines.h"

#include "BKE_global.hh"

#include "GPU_framebuffer.h"

#include "GHOST_C-api.h"

#include "gpu_context_private.hh"
#include "gpu_immediate_private.hh"

#include "gl_debug.hh"
#include "gl_immediate.hh"
#include "gl_state.hh"
#include "gl_uniform_buffer.hh"

#include "gl_backend.hh" /* TODO: remove. */
#include "gl_context.hh"

using namespace blender;
using namespace blender::gpu;

/* -------------------------------------------------------------------- */
/** \name Constructor / Destructor
 * \{ */

GLContext::GLContext(void *ghost_window, GLSharedOrphanLists &shared_orphan_list)
    : shared_orphan_list_(shared_orphan_list)
{
  if (G.debug & G_DEBUG_GPU) {
    debug::init_gl_callbacks();
  }

  float data[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  glGenBuffers(1, &default_attr_vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, default_attr_vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  state_manager = new GLStateManager();
  imm = new GLImmediate();
  ghost_window_ = ghost_window;

  if (ghost_window) {
    GLuint default_fbo = GHOST_GetDefaultGPUFramebuffer((GHOST_WindowHandle)ghost_window);
    GHOST_RectangleHandle bounds = GHOST_GetClientBounds((GHOST_WindowHandle)ghost_window);
    int w = GHOST_GetWidthRectangle(bounds);
    int h = GHOST_GetHeightRectangle(bounds);
    GHOST_DisposeRectangle(bounds);

    if (default_fbo != 0) {
      /* Bind default framebuffer, otherwise state might be undefined because of
       * detect_mip_render_workaround(). */
      glBindFramebuffer(GL_FRAMEBUFFER, default_fbo);
      front_left = new GLFrameBuffer("front_left", this, GL_COLOR_ATTACHMENT0, default_fbo, w, h);
      back_left = new GLFrameBuffer("back_left", this, GL_COLOR_ATTACHMENT0, default_fbo, w, h);
    }
    else {
      front_left = new GLFrameBuffer("front_left", this, GL_FRONT_LEFT, 0, w, h);
      back_left = new GLFrameBuffer("back_left", this, GL_BACK_LEFT, 0, w, h);
    }

    GLboolean supports_stereo_quad_buffer = GL_FALSE;
    glGetBooleanv(GL_STEREO, &supports_stereo_quad_buffer);
    if (supports_stereo_quad_buffer) {
      front_right = new GLFrameBuffer("front_right", this, GL_FRONT_RIGHT, 0, w, h);
      back_right = new GLFrameBuffer("back_right", this, GL_BACK_RIGHT, 0, w, h);
    }
  }
  else {
    /* For off-screen contexts. Default frame-buffer is null. */
    back_left = new GLFrameBuffer("back_left", this, GL_NONE, 0, 0, 0);
  }

  active_fb = back_left;
  static_cast<GLStateManager *>(state_manager)->active_fb = static_cast<GLFrameBuffer *>(
      active_fb);
}

GLContext::~GLContext()
{
  BLI_assert(orphaned_framebuffers_.is_empty());
  BLI_assert(orphaned_vertarrays_.is_empty());
  /* For now don't allow GPUFrameBuffers to be reuse in another context. */
  BLI_assert(framebuffers_.is_empty());
  /* Delete VAO's so the batch can be reused in another context. */
  for (GLVaoCache *cache : vao_caches_) {
    cache->clear();
  }
  glDeleteBuffers(1, &default_attr_vbo_);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Activate / Deactivate context
 * \{ */

void GLContext::activate()
{
  /* Make sure no other context is already bound to this thread. */
  BLI_assert(is_active_ == false);

  is_active_ = true;
  thread_ = pthread_self();

  /* Clear accumulated orphans. */
  orphans_clear();

  if (ghost_window_) {
    /* Get the correct framebuffer size for the internal framebuffers. */
    GHOST_RectangleHandle bounds = GHOST_GetClientBounds((GHOST_WindowHandle)ghost_window_);
    int w = GHOST_GetWidthRectangle(bounds);
    int h = GHOST_GetHeightRectangle(bounds);
    GHOST_DisposeRectangle(bounds);

    if (front_left) {
      front_left->size_set(w, h);
    }
    if (back_left) {
      back_left->size_set(w, h);
    }
    if (front_right) {
      front_right->size_set(w, h);
    }
    if (back_right) {
      back_right->size_set(w, h);
    }
  }

  /* Not really following the state but we should consider
   * no ubo bound when activating a context. */
  bound_ubo_slots = 0;

  immActivate();
}

void GLContext::deactivate()
{
  immDeactivate();
  is_active_ = false;
}

void GLContext::begin_frame()
{
  /* No-op. */
}

void GLContext::end_frame()
{
  /* No-op. */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Flush, Finish & sync
 * \{ */

void GLContext::flush()
{
  glFlush();
}

void GLContext::finish()
{
  glFinish();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Safe object deletion
 *
 * GPU objects can be freed when the context is not bound.
 * In this case we delay the deletion until the context is bound again.
 * \{ */

void GLSharedOrphanLists::orphans_clear()
{
  /* Check if any context is active on this thread! */
  BLI_assert(GLContext::get());

  lists_mutex.lock();
  if (!buffers.is_empty()) {
    glDeleteBuffers(uint(buffers.size()), buffers.data());
    buffers.clear();
  }
  if (!textures.is_empty()) {
    glDeleteTextures(uint(textures.size()), textures.data());
    textures.clear();
  }
  lists_mutex.unlock();
};

void GLContext::orphans_clear()
{
  /* Check if context has been activated by another thread! */
  BLI_assert(this->is_active_on_thread());

  lists_mutex_.lock();
  if (!orphaned_vertarrays_.is_empty()) {
    glDeleteVertexArrays(uint(orphaned_vertarrays_.size()), orphaned_vertarrays_.data());
    orphaned_vertarrays_.clear();
  }
  if (!orphaned_framebuffers_.is_empty()) {
    glDeleteFramebuffers(uint(orphaned_framebuffers_.size()), orphaned_framebuffers_.data());
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
  if (this == GLContext::get()) {
    glDeleteVertexArrays(1, &vao_id);
  }
  else {
    orphans_add(orphaned_vertarrays_, lists_mutex_, vao_id);
  }
}

void GLContext::fbo_free(GLuint fbo_id)
{
  if (this == GLContext::get()) {
    glDeleteFramebuffers(1, &fbo_id);
  }
  else {
    orphans_add(orphaned_framebuffers_, lists_mutex_, fbo_id);
  }
}

void GLContext::buf_free(GLuint buf_id)
{
  /* Any context can free. */
  if (GLContext::get()) {
    glDeleteBuffers(1, &buf_id);
  }
  else {
    GLSharedOrphanLists &orphan_list = GLBackend::get()->shared_orphan_list_get();
    orphans_add(orphan_list.buffers, orphan_list.lists_mutex, buf_id);
  }
}

void GLContext::tex_free(GLuint tex_id)
{
  /* Any context can free. */
  if (GLContext::get()) {
    glDeleteTextures(1, &tex_id);
  }
  else {
    GLSharedOrphanLists &orphan_list = GLBackend::get()->shared_orphan_list_get();
    orphans_add(orphan_list.textures, orphan_list.lists_mutex, tex_id);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Linked object deletion
 *
 * These objects contain data that are stored per context. We
 * need to do some cleanup if they are used across context or if context
 * is discarded.
 * \{ */

void GLContext::vao_cache_register(GLVaoCache *cache)
{
  lists_mutex_.lock();
  vao_caches_.add(cache);
  lists_mutex_.unlock();
}

void GLContext::vao_cache_unregister(GLVaoCache *cache)
{
  lists_mutex_.lock();
  vao_caches_.remove(cache);
  lists_mutex_.unlock();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Memory statistics
 * \{ */

void GLContext::memory_statistics_get(int *r_total_mem, int *r_free_mem)
{
  if (epoxy_has_gl_extension("GL_NVX_gpu_memory_info")) {
    /* Returned value in Kb. */
    glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, r_total_mem);
    glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, r_free_mem);
  }
  else if (epoxy_has_gl_extension("GL_ATI_meminfo")) {
    int stats[4];
    glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, stats);

    *r_total_mem = 0;
    *r_free_mem = stats[0]; /* Total memory free in the pool. */
  }
  else {
    *r_total_mem = 0;
    *r_free_mem = 0;
  }
}

/** \} */
