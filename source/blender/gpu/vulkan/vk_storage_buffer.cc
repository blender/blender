/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */
#include "vk_shader.hh"
#include "vk_shader_interface.hh"
#include "vk_staging_buffer.hh"
#include "vk_state_manager.hh"
#include "vk_vertex_buffer.hh"

#include "vk_storage_buffer.hh"

namespace blender::gpu {

VKStorageBuffer::VKStorageBuffer(int size, GPUUsageType usage, const char *name)
    : StorageBuf(size, name), usage_(usage)
{
}

void VKStorageBuffer::update(const void *data)
{
  VKContext &context = *VKContext::get();
  ensure_allocated();
  VKStagingBuffer staging_buffer(buffer_, VKStagingBuffer::Direction::HostToDevice);
  staging_buffer.host_buffer_get().update(data);
  staging_buffer.copy_to_device(context);
}

void VKStorageBuffer::ensure_allocated()
{
  if (!buffer_.is_allocated()) {
    allocate();
  }
}

void VKStorageBuffer::allocate()
{
  const bool is_host_visible = false;
  const VkBufferUsageFlags buffer_usage_flags = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  buffer_.create(size_in_bytes_, usage_, buffer_usage_flags, is_host_visible);
  debug::object_label(buffer_.vk_handle(), name_);
}

void VKStorageBuffer::bind(int slot)
{
  VKContext &context = *VKContext::get();
  context.state_manager_get().storage_buffer_bind(*this, slot);
}

void VKStorageBuffer::add_to_descriptor_set(AddToDescriptorSetContext &data,
                                            int binding,
                                            shader::ShaderCreateInfo::Resource::BindType bind_type,
                                            const GPUSamplerState /*sampler_state*/)
{
  ensure_allocated();
  const std::optional<VKDescriptorSet::Location> location =
      data.shader_interface.descriptor_set_location(bind_type, binding);
  if (location) {
    data.descriptor_set.bind(*this, *location);
    render_graph::VKBufferAccess buffer_access = {};
    buffer_access.vk_buffer = buffer_.vk_handle();
    buffer_access.vk_access_flags = data.shader_interface.access_mask(bind_type, binding);
    data.resource_access_info.buffers.append(buffer_access);
  }
}

void VKStorageBuffer::unbind()
{
  unbind_from_active_context();
}

void VKStorageBuffer::clear(uint32_t clear_value)
{
  ensure_allocated();
  VKContext &context = *VKContext::get();
  buffer_.clear(context, clear_value);
}

void VKStorageBuffer::copy_sub(VertBuf *src, uint dst_offset, uint src_offset, uint copy_size)
{
  ensure_allocated();

  VKVertexBuffer &src_vertex_buffer = *unwrap(src);
  src_vertex_buffer.upload();

  render_graph::VKCopyBufferNode::CreateInfo copy_buffer = {};
  copy_buffer.src_buffer = src_vertex_buffer.vk_handle();
  copy_buffer.dst_buffer = vk_handle();
  copy_buffer.region.srcOffset = src_offset;
  copy_buffer.region.dstOffset = dst_offset;
  copy_buffer.region.size = copy_size;

  VKContext &context = *VKContext::get();
  if (use_render_graph) {
    context.render_graph.add_node(copy_buffer);
  }
  else {
    VKCommandBuffers &command_buffers = context.command_buffers_get();
    command_buffers.copy(
        buffer_, copy_buffer.src_buffer, Span<VkBufferCopy>(&copy_buffer.region, 1));
    context.flush();
  }
}

void VKStorageBuffer::async_flush_to_host()
{
  GPU_memory_barrier(GPU_BARRIER_BUFFER_UPDATE);
}

void VKStorageBuffer::read(void *data)
{
  ensure_allocated();
  VKContext &context = *VKContext::get();
  if (!use_render_graph) {
    context.flush();
  }

  VKStagingBuffer staging_buffer(buffer_, VKStagingBuffer::Direction::DeviceToHost);
  staging_buffer.copy_from_device(context);
  staging_buffer.host_buffer_get().read(context, data);
}

}  // namespace blender::gpu
