/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "MEM_guardedalloc.h"

#include "vk_data_conversion.hh"
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
  if (!buffer_.is_allocated()) {
    allocate();
  }

  VKContext &context = *VKContext::get();
  VKShader *shader = static_cast<VKShader *>(context.shader);
  const VKShaderInterface &shader_interface = shader->interface_get();
  const std::optional<VKDescriptorSet::Location> location =
      shader_interface.descriptor_set_location(
          shader::ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER, binding);
  BLI_assert_msg(location, "Locations to SSBOs should always exist.");
  shader->pipeline_get().descriptor_set_get().bind_as_ssbo(*this, *location);
}

void VKVertexBuffer::bind_as_texture(uint /*binding*/)
{
  NOT_YET_IMPLEMENTED
}

void VKVertexBuffer::wrap_handle(uint64_t /*handle*/)
{
  NOT_YET_IMPLEMENTED
}

void VKVertexBuffer::update_sub(uint /*start*/, uint /*len*/, const void * /*data*/)
{
  NOT_YET_IMPLEMENTED
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
  /* TODO: Use mapped memory. */
  MEM_SAFE_FREE(data);
  data = (uchar *)MEM_mallocN(sizeof(uchar) * this->size_alloc_get(), __func__);
}

void VKVertexBuffer::resize_data()
{
  if (usage_ == GPU_USAGE_DEVICE_ONLY) {
    return;
  }

  data = (uchar *)MEM_reallocN(data, sizeof(uchar) * this->size_alloc_get());
}

void VKVertexBuffer::release_data()
{
  MEM_SAFE_FREE(data);
}

static bool inplace_conversion_supported(const GPUUsageType &usage)
{
  return ELEM(usage, GPU_USAGE_STATIC, GPU_USAGE_STREAM);
}

void *VKVertexBuffer::convert() const
{
  void *out_data = data;
  if (!inplace_conversion_supported(usage_)) {
    out_data = MEM_dupallocN(out_data);
  }
  BLI_assert(format.deinterleaved);
  convert_in_place(out_data, format, vertex_len);
  return out_data;
}

void VKVertexBuffer::upload_data()
{
  if (!buffer_.is_allocated()) {
    allocate();
  }

  if (flag & GPU_VERTBUF_DATA_DIRTY) {
    void *data_to_upload = data;
    if (conversion_needed(format)) {
      data_to_upload = convert();
    }
    buffer_.update(data_to_upload);
    if (data_to_upload != data) {
      MEM_SAFE_FREE(data_to_upload);
    }
    if (usage_ == GPU_USAGE_STATIC) {
      MEM_SAFE_FREE(data);
    }

    flag &= ~GPU_VERTBUF_DATA_DIRTY;
    flag |= GPU_VERTBUF_DATA_UPLOADED;
  }
}

void VKVertexBuffer::duplicate_data(VertBuf * /*dst*/)
{
  NOT_YET_IMPLEMENTED
}

void VKVertexBuffer::allocate()
{
  buffer_.create(size_alloc_get(),
                 usage_,
                 static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
  debug::object_label(buffer_.vk_handle(), "VertexBuffer");
}

}  // namespace blender::gpu
