/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_vertex_buffer_private.hh"

#include "vk_buffer.hh"

namespace blender::gpu {

class VKVertexBuffer : public VertBuf {
  VKBuffer buffer_;

 public:
  ~VKVertexBuffer();

  void bind_as_ssbo(uint binding) override;
  void bind_as_texture(uint binding) override;
  void wrap_handle(uint64_t handle) override;

  void update_sub(uint start, uint len, const void *data) override;
  void read(void *data) const override;

  VkBuffer vk_handle() const
  {
    BLI_assert(buffer_.is_allocated());
    return buffer_.vk_handle();
  }

 protected:
  void acquire_data() override;
  void resize_data() override;
  void release_data() override;
  void upload_data() override;
  void duplicate_data(VertBuf *dst) override;

 private:
  void allocate();
  void *convert() const;
};

static inline VKVertexBuffer *unwrap(VertBuf *vertex_buffer)
{
  return static_cast<VKVertexBuffer *>(vertex_buffer);
}

}  // namespace blender::gpu
