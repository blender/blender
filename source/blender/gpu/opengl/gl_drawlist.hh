/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Implementation of Multi Draw Indirect using OpenGL.
 * Fallback if the needed extensions are not supported.
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "BLI_sys_types.h"

#include "GPU_batch.hh"

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
  void submit() override;

 private:
  void init();

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
  /** Offset of `data_` inside the whole buffer (in byte). */
  GLintptr data_offset_;

  /** To free the buffer_id_. */
  GLContext *context_;

  MEM_CXX_CLASS_ALLOC_FUNCS("GLDrawList");
};

}  // namespace gpu
}  // namespace blender
