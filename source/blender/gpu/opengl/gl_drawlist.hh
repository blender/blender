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
 *
 * Implementation of Multi Draw Indirect using OpenGL.
 * Fallback if the needed extensions are not supported.
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "BLI_sys_types.h"

#include "GPU_batch.h"

#include "gpu_drawlist_private.hh"

#include "gl_context.hh"

namespace blender {
namespace gpu {

/**
 * Implementation of Multi Draw Indirect using OpenGL.
 */
class GLDrawList : public DrawList {
 public:
  GLDrawList(int length);
  ~GLDrawList();

  void append(GPUBatch *batch, int i_first, int i_count) override;
  void submit(void) override;

 private:
  void init(void);

  /** Batch for which we are recording commands for. */
  GLBatch *batch_;
  /** Mapped memory bounds. */
  GLbyte *data_;
  /** Length of the mapped buffer (in byte). */
  GLsizeiptr data_size_;
  /** Current offset inside the mapped buffer (in byte). */
  GLintptr command_offset_;
  /** Current number of command recorded inside the mapped buffer. */
  uint command_len_;
  /** Is UINT_MAX if not drawing indexed geom. Also Avoid dereferencing batch. */
  GLuint base_index_;
  /** Also Avoid dereferencing batch. */
  GLuint v_first_, v_count_;

  /** GL Indirect Buffer id. 0 means MultiDrawIndirect is not supported/enabled. */
  GLuint buffer_id_;
  /** Length of whole the buffer (in byte). */
  GLsizeiptr buffer_size_;
  /** Offset of data_ inside the whole buffer (in byte). */
  GLintptr data_offset_;

  /** To free the buffer_id_. */
  GLContext *context_;

  MEM_CXX_CLASS_ALLOC_FUNCS("GLDrawList");
};

}  // namespace gpu
}  // namespace blender
