/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_math_base_c.hh"

#include "vk_buffer.hh"
#include "vk_buffer_pool.hh"
#include "vk_context.hh"
#include "vk_debug.hh"

#include <memory>

namespace blender::gpu {

VKBufferPool::~VKBufferPool()
{
  BLI_assert(buffers_.is_empty());
}

VKBufferWithOffset VKBufferPool::append(Span<uint8_t> data)
{
  const bool should_allocate_new_buffer = buffers_.is_empty() ||
                                          (buffers_.last()->size_in_bytes() - buffer_offset_) <
                                              data.size();
  /* Flush last buffer as all data is available. */
  if (should_allocate_new_buffer && !buffers_.is_empty() && buffer_offset_ != 0) {
    finalize_active_buffer();
  }
  if (should_allocate_new_buffer) {
    VkDeviceSize allocation_size = std::max(VkDeviceSize(data.size()), default_buffer_size_);
    std::unique_ptr<VKBuffer> new_buffer = std::make_unique<VKBuffer>();
    new_buffer->create(allocation_size,
                       vk_buffer_usage_,
                       vma_memory_usage_,
                       vma_allocation_create_flags_,
                       priority_,
                       false,
                       name_.c_str());
    debug::object_label(new_buffer->vk_handle(), name_);
    buffers_.append(std::move(new_buffer));
    buffer_offset_ = 0;
  }
  VKBufferWithOffset result = {.buffer = buffers_.last()->vk_handle(), .offset = buffer_offset_};

  /* Resize data_ when more space is needed than the default_buffer_size_. */
  if (buffer_offset_ + data.size() > data_.size()) {
    data_.resize(buffer_offset_ + data.size());
  }
  data_.as_mutable_span()
      .slice(IndexRange::from_begin_size(buffer_offset_, data.size()))
      .copy_from(data);

  buffer_offset_ += data.size();
  buffer_offset_ = ceil_to_multiple_ul(buffer_offset_, alignment_);

  return result;
}

void VKBufferPool::ensure_uploaded()
{
  if (!buffers_.is_empty() && buffer_offset_ != 0) {
    finalize_active_buffer();
  }
}

void VKBufferPool::discard()
{
  buffers_.clear();
  buffer_offset_ = 0;
}

void VKBufferPool::finalize_active_buffer()
{
  BLI_assert(!buffers_.is_empty());
  std::unique_ptr<VKBuffer> &active_buffer = buffers_.last();
  if (active_buffer->is_mapped()) {
    active_buffer->update_sub_immediately(0, buffer_offset_, data_.data());
  }
  else {
    VKContext &context = *VKContext::get();
    void *data_copy = MEM_new_uninitialized(active_buffer->allocated_size_in_bytes(), __func__);
    memcpy(data_copy, data_.data(), std::min(data_.size(), int64_t(buffer_offset_)));
    active_buffer->update_render_graph(context, data_copy);
  }
}

}  // namespace blender::gpu
