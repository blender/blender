/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "MEM_guardedalloc.h"
#include "gpu_storage_buffer_private.hh"

#include "mtl_context.hh"

namespace blender::gpu {

class MTLUniformBuf;
class MTLVertBuf;
class MTLIndexBuf;
class MTLCircularBuffer;

/**
 * Implementation of Storage Buffers using Metal.
 */
class MTLStorageBuf : public StorageBuf {
  friend MTLCircularBuffer;

 private:
  /** Allocation Handle or indirect wrapped instance.
   * MTLStorageBuf can wrap a MTLVertBuf, MTLIndexBuf or MTLUniformBuf for binding as a writeable
   * resource. */
  enum {
    MTL_STORAGE_BUF_TYPE_DEFAULT = 0,
    MTL_STORAGE_BUF_TYPE_UNIFORMBUF = 1,
    MTL_STORAGE_BUF_TYPE_VERTBUF = 2,
    MTL_STORAGE_BUF_TYPE_INDEXBUF = 3,
    MTL_STORAGE_BUF_TYPE_TEXTURE = 4,
  } storage_source_ = MTL_STORAGE_BUF_TYPE_DEFAULT;

  union {
    /** Own allocation. */
    gpu::MTLBuffer *metal_buffer_;
    /* Wrapped type. */
    MTLUniformBuf *uniform_buffer_;
    MTLVertBuf *vertex_buffer_;
    MTLIndexBuf *index_buffer_;
    gpu::MTLTexture *texture_;
  };

  /* Whether buffer has contents, if false, no GPU buffer will
   * have yet been allocated. */
  bool has_data_ = false;
  /** Bind-state tracking. */
  int bind_slot_ = -1;
  MTLContext *bound_ctx_ = nullptr;

  /** Usage type. */
  GPUUsageType usage_;

  /* Synchronization event for host reads. */
  id<MTLSharedEvent> gpu_write_fence_ = nil;
  uint64_t host_read_signal_value_ = 0;

 public:
  MTLStorageBuf(size_t size, GPUUsageType usage, const char *name);
  ~MTLStorageBuf() override;

  MTLStorageBuf(MTLUniformBuf *uniform_buf, size_t size);
  MTLStorageBuf(MTLVertBuf *vert_buf, size_t size);
  MTLStorageBuf(MTLIndexBuf *index_buf, size_t size);
  MTLStorageBuf(MTLTexture *texture, size_t size);

  /* Only used internally to create a bindable buffer for #Immediate. */
  MTLStorageBuf(size_t size);

  void update(const void *data) override;
  void bind(int slot) override;
  void unbind() override;
  void clear(uint32_t clear_value) override;
  void copy_sub(VertBuf *src, uint dst_offset, uint src_offset, uint copy_size) override;
  void read(void *data) override;
  void async_flush_to_host() override;
  void sync_as_indirect_buffer() override { /* No-Op. */ };

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

}  // namespace blender::gpu
