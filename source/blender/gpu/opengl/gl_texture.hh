
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
 *
 * GPU Framebuffer
 * - this is a wrapper for an OpenGL framebuffer object (FBO). in practice
 *   multiple FBO's may be created.
 * - actual FBO creation & config is deferred until GPU_framebuffer_bind or
 *   GPU_framebuffer_check_valid to allow creation & config while another
 *   opengl context is bound (since FBOs are not shared between ogl contexts).
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "BLI_assert.h"

#include "gpu_texture_private.hh"

#include "glew-mx.h"

namespace blender {
namespace gpu {

#if 0
class GLContext {
  /** Currently bound textures. Updated before drawing. */
  GLuint bound_textures[64];
  GLuint bound_samplers[64];
  /** All sampler objects. Last one is for icon sampling. */
  GLuint samplers[GPU_SAMPLER_MAX + 1];
};
#endif

class GLTexture : public Texture {
 private:
  /** Texture unit to which this texture is bound. */
  int slot = -1;
  /** Target to bind the texture to (GL_TEXTURE_1D, GL_TEXTURE_2D, etc...)*/
  GLenum target_ = -1;
  /** opengl identifier for texture. */
  GLuint tex_id_ = 0;
  /** Legacy workaround for texture copy. */
  GLuint copy_fb = 0;
  GPUContext *copy_fb_ctx = NULL;

 public:
  GLTexture(const char *name);
  ~GLTexture();

  void bind(int slot) override;
  void update(void *data) override;
  void update_sub(void *data, int offset[3], int size[3]) override;
  void generate_mipmap(void) override;
  void copy_to(Texture *tex) override;

  void swizzle_set(char swizzle_mask[4]) override;

  /* TODO(fclem) Legacy. Should be removed at some point. */
  uint gl_bindcode_get(void) override;

 private:
  void init(void);
};

static inline GLenum target_to_gl(eGPUTextureFlag target)
{
  switch (target & GPU_TEXTURE_TARGET) {
    case GPU_TEXTURE_1D:
      return GL_TEXTURE_1D;
    case GPU_TEXTURE_1D | GPU_TEXTURE_ARRAY:
      return GL_TEXTURE_1D_ARRAY;
    case GPU_TEXTURE_2D:
      return GL_TEXTURE_2D;
    case GPU_TEXTURE_2D | GPU_TEXTURE_ARRAY:
      return GL_TEXTURE_2D_ARRAY;
    case GPU_TEXTURE_3D:
      return GL_TEXTURE_3D;
    case GPU_TEXTURE_CUBE:
      return GL_TEXTURE_CUBE_MAP;
    case GPU_TEXTURE_CUBE | GPU_TEXTURE_ARRAY:
      return GL_TEXTURE_CUBE_MAP_ARRAY_ARB;
    case GPU_TEXTURE_BUFFER:
      return GL_TEXTURE_BUFFER;
    default:
      BLI_assert(0);
      return GPU_TEXTURE_1D;
  }
}

static inline GLenum swizzle_to_gl(const char swizzle)
{
  switch (swizzle) {
    default:
    case 'x':
    case 'r':
      return GL_RED;
    case 'y':
    case 'g':
      return GL_GREEN;
    case 'z':
    case 'b':
      return GL_BLUE;
    case 'w':
    case 'a':
      return GL_ALPHA;
    case '0':
      return GL_ZERO;
    case '1':
      return GL_ONE;
  }
}

static inline GLenum to_gl(eGPUDataFormat format)
{
  switch (format) {
    case GPU_DATA_FLOAT:
      return GL_FLOAT;
    case GPU_DATA_INT:
      return GL_INT;
    case GPU_DATA_UNSIGNED_INT:
      return GL_UNSIGNED_INT;
    case GPU_DATA_UNSIGNED_BYTE:
      return GL_UNSIGNED_BYTE;
    case GPU_DATA_UNSIGNED_INT_24_8:
      return GL_UNSIGNED_INT_24_8;
    case GPU_DATA_10_11_11_REV:
      return GL_UNSIGNED_INT_10F_11F_11F_REV;
    default:
      BLI_assert(!"Unhandled data format");
      return GL_FLOAT;
  }
}

/* Assume Unorm / Float target. Used with glReadPixels. */
static inline GLenum channel_len_to_gl(int channel_len)
{
  switch (channel_len) {
    case 1:
      return GL_RED;
    case 2:
      return GL_RG;
    case 3:
      return GL_RGB;
    case 4:
      return GL_RGBA;
    default:
      BLI_assert(!"Wrong number of texture channels");
      return GL_RED;
  }
}

}  // namespace gpu
}  // namespace blender
