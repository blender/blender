/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "GPU_texture.h"

#include "gpu_storage_buffer_private.hh"
#include "gpu_vertex_buffer_private.hh"

#include "vk_buffer.hh"

namespace blender::gpu {
class VertBuf;

class VKStorageBuffer : public StorageBuf {
  GPUUsageType usage_;
  VKBuffer buffer_;

 public:
  VKStorageBuffer(int size, GPUUsageType usage, const char *name)
      : StorageBuf(size, name), usage_(usage)
  {
  }

  void update(const void *data) override;
  void bind(int slot) override;
  void unbind() override;
  void clear(uint32_t clear_value) override;
  void copy_sub(VertBuf *src, uint dst_offset, uint src_offset, uint copy_size) override;
  void read(void *data) override;

  VkBuffer vk_handle() const
  {
    return buffer_.vk_handle();
  }

  int64_t size_in_bytes() const
  {
    return buffer_.size_in_bytes();
  }

 private:
  void allocate();
};

}  // namespace blender::gpu
