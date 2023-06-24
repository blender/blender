/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "MEM_guardedalloc.h"

#include "vk_data_conversion.hh"
#include "vk_memory.hh"
#include "vk_shader.hh"
#include "vk_shader_interface.hh"
#include "vk_state_manager.hh"
#include "vk_vertex_buffer.hh"

namespace blender::gpu {

VKVertexBuffer::~VKVertexBuffer()
{
  release_data();
}

void VKVertexBuffer::bind_as_ssbo(uint binding)
{
  VKContext &context = *VKContext::get();
  VKStateManager &state_manager = context.state_manager_get();
  state_manager.storage_buffer_bind(*this, binding);
}

void VKVertexBuffer::bind_as_texture(uint binding)
{
  VKContext &context = *VKContext::get();
  VKStateManager &state_manager = context.state_manager_get();
  state_manager.texel_buffer_bind(*this, binding);
}

void VKVertexBuffer::bind(int binding, shader::ShaderCreateInfo::Resource::BindType bind_type)
{
  VKContext &context = *VKContext::get();
  VKShader *shader = static_cast<VKShader *>(context.shader);
  const VKShaderInterface &shader_interface = shader->interface_get();
  const std::optional<VKDescriptorSet::Location> location =
      shader_interface.descriptor_set_location(bind_type, binding);
  if (!location) {
    return;
  }

  upload_data();

  if (bind_type == shader::ShaderCreateInfo::Resource::BindType::SAMPLER &&
      vk_buffer_view_ == VK_NULL_HANDLE)
  {
    VkBufferViewCreateInfo buffer_view_info = {};
    eGPUTextureFormat texture_format = to_texture_format(&format);

    buffer_view_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    buffer_view_info.buffer = buffer_.vk_handle();
    buffer_view_info.format = to_vk_format(texture_format);
    buffer_view_info.range = buffer_.size_in_bytes();

    VK_ALLOCATION_CALLBACKS;
    const VKDevice &device = VKBackend::get().device_get();
    vkCreateBufferView(
        device.device_get(), &buffer_view_info, vk_allocation_callbacks, &vk_buffer_view_);
  }

  /* TODO: Check if we can move this check inside the descriptor set. */
  VKDescriptorSetTracker &descriptor_set = shader->pipeline_get().descriptor_set_get();
  if (bind_type == shader::ShaderCreateInfo::Resource::BindType::SAMPLER) {
    descriptor_set.bind(*this, *location);
  }
  else {
    descriptor_set.bind_as_ssbo(*this, *location);
  }
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
  if (vk_buffer_view_ != VK_NULL_HANDLE) {
    const VKDevice &device = VKBackend::get().device_get();
    VK_ALLOCATION_CALLBACKS;
    vkDestroyBufferView(device.device_get(), vk_buffer_view_, vk_allocation_callbacks);
    vk_buffer_view_ = VK_NULL_HANDLE;
  }

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
  if (!ELEM(usage_, GPU_USAGE_STATIC, GPU_USAGE_STREAM, GPU_USAGE_DYNAMIC)) {
    return;
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
                 static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                    VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT));
  debug::object_label(buffer_.vk_handle(), "VertexBuffer");
}

}  // namespace blender::gpu
