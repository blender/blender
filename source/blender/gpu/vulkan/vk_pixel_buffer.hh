/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_texture_private.hh"

#include "vk_buffer.hh"

namespace blender::gpu {

class VKPixelBuffer : public PixelBuffer {
  VKBuffer buffer_;
  bool buffer_initialized_ = false;
  bool buffer_memory_export_ = false;

 public:
  VKPixelBuffer(size_t size);
  void *map() override;
  void unmap() override;
  GPUPixelBufferNativeHandle get_native_handle() override;
  size_t get_size() override;

  VKBuffer &buffer_get()
  {
    return buffer_;
  }

 protected:
  void create(bool memory_export);
};

static inline VKPixelBuffer *unwrap(PixelBuffer *pixel_buffer)
{
  return static_cast<VKPixelBuffer *>(pixel_buffer);
}

}  // namespace blender::gpu
