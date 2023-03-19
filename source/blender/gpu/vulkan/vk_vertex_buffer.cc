/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "MEM_guardedalloc.h"

#include "vk_shader.hh"
#include "vk_shader_interface.hh"
#include "vk_vertex_buffer.hh"

namespace blender::gpu {

VKVertexBuffer::~VKVertexBuffer()
{
  release_data();
}

void VKVertexBuffer::bind_as_ssbo(uint binding)
{
  VKContext &context = *VKContext::get();
  if (!buffer_.is_allocated()) {
    allocate(context);
  }

  VKShader *shader = static_cast<VKShader *>(context.shader);
  const VKShaderInterface &shader_interface = shader->interface_get();
  const VKDescriptorSet::Location location = shader_interface.descriptor_set_location(
      shader::ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER, binding);
  shader->pipeline_get().descriptor_set_get().bind_as_ssbo(*this, location);
}

void VKVertexBuffer::bind_as_texture(uint /*binding*/)
{
}

void VKVertexBuffer::wrap_handle(uint64_t /*handle*/)
{
}

void VKVertexBuffer::update_sub(uint /*start*/, uint /*len*/, const void * /*data*/)
{
}

void VKVertexBuffer::read(void *data) const
{
  VKContext &context = *VKContext::get();
  VKCommandBuffer &command_buffer = context.command_buffer_get();
  command_buffer.submit();
  buffer_.read(data);
}

void VKVertexBuffer::acquire_data()
{
  if (usage_ == GPU_USAGE_DEVICE_ONLY) {
    return;
  }

  /* Discard previous data if any. */
  MEM_SAFE_FREE(data);
  data = (uchar *)MEM_mallocN(sizeof(uchar) * this->size_alloc_get(), __func__);
}

void VKVertexBuffer::resize_data()
{
}

void VKVertexBuffer::release_data()
{
  MEM_SAFE_FREE(data);
}

void VKVertexBuffer::upload_data()
{
}

void VKVertexBuffer::duplicate_data(VertBuf * /*dst*/)
{
}

void VKVertexBuffer::allocate(VKContext &context)
{
  buffer_.create(context,
                 size_used_get(),
                 usage_,
                 static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
}

}  // namespace blender::gpu
