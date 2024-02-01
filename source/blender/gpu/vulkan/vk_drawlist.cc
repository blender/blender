/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "GPU_batch.h"

#include "vk_batch.hh"
#include "vk_common.hh"
#include "vk_drawlist.hh"
#include "vk_index_buffer.hh"
#include "vk_vertex_buffer.hh"

namespace blender::gpu {

VKDrawList::VKDrawList(int list_length) : length_(list_length)
{
  command_buffer_.create(list_length * sizeof(VkDrawIndexedIndirectCommand),
                         GPU_USAGE_DYNAMIC,
                         VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                         true);
}

void VKDrawList::append(GPUBatch *gpu_batch, int instance_first, int instance_count)
{
  /* Check for different batch. When batch is different the previous commands should be flushed to
   * the gpu. */
  VKBatch *batch = unwrap(gpu_batch);
  if (batch_ != batch) {
    submit();
    batch_ = batch;
  }

  /* Record the new command */
  const VKIndexBuffer *index_buffer = batch_->index_buffer_get();
  const bool is_indexed = index_buffer != nullptr;
  if (is_indexed) {
    VkDrawIndexedIndirectCommand &command = get_command<VkDrawIndexedIndirectCommand>();
    command.firstIndex = index_buffer->index_base_get();
    command.vertexOffset = index_buffer->index_start_get();
    command.indexCount = index_buffer->index_len_get();
    command.firstInstance = instance_first;
    command.instanceCount = instance_count;
  }
  else {
    const VKVertexBuffer *vertex_buffer = batch_->vertex_buffer_get(0);
    if (vertex_buffer == nullptr) {
      batch_ = nullptr;
      return;
    }
    VkDrawIndirectCommand &command = get_command<VkDrawIndirectCommand>();
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
  if (command_index_ > 1) {
    printf("%s: %d\n", __func__, command_index_);
  }

  const VKIndexBuffer *index_buffer = batch_->index_buffer_get();
  const bool is_indexed = index_buffer != nullptr;
  command_buffer_.flush();
  batch_->multi_draw_indirect(command_buffer_.vk_handle(),
                              command_index_,
                              0,
                              is_indexed ? sizeof(VkDrawIndexedIndirectCommand) :
                                           sizeof(VkDrawIndirectCommand));
  command_index_ = 0;
  batch_ = nullptr;
}

}  // namespace blender::gpu
