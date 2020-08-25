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

#include "MEM_guardedalloc.h"

#include "GPU_framebuffer.h"

#include "glew-mx.h" /* For GLuint. To remove. */

struct GPUTexture;

typedef enum {
  GPU_FB_DEPTH_ATTACHMENT = 0,
  GPU_FB_DEPTH_STENCIL_ATTACHMENT,
  GPU_FB_COLOR_ATTACHMENT0,
  GPU_FB_COLOR_ATTACHMENT1,
  GPU_FB_COLOR_ATTACHMENT2,
  GPU_FB_COLOR_ATTACHMENT3,
  GPU_FB_COLOR_ATTACHMENT4,
  GPU_FB_COLOR_ATTACHMENT5,
  /* Number of maximum output slots.
   * We support 6 outputs for now (usually we wouldn't need more to preserve fill rate). */
  /* Keep in mind that GL max is GL_MAX_DRAW_BUFFERS and is at least 8, corresponding to
   * the maximum number of COLOR attachments specified by glDrawBuffers. */
  GPU_FB_MAX_ATTACHEMENT,
} GPUAttachmentType;

namespace blender {
namespace gpu {

#define FOREACH_ATTACHMENT_RANGE(att, _start, _end) \
  for (GPUAttachmentType att = static_cast<GPUAttachmentType>(_start); att < _end; \
       att = static_cast<GPUAttachmentType>(att + 1))

#define GPU_FB_MAX_COLOR_ATTACHMENT (GPU_FB_MAX_ATTACHEMENT - GPU_FB_COLOR_ATTACHMENT0)

#define GPU_FB_DIRTY_DRAWBUFFER (1 << 15)

#define GPU_FB_ATTACHEMENT_IS_DIRTY(flag, type) ((flag & (1 << type)) != 0)
#define GPU_FB_ATTACHEMENT_SET_DIRTY(flag, type) (flag |= (1 << type))

class FrameBuffer {
 public:
  GPUContext *ctx;
  GLuint object;
  GPUAttachment attachments[GPU_FB_MAX_ATTACHEMENT];
  uint16_t dirty_flag;
  int width, height;
  bool multisample;
  /* TODO Check that we always use the right context when binding
   * (FBOs are not shared across ogl contexts). */
  // void *ctx;

 public:
  GPUTexture *depth_tex(void) const
  {
    if (attachments[GPU_FB_DEPTH_ATTACHMENT].tex) {
      return attachments[GPU_FB_DEPTH_ATTACHMENT].tex;
    }
    return attachments[GPU_FB_DEPTH_STENCIL_ATTACHMENT].tex;
  };

  GPUTexture *color_tex(int slot) const
  {
    return attachments[GPU_FB_COLOR_ATTACHMENT0 + slot].tex;
  };

  MEM_CXX_CLASS_ALLOC_FUNCS("FrameBuffer")
};

}  // namespace gpu
}  // namespace blender
