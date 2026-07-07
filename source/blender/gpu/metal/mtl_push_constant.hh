/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_shader_create_info.hh"

namespace blender::gpu {

/* Metal Shader Uniform data store.
 * This blocks is used to store current shader push_constant
 * data before it is submitted to the GPU. This is currently
 * stored per shader instance, though depending on GPU module
 * functionality, this could potentially be a global data store.
 * This data is associated with the PushConstantBlock. */
class MTLPushConstantBuf {
 private:
  uint8_t *data_ = nullptr;
  /* Offset inside buffer for allocating push constants. */
  size_t offset_ = 0;
  /* Size of the allocated buffer in `data_`. */
  size_t size_ = 0;
  /* True if the data has been touched and must be resent to GPU. */
  bool is_dirty_ = true;

 public:
  MTLPushConstantBuf(const shader::ShaderCreateInfo &info);
  ~MTLPushConstantBuf();

  /* Append uniform description to the buffer and return the location inside the buffer. */
  int append(shader::ShaderCreateInfo::PushConst push_constant);

  uint8_t *data()
  {
    return data_;
  }

  const uint8_t *data() const
  {
    return data_;
  }

  size_t size() const
  {
    return size_;
  }

  bool is_dirty() const
  {
    return is_dirty_;
  }

  void tag_dirty()
  {
    is_dirty_ = true;
  }

  void tag_updated()
  {
    is_dirty_ = false;
  }
};

}  // namespace blender::gpu
