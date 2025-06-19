/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

  /**
   * Has this uniform data already been fed with data. When so we are not allowed to directly
   * overwrite the data as it could still be in use.
   */
  bool data_uploaded_ = false;

 public:
  VKUniformBuffer(size_t size, const char *name) : UniformBuf(size, name) {}

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
  inline VkDeviceAddress device_address_get() const
  {
    return buffer_.device_address_get();
  }

  size_t size_in_bytes() const
  {
    return size_in_bytes_;
  }

  void ensure_updated();

  /**
   * Reset data uploaded flag. When the resource is sure it isn't used, the caller can call
   * reset_data_uploaded so the next update can use ReBAR when available.
   */
  void reset_data_uploaded()
  {
    data_uploaded_ = false;
  }

 private:
  void allocate();
};

BLI_INLINE UniformBuf *wrap(VKUniformBuffer *uniform_buffer)
{
  return static_cast<UniformBuf *>(uniform_buffer);
}

}  // namespace blender::gpu
