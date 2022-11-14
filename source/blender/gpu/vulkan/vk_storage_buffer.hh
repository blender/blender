/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "GPU_texture.h"

#include "gpu_storage_buffer_private.hh"

namespace blender::gpu {

class VKStorageBuffer : public StorageBuf {
 public:
  VKStorageBuffer(int size, const char *name) : StorageBuf(size, name)
  {
  }

  void update(const void *data) override;
  void bind(int slot) override;
  void unbind() override;
  void clear(eGPUTextureFormat internal_format, eGPUDataFormat data_format, void *data) override;
  void copy_sub(VertBuf *src, uint dst_offset, uint src_offset, uint copy_size) override;
  void read(void *data) override;
};

}  // namespace blender::gpu