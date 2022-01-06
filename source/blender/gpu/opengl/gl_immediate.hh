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
 * Mimics old style opengl immediate mode drawing.
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "glew-mx.h"

#include "gpu_immediate_private.hh"

namespace blender::gpu {

/* size of internal buffer */
#define DEFAULT_INTERNAL_BUFFER_SIZE (4 * 1024 * 1024)

class GLImmediate : public Immediate {
 private:
  /* Use two buffers for strict and non-strict vertex count to
   * avoid some huge driver slowdown (see T70922).
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
