/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

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

 public:
  GLStorageBuf(size_t size, GPUUsageType usage, const char *name);
  ~GLStorageBuf();

  void update(const void *data) override;
  void bind(int slot) override;
  void unbind() override;
  void clear(eGPUTextureFormat internal_format, eGPUDataFormat data_format, void *data) override;
  void copy_sub(VertBuf *src, uint dst_offset, uint src_offset, uint copy_size) override;

  /* Special internal function to bind SSBOs to indirect argument targets. */
  void bind_as(GLenum target);

 private:
  void init();

  MEM_CXX_CLASS_ALLOC_FUNCS("GLStorageBuf");
};

}  // namespace gpu
}  // namespace blender
