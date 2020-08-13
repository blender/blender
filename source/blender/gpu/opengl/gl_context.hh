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

#include "gpu_context_private.hh"

#include "GPU_framebuffer.h"

#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "glew-mx.h"

#include "gl_batch.hh"

#include <mutex>

namespace blender {
namespace gpu {

class GLSharedOrphanLists {
 public:
  /** Mutex for the bellow structures. */
  std::mutex lists_mutex;
  /** Buffers and textures are shared across context. Any context can free them. */
  Vector<GLuint> textures;
  Vector<GLuint> buffers;

 public:
  void orphans_clear(void);
};

struct GLContext : public GPUContext {
  /* TODO(fclem) these needs to become private. */
 public:
  /** Default VAO for procedural draw calls. */
  GLuint default_vao_;
  /** Default framebuffer object for some GL implementation. */
  GLuint default_framebuffer_;
  /** VBO for missing vertex attrib binding. Avoid undefined behavior on some implementation. */
  GLuint default_attr_vbo_;
  /**
   * GPUBatch & GPUFramebuffer have references to the context they are from, in the case the
   * context is destroyed, we need to remove any reference to it.
   */
  Set<GLVaoCache *> vao_caches_;
  Set<GPUFrameBuffer *> framebuffers_;
  /** Mutex for the bellow structures. */
  std::mutex lists_mutex_;
  /** VertexArrays and framebuffers are not shared across context. */
  Vector<GLuint> orphaned_vertarrays_;
  Vector<GLuint> orphaned_framebuffers_;
  /** GLBackend onws this data. */
  GLSharedOrphanLists &shared_orphan_list_;

 public:
  GLContext(void *ghost_window, GLSharedOrphanLists &shared_orphan_list);
  ~GLContext();

  void activate(void) override;
  void deactivate(void) override;

  /* TODO(fclem) these needs to become private. */
 public:
  void orphans_add(Vector<GLuint> &orphan_list, std::mutex &list_mutex, GLuint id);
  void orphans_clear(void);

  void vao_free(GLuint vao_id);
  void fbo_free(GLuint fbo_id);
  void vao_cache_register(GLVaoCache *cache);
  void vao_cache_unregister(GLVaoCache *cache);
  void framebuffer_register(struct GPUFrameBuffer *fb);
  void framebuffer_unregister(struct GPUFrameBuffer *fb);
};

}  // namespace gpu
}  // namespace blender
