/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "GPU_batch.hh"

#include "vk_batch.hh"
#include "vk_common.hh"
#include "vk_context.hh"
#include "vk_drawlist.hh"
#include "vk_index_buffer.hh"
#include "vk_vertex_buffer.hh"

namespace blender::gpu {

VKDrawList::VKDrawList(int list_length) : length_(list_length) {}

void VKDrawList::append(Batch *gpu_batch, int instance_first, int instance_count)
{
  /* Check for different batch. When batch is different the previous commands should be flushed to
   * the gpu. */
  VKBatch *batch = unwrap(gpu_batch);
  if (batch_ != batch) {
    submit();
    batch_ = batch;
  }

  /* Record the new command */
  VKContext &context = *VKContext::get();
  const VKIndexBuffer *index_buffer = batch_->index_buffer_get();
  const bool is_indexed = index_buffer != nullptr;
  if (is_indexed) {
    /* Don't record commands for invalid GPUBatches. */
    if (index_buffer->index_len_get() == 0) {
      return;
    }

    const bool new_buffer_needed = command_index_ == 0;
    std::unique_ptr<VKBuffer> &buffer = tracked_resource_for(context, new_buffer_needed);
    VkDrawIndexedIndirectCommand &command = get_command<VkDrawIndexedIndirectCommand>(*buffer);
    command.firstIndex = index_buffer->index_base_get();
    command.vertexOffset = index_buffer->index_start_get();
    command.indexCount = index_buffer->index_len_get();
    command.firstInstance = instance_first;
    command.instanceCount = instance_count;
  }
  else {
    const VKVertexBuffer *vertex_buffer = batch_->vertex_buffer_get(0);
    /* Don't record commands for invalid GPUBatches. */
    if (vertex_buffer == nullptr || vertex_buffer->vertex_len == 0) {
      return;
    }

    const bool new_buffer_needed = command_index_ == 0;
    std::unique_ptr<VKBuffer> &buffer = tracked_resource_for(context, new_buffer_needed);
    VkDrawIndirectCommand &command = get_command<VkDrawIndirectCommand>(*buffer);
    command.vertexCount = vertex_buffer->vertex_len;
    command.instanceCount = instance_count;
    command.firstVertex = 0;
    command.firstInstance = instance_first;
  }
  command_index_++;

  /* Submit commands when command buffer is full. */
  if (command_index_ == length_) {
    submit();
  }
}

void VKDrawList::submit()
{
  if (batch_ == nullptr || command_index_ == 0) {
    command_index_ = 0;
    batch_ = nullptr;
    return;
  }

  const VKIndexBuffer *index_buffer = batch_->index_buffer_get();
  const bool is_indexed = index_buffer != nullptr;
  VKBuffer &buffer = *active_resource();
  batch_->multi_draw_indirect(buffer.vk_handle(),
                              command_index_,
                              0,
                              is_indexed ? sizeof(VkDrawIndexedIndirectCommand) :
                                           sizeof(VkDrawIndirectCommand));
  command_index_ = 0;
  batch_ = nullptr;
}

std::unique_ptr<VKBuffer> VKDrawList::create_resource(VKContext & /*context*/)
{
  const size_t bytes_needed = length_ * sizeof(VkDrawIndexedIndirectCommand);
  std::unique_ptr<VKBuffer> result = std::make_unique<VKBuffer>();
  result->create(bytes_needed, GPU_USAGE_DYNAMIC, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, true);
  debug::object_label(result->vk_handle(), "DrawList.Indirect");
  return result;
}

}  // namespace blender::gpu
