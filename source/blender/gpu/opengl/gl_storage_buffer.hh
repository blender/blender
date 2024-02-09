/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "gpu_storage_buffer_private.hh"

namespace blender {
namespace gpu {

/**
 * Implementation of Storage Buffers using OpenGL.
 */
class GLStorageBuf : public StorageBuf {
 private:
  /** Slot to which this UBO is currently bound. -1 if not bound. */
  int slot_ = -1;
  /** OpenGL Object handle. */
  GLuint ssbo_id_ = 0;
  /** Usage type. */
  GPUUsageType usage_;
  /* Read */
  GLuint read_ssbo_id_ = 0;
  GLsync read_fence_ = 0;
  void *persistent_ptr_ = nullptr;

 public:
  GLStorageBuf(size_t size, GPUUsageType usage, const char *name);
  ~GLStorageBuf();

  void update(const void *data) override;
  void bind(int slot) override;
  void unbind() override;
  void clear(uint32_t clear_value) override;
  void copy_sub(VertBuf *src, uint dst_offset, uint src_offset, uint copy_size) override;
  void read(void *data) override;
  void async_flush_to_host() override;
  void sync_as_indirect_buffer() override;

  /* Special internal function to bind SSBOs to indirect argument targets. */
  void bind_as(GLenum target);

 private:
  void init();

  MEM_CXX_CLASS_ALLOC_FUNCS("GLStorageBuf");
};

}  // namespace gpu
}  // namespace blender
