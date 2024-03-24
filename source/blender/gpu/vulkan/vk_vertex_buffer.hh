/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "GPU_vertex_buffer.hh"

#include "vk_bindable_resource.hh"
#include "vk_buffer.hh"
#include "vk_data_conversion.hh"

namespace blender::gpu {

class VKVertexBuffer : public VertBuf, public VKBindableResource {
  VKBuffer buffer_;
  /** When a vertex buffer is used as a UNIFORM_TEXEL_BUFFER the buffer requires a buffer view. */
  VkBufferView vk_buffer_view_ = VK_NULL_HANDLE;

  VertexFormatConverter vertex_format_converter;

 public:
  ~VKVertexBuffer();

  void bind_as_ssbo(uint binding) override;
  void bind_as_texture(uint binding) override;
  void bind(int binding,
            shader::ShaderCreateInfo::Resource::BindType bind_type,
            const GPUSamplerState sampler_state) override;
  void wrap_handle(uint64_t handle) override;

  void update_sub(uint start, uint len, const void *data) override;
  void read(void *data) const override;

  VkBuffer vk_handle() const
  {
    BLI_assert(buffer_.is_allocated());
    return buffer_.vk_handle();
  }

  VkBufferView vk_buffer_view_get() const
  {
    BLI_assert(vk_buffer_view_ != VK_NULL_HANDLE);
    return vk_buffer_view_;
  }

  void device_format_ensure();
  const GPUVertFormat &device_format_get() const;

 protected:
  void acquire_data() override;
  void resize_data() override;
  void release_data() override;
  void upload_data() override;
  void duplicate_data(VertBuf *dst) override;

 private:
  void allocate();

  void upload_data_direct(const VKBuffer &host_buffer);
  void upload_data_via_staging_buffer(VKContext &context);

  /* VKTexture requires access to `buffer_` to convert a vertex buffer to a texture. */
  friend class VKTexture;
};

BLI_INLINE VKVertexBuffer *unwrap(VertBuf *vertex_buffer)
{
  return static_cast<VKVertexBuffer *>(vertex_buffer);
}

}  // namespace blender::gpu
