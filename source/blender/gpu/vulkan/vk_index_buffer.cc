/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_index_buffer.hh"
#include "vk_shader.hh"
#include "vk_shader_interface.hh"

namespace blender::gpu {

void VKIndexBuffer::upload_data()
{
}

void VKIndexBuffer::bind_as_ssbo(uint binding)
{
  VKContext &context = *VKContext::get();
  if (!buffer_.is_allocated()) {
    allocate(context);
  }

  VKShader *shader = static_cast<VKShader *>(context.shader);
  const VKShaderInterface &shader_interface = shader->interface_get();
  const ShaderInput *shader_input = shader_interface.shader_input_get(
      shader::ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER, binding);
  shader->pipeline_get().descriptor_set_get().bind_as_ssbo(*this, shader_input);
}

void VKIndexBuffer::read(uint32_t *data) const
{
  VKContext &context = *VKContext::get();
  VKCommandBuffer &command_buffer = context.command_buffer_get();
  command_buffer.submit();

  void *mapped_memory;
  if (buffer_.map(context, &mapped_memory)) {
    memcpy(data, mapped_memory, size_get());
    buffer_.unmap(context);
  }
}

void VKIndexBuffer::update_sub(uint /*start*/, uint /*len*/, const void * /*data*/)
{
}

void VKIndexBuffer::strip_restart_indices()
{
}

void VKIndexBuffer::allocate(VKContext &context)
{
  GPUUsageType usage = data_ == nullptr ? GPU_USAGE_DEVICE_ONLY : GPU_USAGE_STATIC;
  buffer_.create(context,
                 size_get(),
                 usage,
                 static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT));
}

}  // namespace blender::gpu
