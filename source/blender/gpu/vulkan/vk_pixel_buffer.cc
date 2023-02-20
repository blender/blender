/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_pixel_buffer.hh"

namespace blender::gpu {

VKPixelBuffer::VKPixelBuffer(int64_t size) : PixelBuffer(size)
{
}

void *VKPixelBuffer::map()
{
  return nullptr;
}

void VKPixelBuffer::unmap()
{
}

int64_t VKPixelBuffer::get_native_handle()
{
  return -1;
}

uint VKPixelBuffer::get_size()
{
  return size_;
}

}  // namespace blender::gpu
