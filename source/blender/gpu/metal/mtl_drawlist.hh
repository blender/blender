/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Implementation of Multi Draw Indirect using OpenGL.
 * Fallback if the needed extensions are not supported.
 */

#pragma once

#include "BLI_sys_types.h"
#include "GPU_batch.h"
#include "MEM_guardedalloc.h"
#include "gpu_drawlist_private.hh"

#include "mtl_batch.hh"
#include "mtl_context.hh"

namespace blender::gpu {

/**
 * Implementation of Multi Draw Indirect using OpenGL.
 **/
class MTLDrawList : public DrawList {

 private:
  /** Batch for which we are recording commands for. */
  MTLBatch *batch_;
  /** Mapped memory bounds. */
  void *data_;
  /** Length of the mapped buffer (in byte). */
  size_t data_size_;
  /** Current offset inside the mapped buffer (in byte). */
  size_t command_offset_;
  /** Current number of command recorded inside the mapped buffer. */
  uint32_t command_len_;
  /** Is UINT_MAX if not drawing indexed geom. Also Avoid dereferencing batch. */
  uint32_t base_index_;
  /** Also Avoid dereferencing batch. */
  uint32_t v_first_, v_count_;
  /** Length of whole the buffer (in byte). */
  uint32_t buffer_size_;

 public:
  MTLDrawList(int length);
  ~MTLDrawList();

  void append(GPUBatch *batch, int i_first, int i_count) override;
  void submit() override;

 private:
  void init();

  MEM_CXX_CLASS_ALLOC_FUNCS("MTLDrawList");
};

}  // namespace blender::gpu
