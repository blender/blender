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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "gpu_index_buffer_private.hh"

#include "glew-mx.h"

namespace blender::gpu {

class GLIndexBuf : public IndexBuf {
  friend class GLBatch;
  friend class GLDrawList;
  friend class GLShader; /* For compute shaders. */

 private:
  GLuint ibo_id_ = 0;

 public:
  ~GLIndexBuf();

  void bind(void);
  void bind_as_ssbo(uint binding) override;

  const uint32_t *read() const override;

  void *offset_ptr(uint additional_vertex_offset) const
  {
    additional_vertex_offset += index_start_;
    if (index_type_ == GPU_INDEX_U32) {
      return (GLuint *)0 + additional_vertex_offset;
    }
    return (GLushort *)0 + additional_vertex_offset;
  }

  GLuint restart_index(void) const
  {
    return (index_type_ == GPU_INDEX_U16) ? 0xFFFFu : 0xFFFFFFFFu;
  }

  void upload_data(void) override;

  void update_sub(uint start, uint len, const void *data) override;

 private:
  bool is_active() const;

  MEM_CXX_CLASS_ALLOC_FUNCS("GLIndexBuf")
};

static inline GLenum to_gl(GPUIndexBufType type)
{
  return (type == GPU_INDEX_U32) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
}

}  // namespace blender::gpu
