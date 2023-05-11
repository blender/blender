/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_utility_mixins.hh"

#include "gpu_uniform_buffer_private.hh"

#include "vk_buffer.hh"

namespace blender::gpu {

class VKUniformBuffer : public UniformBuf, NonCopyable {
  VKBuffer buffer_;

 public:
  VKUniformBuffer(int size, const char *name) : UniformBuf(size, name) {}

  void update(const void *data) override;
  void clear_to_zero() override;
  void bind(int slot) override;
  void bind_as_ssbo(int slot) override;
  void bind(int slot, shader::ShaderCreateInfo::Resource::BindType bind_type);
  void unbind() override;

  VkBuffer vk_handle() const
  {
    return buffer_.vk_handle();
  }

  size_t size_in_bytes() const
  {
    return size_in_bytes_;
  }

 private:
  void allocate();
};

}  // namespace blender::gpu
