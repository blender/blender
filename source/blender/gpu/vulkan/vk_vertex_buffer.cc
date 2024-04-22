/* SPDX-FileCopyrightText: 2022 Blender Authors
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
#include "vk_staging_buffer.hh"
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

void VKVertexBuffer::add_to_descriptor_set(AddToDescriptorSetContext &data,
                                           int binding,
                                           shader::ShaderCreateInfo::Resource::BindType bind_type,
                                           const GPUSamplerState /*sampler_state*/)
{
  const std::optional<VKDescriptorSet::Location> location =
      data.shader_interface.descriptor_set_location(bind_type, binding);
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
  if (bind_type == shader::ShaderCreateInfo::Resource::BindType::SAMPLER) {
    data.descriptor_set.bind(*this, *location);
  }
  else {
    data.descriptor_set.bind_as_ssbo(*this, *location);
  }
  render_graph::VKBufferAccess buffer_access = {};
  buffer_access.vk_buffer = buffer_.vk_handle();
  buffer_access.vk_access_flags = data.shader_interface.access_mask(bind_type, *location);
  data.resource_access_info.buffers.append(buffer_access);
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
  if (!use_render_graph) {
    context.flush();
  }
  if (buffer_.is_mapped()) {
    buffer_.read(context, data);
    return;
  }

  VKStagingBuffer staging_buffer(buffer_, VKStagingBuffer::Direction::DeviceToHost);
  staging_buffer.copy_from_device(context);
  staging_buffer.host_buffer_get().read(context, data);
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

void VKVertexBuffer::upload_data_direct(const VKBuffer &host_buffer)
{
  device_format_ensure();
  if (vertex_format_converter.needs_conversion()) {
    if (G.debug & G_DEBUG_GPU) {
      std::cout << "PERFORMANCE: Vertex buffer requires conversion.\n";
    }
    vertex_format_converter.convert(host_buffer.mapped_memory_get(), data, vertex_len);
    host_buffer.flush();
  }
  else {
    host_buffer.update(data);
  }
}

void VKVertexBuffer::upload_data_via_staging_buffer(VKContext &context)
{
  VKStagingBuffer staging_buffer(buffer_, VKStagingBuffer::Direction::HostToDevice);
  upload_data_direct(staging_buffer.host_buffer_get());
  staging_buffer.copy_to_device(context);
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
    device_format_ensure();
    if (buffer_.is_mapped()) {
      upload_data_direct(buffer_);
    }
    else {
      VKContext &context = *VKContext::get();
      upload_data_via_staging_buffer(context);
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

void VKVertexBuffer::device_format_ensure()
{
  if (!vertex_format_converter.is_initialized()) {
    const VKWorkarounds &workarounds = VKBackend::get().device_get().workarounds_get();
    vertex_format_converter.init(&format, workarounds);
  }
}

const GPUVertFormat &VKVertexBuffer::device_format_get() const
{
  return vertex_format_converter.device_format_get();
}

void VKVertexBuffer::allocate()
{
  const bool is_host_visible = ELEM(usage_, GPU_USAGE_DYNAMIC, GPU_USAGE_STREAM);
  VkBufferUsageFlags vk_buffer_usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                       VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
  if (!is_host_visible) {
    vk_buffer_usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  }
  buffer_.create(size_alloc_get(), usage_, vk_buffer_usage, is_host_visible);
  debug::object_label(buffer_.vk_handle(), "VertexBuffer");
}

}  // namespace blender::gpu
