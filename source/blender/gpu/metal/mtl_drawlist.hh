/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Implementation of Multi Draw Indirect using OpenGL.
 * Fallback if the needed extensions are not supported.
 */

#pragma once

#pragma once

#include "gpu_drawlist_private.hh"

namespace blender {
namespace gpu {

/**
 * TODO(Metal): MTLDrawList Implementation. Included as temporary stub.
 */
class MTLDrawList : public DrawList {
 public:
  MTLDrawList(int length)
  {
  }
  ~MTLDrawList()
  {
  }

  void append(GPUBatch *batch, int i_first, int i_count) override
  {
  }
  void submit() override
  {
  }

  MEM_CXX_CLASS_ALLOC_FUNCS("MTLDrawList");
};

}  // namespace gpu
}  // namespace blender
