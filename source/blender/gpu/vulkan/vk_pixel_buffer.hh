/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_texture_private.hh"

#include "vk_buffer.hh"

namespace blender::gpu {

class VKPixelBuffer : public PixelBuffer {
  VKBuffer buffer_;

 public:
  VKPixelBuffer(int64_t size);
  void *map() override;
  void unmap() override;
  int64_t get_native_handle() override;
  uint get_size() override;
};

}  // namespace blender::gpu
