/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Mimics old style opengl immediate mode drawing.
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "gpu_immediate_private.hh"

namespace blender::gpu {

/* size of internal buffer */
#define DEFAULT_INTERNAL_BUFFER_SIZE (4 * 1024 * 1024)

class GLImmediate : public Immediate {
 private:
  /* Use two buffers for strict and non-strict vertex count to
   * avoid some huge driver slowdown (see #70922).
   * Use accessor functions to get / modify. */
  struct {
    /** Opengl Handle for this buffer. */
    GLuint vbo_id = 0;
    /** Offset of the mapped data in data. */
    size_t buffer_offset = 0;
    /** Size of the whole buffer in bytes. */
    size_t buffer_size = 0;
  } buffer, buffer_strict;
  /** Size in bytes of the mapped region. */
  size_t bytes_mapped_ = 0;
  /** Vertex array for this immediate mode instance. */
  GLuint vao_id_ = 0;

 public:
  GLImmediate();
  ~GLImmediate();

  uchar *begin() override;
  void end() override;

 private:
  GLuint &vbo_id()
  {
    return strict_vertex_len ? buffer_strict.vbo_id : buffer.vbo_id;
  };

  size_t &buffer_offset()
  {
    return strict_vertex_len ? buffer_strict.buffer_offset : buffer.buffer_offset;
  };

  size_t &buffer_size()
  {
    return strict_vertex_len ? buffer_strict.buffer_size : buffer.buffer_size;
  };
};

}  // namespace blender::gpu
