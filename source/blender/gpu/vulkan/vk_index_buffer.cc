/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_index_buffer.hh"

namespace blender::gpu {

void VKIndexBuffer::upload_data()
{
}

void VKIndexBuffer::bind_as_ssbo(uint /*binding*/)
{
}

const uint32_t *VKIndexBuffer::read() const
{
  return 0;
}

void VKIndexBuffer::update_sub(uint /*start*/, uint /*len*/, const void * /*data*/)
{
}

void VKIndexBuffer::strip_restart_indices()
{
}

}  // namespace blender::gpu