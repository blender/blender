/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_utility_mixins.hh"

#include "gpu_uniform_buffer_private.hh"

#include "vk_bindable_resource.hh"
#include "vk_buffer.hh"

namespace blender::gpu {

class VKUniformBuffer : public UniformBuf, public VKBindableResource, NonCopyable {
  VKBuffer buffer_;

 public:
  VKUniformBuffer(int size, const char *name) : UniformBuf(size, name) {}

  void update(const void *data) override;
  void clear_to_zero() override;
  void bind(int slot) override;
  void bind_as_ssbo(int slot) override;

  /**
   * Unbind uniform buffer from active context.
   */
  void unbind() override;

  VkBuffer vk_handle() const
  {
    return buffer_.vk_handle();
  }

  size_t size_in_bytes() const
  {
    return size_in_bytes_;
  }

  /* Bindable resource */
  void add_to_descriptor_set(AddToDescriptorSetContext &data,
                             int binding,
                             shader::ShaderCreateInfo::Resource::BindType bind_type,
                             const GPUSamplerState sampler_state) override;

 private:
  void allocate();
};

BLI_INLINE UniformBuf *wrap(VKUniformBuffer *uniform_buffer)
{
  return static_cast<UniformBuf *>(uniform_buffer);
}

}  // namespace blender::gpu
