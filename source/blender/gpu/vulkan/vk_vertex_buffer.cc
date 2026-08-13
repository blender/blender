/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "MEM_guardedalloc.h"

#include "gpu_capabilities_private.hh"

#include "render_graph/nodes/vk_copy_buffer_node.hh"
#include "vk_data_conversion.hh"
#include "vk_shader.hh"
#include "vk_shader_interface.hh"
#include "vk_staging_buffer.hh"
#include "vk_state_manager.hh"
#include "vk_vertex_buffer.hh"

#include "CLG_log.h"

namespace blender {

static CLG_LogRef LOG = {"gpu.vulkan"};

namespace gpu {

VKVertexBuffer::~VKVertexBuffer()
{
  release_data();
}

void VKVertexBuffer::bind_as_ssbo(uint binding)
{
  VKContext &context = *VKContext::get();
  VKStateManager &state_manager = context.state_manager_get();
  state_manager.storage_buffer_bind(BindSpaceStorageBuffers::Type::VertexBuffer, this, binding);
}

void VKVertexBuffer::bind_as_texture(uint binding)
{
  VKContext &context = *VKContext::get();
  VKStateManager &state_manager = context.state_manager_get();
  state_manager.texel_buffer_bind(*this, binding);
}

void VKVertexBuffer::ensure_updated()
{
  upload_data();
}

void VKVertexBuffer::ensure_buffer_view()
{
  if (vk_buffer_view_ != VK_NULL_HANDLE) {
    return;
  }

  VkBufferViewCreateInfo buffer_view_info = {};
  buffer_view_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
  buffer_view_info.buffer = buffer_.vk_handle();
  buffer_view_info.format = to_vk_format();
  buffer_view_info.range = buffer_.size_in_bytes();

  const VKDevice &device = VKBackend::get().device;
  device.functions.vkCreateBufferView(
      device.vk_handle(), &buffer_view_info, nullptr, &vk_buffer_view_);
  debug::object_label(vk_buffer_view_, "VertexBufferView");
}

void VKVertexBuffer::wrap_handle(uint64_t /*handle*/)
{
  NOT_YET_IMPLEMENTED
}

void VKVertexBuffer::update_sub(uint start_offset, uint data_size_in_bytes, const void *data)
{
  if (!buffer_.is_allocated()) {
    /* Allocating huge buffers can fail, in that case we skip copying data. */
    return;
  }
  BLI_assert_msg(start_offset + data_size_in_bytes <= buffer_.size_in_bytes(),
                 "Out of bound write to vertex buffer");
  if (buffer_.is_mapped()) {
    buffer_.update_sub_immediately(start_offset, data_size_in_bytes, data);
  }
  else {
    VKContext &context = *VKContext::get();
    VKStagingBuffer staging_buffer(
        buffer_, VKStagingBuffer::Direction::HostToDevice, start_offset, data_size_in_bytes);
    memcpy(staging_buffer.host_buffer_get().mapped_memory_get(), data, data_size_in_bytes);
    staging_buffer.copy_to_device(context);
  }
}

void VKVertexBuffer::copy_sub(VertBuf &source_buf,
                              uint source_first_vertex,
                              uint dest_first_vertex,
                              uint vertex_len)
{
  BLI_assert(format.stride == source_buf.format.stride);
  BLI_assert_msg(size_t(source_first_vertex) + vertex_len <= source_buf.vertex_alloc,
                 "Copy source range exceeds the source vertex buffer bounds");
  BLI_assert_msg(size_t(dest_first_vertex) + vertex_len <= vertex_alloc,
                 "Copy destination range exceeds the vertex buffer bounds");
  VKVertexBuffer &source_vertex_buffer = unwrap(source_buf);
  BLI_assert_msg(buffer_.is_allocated(), "GPU_vertbuf_use() not called on this buffer");
  BLI_assert_msg(source_vertex_buffer.buffer_.is_allocated(),
                 "GPU_vertbuf_use() not called on the source buffer");
  VKContext &context = *VKContext::get();
  render_graph::VKCopyBufferNode::CreateInfo copy_buffer = {};
  copy_buffer.src_buffer = source_vertex_buffer.buffer_.resource();
  copy_buffer.dst_buffer = buffer_.resource();
  copy_buffer.region.srcOffset = source_first_vertex * source_vertex_buffer.format.stride;
  copy_buffer.region.dstOffset = dest_first_vertex * format.stride;
  copy_buffer.region.size = vertex_len * format.stride;
  context.render_graph().add_node(copy_buffer);
}

void VKVertexBuffer::read(void *data) const
{
  VKContext &context = *VKContext::get();
  if (buffer_.is_mapped()) {
    buffer_.read(context, data);
    return;
  }

  /* Allocating huge buffers can fail, in that case we skip copying data. */
  if (buffer_.is_allocated()) {
    VKStagingBuffer staging_buffer(buffer_, VKStagingBuffer::Direction::DeviceToHost);
    VKBuffer &buffer = staging_buffer.host_buffer_get();
    if (buffer.is_mapped()) {
      staging_buffer.copy_from_device(context);
      staging_buffer.host_buffer_get().read(context, data);
    }
    else {
      CLOG_ERROR(
          &LOG,
          "Unable to read data from vertex buffer via a staging buffer as the staging buffer "
          "could not be allocated. ");
    }
  }
}

void VKVertexBuffer::acquire_data()
{
  if (usage_ == GPU_USAGE_DEVICE_ONLY) {
    return;
  }

  /* Discard previous data if any. */
  /* TODO: Use mapped memory. */
  MEM_SAFE_DELETE(data_);
  data_ = MEM_new_array_uninitialized<uchar>(this->size_alloc_get(), __func__);
}

void VKVertexBuffer::resize_data()
{
  if (usage_ == GPU_USAGE_DEVICE_ONLY) {
    return;
  }

  data_ = static_cast<uchar *>(
      MEM_realloc_uninitialized(data_, sizeof(uchar) * this->size_alloc_get()));
}

void VKVertexBuffer::release_data()
{
  if (vk_buffer_view_ != VK_NULL_HANDLE) {
    VKDiscardPool::discard_pool_get().discard_buffer_view(vk_buffer_view_);
    vk_buffer_view_ = VK_NULL_HANDLE;
  }

  MEM_SAFE_DELETE(data_);
}

void VKVertexBuffer::upload_data_direct(const VKBuffer &host_buffer)
{
  host_buffer.update_immediately(data_);
}

void VKVertexBuffer::upload_data_via_staging_buffer(VKContext &context)
{
  VKStagingBuffer staging_buffer(
      buffer_, VKStagingBuffer::Direction::HostToDevice, 0, this->size_used_get());
  VKBuffer &buffer = staging_buffer.host_buffer_get();
  if (buffer.is_allocated()) {
    upload_data_direct(buffer);
    staging_buffer.copy_to_device(context);
  }
  else {
    CLOG_ERROR(&LOG,
               "Unable to upload data to vertex buffer via a staging buffer as the staging buffer "
               "could not be allocated. Vertex buffer will be filled with on zeros to reduce "
               "drawing artifacts due to read from uninitialized memory.");
    buffer_.clear(context, 0u);
  }
}

void VKVertexBuffer::upload_data()
{
  if (!buffer_.is_allocated()) {
    allocate();
    /* If allocation fails, don't upload. */
    if (!buffer_.is_allocated()) {
      CLOG_ERROR(&LOG, "Unable to allocate vertex buffer. Most likely an out of memory issue.");
      return;
    }
  }

  if (!ELEM(usage_, GPU_USAGE_STATIC, GPU_USAGE_STREAM, GPU_USAGE_DYNAMIC)) {
    return;
  }

  if (flag & GPU_VERTBUF_DATA_DIRTY) {
    if (buffer_.is_mapped() && !data_uploaded_) {
      upload_data_direct(buffer_);
    }
    else {
      VKContext &context = *VKContext::get();
      upload_data_via_staging_buffer(context);
    }
    if (usage_ == GPU_USAGE_STATIC) {
      MEM_SAFE_DELETE(data_);
    }
    data_uploaded_ = true;

    flag &= ~GPU_VERTBUF_DATA_DIRTY;
    flag |= GPU_VERTBUF_DATA_UPLOADED;
  }
}

void VKVertexBuffer::allocate()
{
  VkBufferUsageFlags vk_buffer_usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                       VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  if (GCaps.ray_query_support) {
    vk_buffer_usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  }

  buffer_.create(size_alloc_get(),
                 vk_buffer_usage,
                 VMA_MEMORY_USAGE_AUTO,
                 VmaAllocationCreateFlags(0),
                 0.8f,
                 false,
                 "VertexBuffer");
}

}  // namespace gpu
}  // namespace blender
