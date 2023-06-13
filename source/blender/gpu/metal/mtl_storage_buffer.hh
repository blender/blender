/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "MEM_guardedalloc.h"
#include "gpu_storage_buffer_private.hh"

#include "mtl_context.hh"

namespace blender {
namespace gpu {

class MTLUniformBuf;
class MTLVertBuf;
class MTLIndexBuf;

/**
 * Implementation of Storage Buffers using Metal.
 */
class MTLStorageBuf : public StorageBuf {
 private:
  /** Allocation Handle or indirect wrapped instance.
   * MTLStorageBuf can wrap a MTLVertBuf, MTLIndexBuf or MTLUniformBuf for binding as a writeable
   * resource. */
  enum {
    MTL_STORAGE_BUF_TYPE_DEFAULT = 0,
    MTL_STORAGE_BUF_TYPE_UNIFORMBUF = 1,
    MTL_STORAGE_BUF_TYPE_VERTBUF = 2,
    MTL_STORAGE_BUF_TYPE_INDEXBUF = 3,
  } storage_source_ = MTL_STORAGE_BUF_TYPE_DEFAULT;

  union {
    /** Own allocation. */
    gpu::MTLBuffer *metal_buffer_;
    /* Wrapped type. */
    MTLUniformBuf *uniform_buffer_;
    MTLVertBuf *vertex_buffer_;
    MTLIndexBuf *index_buffer_;
  };

  /* Whether buffer has contents, if false, no GPU buffer will
   * have yet been allocated. */
  bool has_data_ = false;
  /** Bind-state tracking. */
  int bind_slot_ = -1;
  MTLContext *bound_ctx_ = nullptr;

  /** Usage type. */
  GPUUsageType usage_;

 public:
  MTLStorageBuf(size_t size, GPUUsageType usage, const char *name);
  ~MTLStorageBuf();

  MTLStorageBuf(MTLUniformBuf *uniform_buf, size_t size);
  MTLStorageBuf(MTLVertBuf *uniform_buf, size_t size);
  MTLStorageBuf(MTLIndexBuf *uniform_buf, size_t size);

  void update(const void *data) override;
  void bind(int slot) override;
  void unbind() override;
  void clear(uint32_t clear_value) override;
  void copy_sub(VertBuf *src, uint dst_offset, uint src_offset, uint copy_size) override;
  void read(void *data) override;

  void init();

  id<MTLBuffer> get_metal_buffer();
  size_t get_size();
  const char *get_name()
  {
    return name_;
  }

 private:
  MEM_CXX_CLASS_ALLOC_FUNCS("MTLStorageBuf");
};

}  // namespace gpu
}  // namespace blender
