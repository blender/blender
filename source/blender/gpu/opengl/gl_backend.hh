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
#include "gl_context.hh"
#include "gl_drawlist.hh"
#include "gl_shader.hh"

namespace blender {
namespace gpu {

class GLBackend : public GPUBackend {
 private:
  GLSharedOrphanLists shared_orphan_list_;

 public:
  GPUContext *context_alloc(void *ghost_window)
  {
    return new GLContext(ghost_window, shared_orphan_list_);
  };

  Batch *batch_alloc(void)
  {
    return new GLBatch();
  };

  DrawList *drawlist_alloc(int list_length)
  {
    return new GLDrawList(list_length);
  };

  Shader *shader_alloc(const char *name)
  {
    return new GLShader(name);
  };

  /* TODO remove */
  void buf_free(GLuint buf_id);
  void tex_free(GLuint tex_id);
  void orphans_add(Vector<GLuint> &orphan_list, std::mutex &list_mutex, unsigned int id)
  {
    list_mutex.lock();
    orphan_list.append(id);
    list_mutex.unlock();
  }
};

}  // namespace gpu
}  // namespace blender
