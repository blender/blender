/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include <Cocoa/Cocoa.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

#include "MEM_guardedalloc.h"

#include "GPU_vertex_buffer.h"
#include "gpu_vertex_buffer_private.hh"
#include "mtl_context.hh"

namespace blender::gpu {

class MTLVertBuf : public VertBuf {
  friend class gpu::MTLTexture; /* For buffer texture. */
  friend class MTLShader;       /* For transform feedback. */
  friend class MTLBatch;
  friend class MTLContext;    /* For transform feedback. */
  friend class MTLStorageBuf; /* For bind as SSBO resource access and copy sub. */

 private:
  /** Metal buffer allocation. **/
  gpu::MTLBuffer *vbo_ = nullptr;
  /** Texture used if the buffer is bound as buffer texture. Init on first use. */
  ::GPUTexture *buffer_texture_ = nullptr;
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
  ~MTLVertBuf();

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
  void duplicate_data(VertBuf *dst) override;
  void bind_as_ssbo(uint binding) override;
  void bind_as_texture(uint binding) override;

  MEM_CXX_CLASS_ALLOC_FUNCS("MTLVertBuf");
};

}  // namespace blender::gpu
