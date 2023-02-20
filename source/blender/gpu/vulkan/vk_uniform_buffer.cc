/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_uniform_buffer.hh"

namespace blender::gpu {

void VKUniformBuffer::update(const void * /*data*/)
{
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
