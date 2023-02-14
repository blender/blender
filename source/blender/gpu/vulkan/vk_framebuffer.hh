/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_framebuffer_private.hh"

namespace blender::gpu {

class VKFrameBuffer : public FrameBuffer {
 public:
  VKFrameBuffer(const char *name) : FrameBuffer(name)
  {
  }

  void bind(bool enabled_srgb) override;
  bool check(char err_out[256]) override;
  void clear(eGPUFrameBufferBits buffers,
             const float clear_col[4],
             float clear_depth,
             uint clear_stencil) override;
  void clear_multi(const float (*clear_col)[4]) override;
  void clear_attachment(GPUAttachmentType type,
                        eGPUDataFormat data_format,
                        const void *clear_value) override;

  void attachment_set_loadstore_op(GPUAttachmentType type,
                                   eGPULoadOp load_action,
                                   eGPUStoreOp store_action) override;

  void read(eGPUFrameBufferBits planes,
            eGPUDataFormat format,
            const int area[4],
            int channel_len,
            int slot,
            void *r_data) override;

  void blit_to(eGPUFrameBufferBits planes,
               int src_slot,
               FrameBuffer *dst,
               int dst_slot,
               int dst_offset_x,
               int dst_offset_y) override;
};

}  // namespace blender::gpu
