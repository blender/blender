/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "gpu_index_buffer_private.hh"

#include <epoxy/gl.h>

namespace blender::gpu {

class GLIndexBuf : public IndexBuf {
  friend class GLBatch;
  friend class GLDrawList;
  friend class GLShader; /* For compute shaders. */

 private:
  GLuint ibo_id_ = 0;

 public:
  ~GLIndexBuf();

  void bind();
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

  GLuint restart_index() const
  {
    return (index_type_ == GPU_INDEX_U16) ? 0xFFFFu : 0xFFFFFFFFu;
  }

  void upload_data() override;

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
