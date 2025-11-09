/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "gpu_uniform_buffer_private.hh"

#include "mtl_context.hh"

namespace blender::gpu {

class MTLStorageBuf;

/**
 * Implementation of Uniform Buffers using Metal.
 */
class MTLUniformBuf : public UniformBuf {
  friend class MTLStorageBuf; /* For bind as SSBO resource access. */

 private:
  /* Allocation Handle. */
  gpu::MTLBuffer *metal_buffer_ = nullptr;

  /* Bind-state tracking. */
  int bind_slot_ = -1;
  MTLContext *bound_ctx_ = nullptr;

  /* SSBO wrapper for bind_as_ssbo support. */
  MTLStorageBuf *ssbo_wrapper_ = nullptr;

 public:
  MTLUniformBuf(size_t size, const char *name);
  ~MTLUniformBuf() override;

  void update(const void *data) override;
  void bind(int slot) override;
  void bind_as_ssbo(int slot) override;
  void unbind() override;
  void clear_to_zero() override;

  id<MTLBuffer> get_metal_buffer();
  size_t get_size();
  const char *get_name()
  {
    return name_;
  }

  MEM_CXX_CLASS_ALLOC_FUNCS("MTLUniformBuf");
};

}  // namespace blender::gpu
