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

#include "glew-mx.h"

#include "gpu_vertex_buffer_private.hh"

namespace blender {
namespace gpu {

class GLVertBuf : public VertBuf {
  friend class GLTexture; /* For buffer texture. */
  friend class GLShader;  /* For transform feedback. */

 private:
  /** OpenGL buffer handle. Init on first upload. Immutable after that. */
  GLuint vbo_id_ = 0;
  /** Defines whether the buffer handle is wrapped by this GLVertBuf, i.e. we do not own it and
   * should not free it. */
  bool is_wrapper_ = false;
  /** Size on the GPU. */
  size_t vbo_size_ = 0;

 public:
  void bind();

  void update_sub(uint start, uint len, const void *data) override;

  const void *read() const override;
  void *unmap(const void *mapped_data) const override;

  void wrap_handle(uint64_t handle) override;

 protected:
  void acquire_data() override;
  void resize_data() override;
  void release_data() override;
  void upload_data() override;
  void duplicate_data(VertBuf *dst) override;
  void bind_as_ssbo(uint binding) override;

 private:
  bool is_active() const;

  MEM_CXX_CLASS_ALLOC_FUNCS("GLVertBuf");
};

static inline GLenum to_gl(GPUUsageType type)
{
  switch (type) {
    case GPU_USAGE_STREAM:
      return GL_STREAM_DRAW;
    case GPU_USAGE_DYNAMIC:
      return GL_DYNAMIC_DRAW;
    case GPU_USAGE_STATIC:
    case GPU_USAGE_DEVICE_ONLY:
      return GL_STATIC_DRAW;
    default:
      BLI_assert(0);
      return GL_STATIC_DRAW;
  }
}

static inline GLenum to_gl(GPUVertCompType type)
{
  switch (type) {
    case GPU_COMP_I8:
      return GL_BYTE;
    case GPU_COMP_U8:
      return GL_UNSIGNED_BYTE;
    case GPU_COMP_I16:
      return GL_SHORT;
    case GPU_COMP_U16:
      return GL_UNSIGNED_SHORT;
    case GPU_COMP_I32:
      return GL_INT;
    case GPU_COMP_U32:
      return GL_UNSIGNED_INT;
    case GPU_COMP_F32:
      return GL_FLOAT;
    case GPU_COMP_I10:
      return GL_INT_2_10_10_10_REV;
    default:
      BLI_assert(0);
      return GL_FLOAT;
  }
}

}  // namespace gpu
}  // namespace blender
