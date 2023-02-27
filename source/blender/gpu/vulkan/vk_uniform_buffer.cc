/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_uniform_buffer.hh"
#include "vk_context.hh"

namespace blender::gpu {

void VKUniformBuffer::update(const void *data)
{
  VKContext &context = *VKContext::get();
  if (!buffer_.is_allocated()) {
    allocate(context);
  }
  buffer_.update(context, data);
}

void VKUniformBuffer::allocate(VKContext &context)
{
  buffer_.create(context, size_in_bytes_, GPU_USAGE_STATIC, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
}

void VKUniformBuffer::clear_to_zero()
{
}

void VKUniformBuffer::bind(int /*slot*/)
{
}

void VKUniformBuffer::bind_as_ssbo(int /*slot*/)
{
}

void VKUniformBuffer::unbind()
{
}

}  // namespace blender::gpu
