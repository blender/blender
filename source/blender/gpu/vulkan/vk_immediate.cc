/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Mimics old style OpenGL immediate mode drawing.
 */

#include "GPU_capabilities.hh"
#include "GPU_matrix.hh"

#include "vk_backend.hh"
#include "vk_context.hh"
#include "vk_framebuffer.hh"
#include "vk_immediate.hh"
#include "vk_state_manager.hh"

#include "CLG_log.h"

namespace blender::gpu {

static CLG_LogRef LOG = {"gpu.vulkan"};

VKImmediate::VKImmediate() {}
VKImmediate::~VKImmediate()
{
  BLI_assert_msg(recycling_buffers_.is_empty(),
                 "VKImmediate::deinit must be called before destruction.");
  BLI_assert_msg(active_buffers_.is_empty(),
                 "VKImmediate::deinit must be called before destruction");
}

void VKImmediate::deinit(VKDevice &device)
{
  while (!recycling_buffers_.is_empty()) {
    std::unique_ptr<VKBuffer> buffer = recycling_buffers_.pop_last();
    buffer->free_immediately(device);
    buffer.release();
  }
  while (!active_buffers_.is_empty()) {
    std::unique_ptr<VKBuffer> buffer = active_buffers_.pop_last();
    buffer->free_immediately(device);
    buffer.release();
  }
}

uchar *VKImmediate::begin()
{
  uint add_vertex = prim_type == GPU_PRIM_LINE_LOOP ? 1 : 0;
  const size_t bytes_needed = vertex_buffer_size(&vertex_format, vertex_len + add_vertex);
  size_t offset_alignment = GPU_storage_buffer_alignment();
  VKBuffer &buffer = ensure_space(bytes_needed, offset_alignment);

  /* Apply alignment when allocating new sub buffer, to reduce signed/unsigned data conversion
   * later on. */
  buffer_offset_ += offset_alignment - 1;
  buffer_offset_ &= ~(offset_alignment - 1);
  BLI_assert((buffer_offset_ & (offset_alignment - 1)) == 0);

  current_subbuffer_len_ = bytes_needed;
  uchar *data = static_cast<uchar *>(buffer.mapped_memory_get());
  return data + buffer_offset_;
}

void VKImmediate::end()
{
  BLI_assert_msg(prim_type != GPU_PRIM_NONE, "Illegal state: not between an immBegin/End pair.");
  if (vertex_idx == 0) {
    return;
  }

  if (prim_type == GPU_PRIM_LINE_LOOP) {
    uchar *first_vertex_ptr = static_cast<uchar *>(active_buffers_.last()->mapped_memory_get()) +
                              buffer_offset_;
    size_t vertex_stride = current_subbuffer_len_ / (vertex_len + 1);
    uchar *last_vertex_ptr = first_vertex_ptr + vertex_stride * vertex_len;
    memcpy(last_vertex_ptr, first_vertex_ptr, vertex_stride);

    prim_type = GPU_PRIM_LINE_STRIP;
    vertex_idx += 1;
  }

  VKContext &context = *VKContext::get();
  BLI_assert(context.shader == unwrap(shader));
  Shader &shader = *unwrap(this->shader);
  if (shader.is_polyline) {
    VKBuffer *buffer = active_buffers_.last().get();
    VKStateManager &state_manager = context.state_manager_get();
    state_manager.storage_buffer_bind(BindSpaceStorageBuffers::Type::Buffer,
                                      buffer,
                                      GPU_SSBO_POLYLINE_POS_BUF_SLOT,
                                      buffer_offset_);
    state_manager.storage_buffer_bind(BindSpaceStorageBuffers::Type::Buffer,
                                      buffer,
                                      GPU_SSBO_POLYLINE_COL_BUF_SLOT,
                                      buffer_offset_);
    /* Not used. Satisfy the binding. */
    state_manager.storage_buffer_bind(
        BindSpaceStorageBuffers::Type::Buffer, buffer, GPU_SSBO_INDEX_BUF_SLOT, buffer_offset_);
    this->polyline_draw_workaround(0);
  }
  else {
    GPU_matrix_bind(wrap(context.shader));
    render_graph::VKResourceAccessInfo &resource_access_info = context.reset_and_get_access_info();
    vertex_attributes_.update_bindings(*this);
    context.active_framebuffer_get()->rendering_ensure(context);

    render_graph::VKDrawNode::CreateInfo draw(resource_access_info);
    draw.node_data.vertex_count = vertex_idx;
    draw.node_data.instance_count = 1;
    draw.node_data.first_vertex = 0;
    draw.node_data.first_instance = 0;

    context.active_framebuffer_get()->vk_viewports_append(draw.node_data.viewport_data.viewports);
    context.active_framebuffer_get()->vk_render_areas_append(
        draw.node_data.viewport_data.scissors);

    vertex_attributes_.bind(draw.node_data.vertex_buffers);
    context.update_pipeline_data(prim_type, vertex_attributes_, draw.node_data.pipeline_data);

    context.render_graph().add_node(draw);
  }

  buffer_offset_ += current_subbuffer_len_;
  current_subbuffer_len_ = 0;
}

VKBufferWithOffset VKImmediate::active_buffer() const
{
  VKBufferWithOffset result = {active_buffers_.last()->vk_handle(), buffer_offset_};
  return result;
}

VkDeviceSize VKImmediate::buffer_bytes_free()
{
  return active_buffers_.last()->size_in_bytes() - buffer_offset_;
}

static VkDeviceSize new_buffer_size(VkDeviceSize sub_buffer_size)
{
  return max_ulul(sub_buffer_size, DEFAULT_INTERNAL_BUFFER_SIZE);
}

VKBuffer &VKImmediate::ensure_space(VkDeviceSize bytes_needed, VkDeviceSize offset_alignment)
{
  VkDeviceSize bytes_required = bytes_needed + offset_alignment;

  /* Last used buffer still has space. */
  if (!active_buffers_.is_empty() && buffer_bytes_free() >= bytes_required) {
    return *active_buffers_.last();
  }

  /* Recycle a previous buffer. */
  if (!recycling_buffers_.is_empty() &&
      recycling_buffers_.last()->size_in_bytes() >= bytes_required)
  {
    CLOG_DEBUG(&LOG, "Activating recycled buffer");
    buffer_offset_ = 0;
    active_buffers_.append(recycling_buffers_.pop_last());
    return *active_buffers_.last();
  }

  /* Offset alignment isn't needed when creating buffers as it is managed by VMA. */
  VkDeviceSize alloc_size = new_buffer_size(bytes_needed);
  CLOG_DEBUG(&LOG, "Allocate buffer (size=%d)", int(alloc_size));
  buffer_offset_ = 0;
  active_buffers_.append(std::make_unique<VKBuffer>());
  VKBuffer &result = *active_buffers_.last();
  result.create(alloc_size,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VMA_ALLOCATION_CREATE_MAPPED_BIT |
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
  debug::object_label(result.vk_handle(), "Immediate");

  return result;
}

void VKImmediate::reset()
{
  if (!recycling_buffers_.is_empty()) {
    CLOG_DEBUG(&LOG, "Discarding %d unused buffers", int(recycling_buffers_.size()));
  }
  recycling_buffers_.clear();
  recycling_buffers_ = std::move(active_buffers_);
}

}  // namespace blender::gpu
