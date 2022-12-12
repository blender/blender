/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_texture_private.hh"

namespace blender::gpu {

class VKTexture : public Texture {
 public:
  VKTexture(const char *name) : Texture(name)
  {
  }

  void generate_mipmap() override;
  void copy_to(Texture *tex) override;
  void clear(eGPUDataFormat format, const void *data) override;
  void swizzle_set(const char swizzle_mask[4]) override;
  void stencil_texture_mode_set(bool use_stencil) override;
  void mip_range_set(int min, int max) override;
  void *read(int mip, eGPUDataFormat format) override;
  void update_sub(
      int mip, int offset[3], int extent[3], eGPUDataFormat format, const void *data) override;
  void update_sub(int offset[3],
                  int extent[3],
                  eGPUDataFormat format,
                  GPUPixelBuffer *pixbuf) override;

  /* TODO(fclem): Legacy. Should be removed at some point. */
  uint gl_bindcode_get() const override;

 protected:
  bool init_internal() override;
  bool init_internal(GPUVertBuf *vbo) override;
  bool init_internal(const GPUTexture *src, int mip_offset, int layer_offset) override;
};

}  // namespace blender::gpu