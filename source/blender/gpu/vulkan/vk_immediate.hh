/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Mimics old style OpenGL immediate mode drawing.
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "gpu_immediate_private.hh"
#include "gpu_vertex_format_private.hh"

#include "vk_buffer.hh"
#include "vk_data_conversion.hh"
#include "vk_mem_alloc.h"
#include "vk_vertex_attribute_object.hh"

namespace blender::gpu {

class VKDevice;

/* Size of internal buffer. */
constexpr size_t DEFAULT_INTERNAL_BUFFER_SIZE = 4 * 1024 * 1024;

class VKImmediate : public Immediate {
 private:
  VKVertexAttributeObject vertex_attributes_;

  VkDeviceSize buffer_offset_ = 0;
  VkDeviceSize current_subbuffer_len_ = 0;

  std::optional<VKBuffer> active_buffer_;

 public:
  uchar *begin() override;
  void end() override;

  friend class VKVertexAttributeObject;

 private:
  VKBufferWithOffset active_buffer() const;
  VkDeviceSize buffer_bytes_free();

  VKBuffer &ensure_space(VkDeviceSize bytes_needed, VkDeviceSize offset_alignment);
};

}  // namespace blender::gpu
