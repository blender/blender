/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "MEM_guardedalloc.h"
#include "gpu_index_buffer_private.hh"
#include "mtl_context.hh"
#include <Cocoa/Cocoa.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

namespace blender::gpu {

class MTLIndexBuf : public IndexBuf {
  friend class MTLBatch;
  friend class MTLDrawList;
  friend class MTLStorageBuf; /* For bind as SSBO resource access. */

 private:
  /* Metal buffer resource. */
  gpu::MTLBuffer *ibo_ = nullptr;
  uint64_t alloc_size_ = 0;

  /* SSBO wrapper for bind_as_ssbo support. */
  MTLStorageBuf *ssbo_wrapper_ = nullptr;

#ifndef NDEBUG
  /* Flags whether point index buffer has been compacted
   * to remove false restart indices. */
  bool point_restarts_stripped_ = false;
#endif

  /* Optimized index buffers.
   * NOTE(Metal): This optimization encodes a new index buffer following
   * #TriangleList topology. Parsing of Index buffers is more optimal
   * when not using restart-compatible primitive topology types. */
  GPUPrimType optimized_primitive_type_;
  gpu::MTLBuffer *optimized_ibo_ = nullptr;
  uint32_t emulated_v_count = 0;
  void free_optimized_buffer();

  /* Flags whether an index buffer can be optimized.
   * For index buffers which are partially modified
   * on the host, or by the GPU, optimization cannot be performed. */
  bool can_optimize_ = true;

 public:
  ~MTLIndexBuf();

  void bind_as_ssbo(uint32_t binding) override;
  void read(uint32_t *data) const override;

  void upload_data() override;
  void update_sub(uint32_t start, uint32_t len, const void *data) override;

  /* #get_index_buffer can conditionally return an optimized index buffer of a
   * differing format, if it is concluded that optimization is preferred
   * for the given inputs.
   * Index buffer optimization is used to replace restart-compatible
   * primitive types with non-restart-compatible ones such as #TriangleList and
   * #LineList. This improves GPU execution for these types significantly, while
   * only incurring a small performance penalty.
   *
   * This is also used to emulate unsupported topology types
   * such as triangle fan. */
  id<MTLBuffer> get_index_buffer(GPUPrimType &in_out_primitive_type, uint &in_out_v_count);
  void flag_can_optimize(bool can_optimize);

  static MTLIndexType gpu_index_type_to_metal(GPUIndexBufType type)
  {
    return (type == GPU_INDEX_U16) ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32;
  }

 private:
  void strip_restart_indices() override;

  MEM_CXX_CLASS_ALLOC_FUNCS("MTLIndexBuf")
};

}  // namespace blender::gpu
