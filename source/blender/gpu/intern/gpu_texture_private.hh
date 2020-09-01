/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_assert.h"

#include "gpu_framebuffer_private.hh"

namespace blender {
namespace gpu {

typedef enum eGPUTextureFlag {
  GPU_TEXFORMAT_DEPTH = (1 << 0),
  GPU_TEXFORMAT_STENCIL = (1 << 1),
  GPU_TEXFORMAT_INTEGER = (1 << 2),
  GPU_TEXFORMAT_FLOAT = (1 << 3),

  GPU_TEXTURE_1D = (1 << 10),
  GPU_TEXTURE_2D = (1 << 11),
  GPU_TEXTURE_3D = (1 << 12),
  GPU_TEXTURE_CUBE = (1 << 13),
  GPU_TEXTURE_ARRAY = (1 << 14),
  GPU_TEXTURE_BUFFER = (1 << 15),

  GPU_TEXTURE_1D_ARRAY = (GPU_TEXTURE_1D | GPU_TEXTURE_ARRAY),
  GPU_TEXTURE_2D_ARRAY = (GPU_TEXTURE_2D | GPU_TEXTURE_ARRAY),
  GPU_TEXTURE_CUBE_ARRAY = (GPU_TEXTURE_CUBE | GPU_TEXTURE_ARRAY),

  GPU_TEXTURE_TARGET = (GPU_TEXTURE_1D | GPU_TEXTURE_2D | GPU_TEXTURE_3D | GPU_TEXTURE_CUBE |
                        GPU_TEXTURE_ARRAY),
} eGPUTextureFlag;

ENUM_OPERATORS(eGPUTextureFlag)

#ifdef DEBUG
#  define DEBUG_NAME_LEN 64
#else
#  define DEBUG_NAME_LEN 8
#endif

/* Maximum number of FBOs a texture can be attached to. */
#define GPU_TEX_MAX_FBO_ATTACHED 12

class Texture {
 public:
  /** Width & Height & Depth. */
  int w = 0, h = 0, d = 0;
  /** Number of color/alpha channels. */
  int components = 0;
  /** Internal data format and it's characteristics. */
  eGPUTextureFormat format;
  eGPUTextureFlag flag;
  /** Internal Sampler state. */
  eGPUSamplerState sampler_state;
  /** Number of mipmaps this texture has. */
  int mipmaps = 0;
  /** Reference counter. */
  int refcount = 0;
  /** Width & Height (of source data), optional. */
  int src_w = 0, src_h = 0;
  /** Framebuffer references to update on deletion. */
  GPUAttachmentType fb_attachment[GPU_TEX_MAX_FBO_ATTACHED];
  FrameBuffer *fb[GPU_TEX_MAX_FBO_ATTACHED];

 protected:
  /** For debugging */
  char name_[DEBUG_NAME_LEN];

 public:
  Texture(const char *name);
  virtual ~Texture();

  virtual void bind(int slot) = 0;
  virtual void update(void *data) = 0;
  virtual void update_sub(void *data, int offset[3], int size[3]) = 0;
  virtual void generate_mipmap(void) = 0;
  virtual void copy_to(Texture *tex) = 0;

  virtual void swizzle_set(char swizzle_mask[4]) = 0;

  /* TODO(fclem) Legacy. Should be removed at some point. */
  virtual uint gl_bindcode_get(void) = 0;

  void attach_to(FrameBuffer *fb);

  GPUAttachmentType attachment_type(int slot) const
  {
    switch (format) {
      case GPU_DEPTH_COMPONENT32F:
      case GPU_DEPTH_COMPONENT24:
      case GPU_DEPTH_COMPONENT16:
        BLI_assert(slot == 0);
        return GPU_FB_DEPTH_ATTACHMENT;
      case GPU_DEPTH24_STENCIL8:
      case GPU_DEPTH32F_STENCIL8:
        BLI_assert(slot == 0);
        return GPU_FB_DEPTH_STENCIL_ATTACHMENT;
      default:
        return GPU_FB_COLOR_ATTACHMENT0 + slot;
    }
  }
};

#undef DEBUG_NAME_LEN

}  // namespace gpu
}  // namespace blender
