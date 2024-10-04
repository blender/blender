/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "GPU_index_buffer.hh"

#include "vk_buffer.hh"

namespace blender::gpu {

class VKIndexBuffer : public IndexBuf {
  VKBuffer buffer_;

 public:
  void upload_data() override;

  void bind_as_ssbo(uint binding) override;

  void read(uint32_t *data) const override;

  void update_sub(uint start, uint len, const void *data) override;

  VkBuffer vk_handle() const
  {
    return buffer_get().vk_handle();
  }
  VkIndexType vk_index_type() const
  {
    return to_vk_index_type(index_type_);
  }

  void ensure_updated();

 private:
  void strip_restart_indices() override;
  void allocate();
  VKBuffer &buffer_get();
  const VKBuffer &buffer_get() const;
};

static inline VKIndexBuffer *unwrap(IndexBuf *index_buffer)
{
  return static_cast<VKIndexBuffer *>(index_buffer);
}

}  // namespace blender::gpu
