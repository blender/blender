/* SPDX-FileCopyrightText: 2023 Blender Foundation
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
#include "gpu_vertex_format_private.h"

#include "vk_buffer.hh"
#include "vk_context.hh"
#include "vk_mem_alloc.h"
#include "vk_resource_tracker.hh"
#include "vk_vertex_attribute_object.hh"

namespace blender::gpu {

/* Size of internal buffer. */
constexpr size_t DEFAULT_INTERNAL_BUFFER_SIZE = (4 * 1024 * 1024);

class VKImmediate : public Immediate, VKResourceTracker<VKBuffer> {
 private:
  VKVertexAttributeObject vertex_attributes_;

  VkDeviceSize buffer_offset_ = 0;
  VkDeviceSize current_subbuffer_len_ = 0;

 public:
  VKImmediate();
  virtual ~VKImmediate();

  uchar *begin(void) override;
  void end(void) override;

  friend class VKVertexAttributeObject;

 private:
  VkDeviceSize subbuffer_offset_get();
  VkDeviceSize buffer_bytes_free();

  std::unique_ptr<VKBuffer> create_resource(VKContext &context) override;
};

}  // namespace blender::gpu
