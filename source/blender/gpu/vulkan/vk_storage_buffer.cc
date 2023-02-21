/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */
#include "vk_shader.hh"
#include "vk_shader_interface.hh"
#include "vk_vertex_buffer.hh"

#include "vk_storage_buffer.hh"

namespace blender::gpu {

void VKStorageBuffer::update(const void *data)
{
  VKContext &context = *VKContext::get();
  if (!buffer_.is_allocated()) {
    allocate(context);
  }
  buffer_.update(context, data);
}

void VKStorageBuffer::allocate(VKContext &context)
{
  buffer_.create(context, size_in_bytes_, usage_, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}

void VKStorageBuffer::bind(int slot)
{
  VKContext &context = *VKContext::get();
  if (!buffer_.is_allocated()) {
    allocate(context);
  }
  VKShader *shader = static_cast<VKShader *>(context.shader);
  const VKShaderInterface &shader_interface = shader->interface_get();
  const ShaderInput *shader_input = shader_interface.shader_input_get(
      shader::ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER, slot);
  shader->pipeline_get().descriptor_set_get().bind(*this, shader_input);
}

void VKStorageBuffer::unbind()
{
}

void VKStorageBuffer::clear(eGPUTextureFormat /*internal_format*/,
                            eGPUDataFormat /*data_format*/,
                            void * /*data*/)
{
}
void VKStorageBuffer::copy_sub(VertBuf * /*src*/,
                               uint /*dst_offset*/,
                               uint /*src_offset*/,
                               uint /*copy_size*/)
{
}

void VKStorageBuffer::read(void *data)
{
  VKContext &context = *VKContext::get();
  if (!buffer_.is_allocated()) {
    allocate(context);
  }

  VKCommandBuffer &command_buffer = context.command_buffer_get();
  command_buffer.submit();

  void *mapped_memory;
  if (buffer_.map(context, &mapped_memory)) {
    memcpy(data, mapped_memory, size_in_bytes_);
    buffer_.unmap(context);
  }
}

}  // namespace blender::gpu
