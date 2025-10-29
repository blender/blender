/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "GPU_vertex_buffer.hh"
#include "MEM_guardedalloc.h"

#include "mtl_context.hh"

#include <Cocoa/Cocoa.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

namespace blender::gpu {

MTLVertexFormat gpu_vertex_format_to_metal(VertAttrType vert_format);
MTLVertexFormat gpu_type_to_metal_vertex_format(shader::Type type);

class MTLVertBuf : public VertBuf {
  friend class gpu::MTLTexture; /* For buffer texture. */
  friend class MTLBatch;
  friend class MTLStorageBuf; /* For bind as SSBO resource access and copy sub. */

 private:
  /** Metal buffer allocation. */
  gpu::MTLBuffer *vbo_ = nullptr;
  /** Texture used if the buffer is bound as buffer texture. Init on first use. */
  gpu::Texture *buffer_texture_ = nullptr;
  /** Defines whether the buffer handle is wrapped by this MTLVertBuf, i.e. we do not own it and
   * should not free it. */
  bool is_wrapper_ = false;
  /** Requested allocation size for Metal buffer.
   * Differs from raw buffer size as alignment is not included. */
  uint64_t alloc_size_ = 0;
  /** Whether existing allocation has been submitted for use by the GPU. */
  bool contents_in_flight_ = false;
  /* SSBO wrapper for bind_as_ssbo support. */
  MTLStorageBuf *ssbo_wrapper_ = nullptr;

  /* Fetch Metal buffer and offset into allocation if necessary.
   * Access limited to friend classes. */
  id<MTLBuffer> get_metal_buffer()
  {
    BLI_assert(vbo_ != nullptr);
    vbo_->debug_ensure_used();
    return vbo_->get_metal_buffer();
  }

 public:
  MTLVertBuf();
  ~MTLVertBuf() override;

  void bind();
  void flag_used();

  void update_sub(uint start, uint len, const void *data) override;

  void read(void *data) const override;

  void wrap_handle(uint64_t handle) override;

 protected:
  void acquire_data() override;
  void resize_data() override;
  void release_data() override;
  void upload_data() override;
  void bind_as_ssbo(uint binding) override;
  void bind_as_texture(uint binding) override;

  MEM_CXX_CLASS_ALLOC_FUNCS("MTLVertBuf");
};

}  // namespace blender::gpu
