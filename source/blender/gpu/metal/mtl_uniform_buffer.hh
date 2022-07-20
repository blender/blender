/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "MEM_guardedalloc.h"
#include "gpu_uniform_buffer_private.hh"

#include "mtl_context.hh"

namespace blender::gpu {

/**
 * Implementation of Uniform Buffers using Metal.
 **/
class MTLUniformBuf : public UniformBuf {
 private:
  /* Allocation Handle. */
  gpu::MTLBuffer *metal_buffer_ = nullptr;

  /* Whether buffer has contents, if false, no GPU buffer will
   * have yet been allocated. */
  bool has_data_ = false;

  /* Bindstate tracking. */
  int bind_slot_ = -1;
  MTLContext *bound_ctx_ = nullptr;

 public:
  MTLUniformBuf(size_t size, const char *name);
  ~MTLUniformBuf();

  void update(const void *data) override;
  void bind(int slot) override;
  void unbind() override;

  id<MTLBuffer> get_metal_buffer(int *r_offset);
  int get_size();
  const char *get_name()
  {
    return name_;
  }

  MEM_CXX_CLASS_ALLOC_FUNCS("MTLUniformBuf");
};

}  // namespace blender::gpu
