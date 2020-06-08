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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_image_types.h"

#include "BLI_blenlib.h"
#include "BLI_math_base.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"

#include "GPU_batch.h"
#include "GPU_context.h"
#include "GPU_debug.h"
#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_framebuffer.h"
#include "GPU_glew.h"
#include "GPU_platform.h"
#include "GPU_texture.h"

#include "gpu_context_private.h"

static struct GPUTextureGlobal {
  /** Texture used in place of invalid textures (not loaded correctly, missing). */
  GPUTexture *invalid_tex_1D;
  GPUTexture *invalid_tex_2D;
  GPUTexture *invalid_tex_3D;
  /** Sampler objects used to replace internal texture parameters. */
  GLuint samplers[GPU_SAMPLER_MAX];
} GG = {NULL};

/* Maximum number of FBOs a texture can be attached to. */
#define GPU_TEX_MAX_FBO_ATTACHED 12

typedef enum eGPUTextureFormatFlag {
  GPU_FORMAT_DEPTH = (1 << 0),
  GPU_FORMAT_STENCIL = (1 << 1),
  GPU_FORMAT_INTEGER = (1 << 2),
  GPU_FORMAT_FLOAT = (1 << 3),

  GPU_FORMAT_1D = (1 << 10),
  GPU_FORMAT_2D = (1 << 11),
  GPU_FORMAT_3D = (1 << 12),
  GPU_FORMAT_CUBE = (1 << 13),
  GPU_FORMAT_ARRAY = (1 << 14),
} eGPUTextureFormatFlag;

/* GPUTexture */
struct GPUTexture {
  int w, h, d;        /* width/height/depth */
  int orig_w, orig_h; /* width/height (of source data), optional. */
  int number;         /* Texture unit to which this texture is bound. */
  int refcount;       /* reference count */
  GLenum target;      /* GL_TEXTURE_* */
  GLenum target_base; /* same as target, (but no multisample)
                       * use it for unbinding */
  GLuint bindcode;    /* opengl identifier for texture */

  eGPUTextureFormat format;
  eGPUTextureFormatFlag format_flag;
  eGPUSamplerState sampler_state; /* Internal Sampler state. */

  int mipmaps;    /* number of mipmaps */
  int components; /* number of color/alpha channels */
  int samples;    /* number of samples for multisamples textures. 0 if not multisample target */

  int fb_attachment[GPU_TEX_MAX_FBO_ATTACHED];
  GPUFrameBuffer *fb[GPU_TEX_MAX_FBO_ATTACHED];
  /* Legacy workaround for texture copy. */
  GLuint copy_fb;
  GPUContext *copy_fb_ctx;
};

static uint gpu_get_bytesize(eGPUTextureFormat data_type);
static void gpu_texture_framebuffer_ensure(GPUTexture *tex);

/* ------ Memory Management ------- */
/* Records every texture allocation / free
 * to estimate the Texture Pool Memory consumption */
static uint memory_usage;

static uint gpu_texture_memory_footprint_compute(GPUTexture *tex)
{
  uint memsize;
  const uint bytesize = gpu_get_bytesize(tex->format);
  const int samp = max_ii(tex->samples, 1);
  switch (tex->target_base) {
    case GL_TEXTURE_1D:
    case GL_TEXTURE_BUFFER:
      memsize = bytesize * tex->w * samp;
      break;
    case GL_TEXTURE_1D_ARRAY:
    case GL_TEXTURE_2D:
      memsize = bytesize * tex->w * tex->h * samp;
      break;
    case GL_TEXTURE_2D_ARRAY:
    case GL_TEXTURE_3D:
      memsize = bytesize * tex->w * tex->h * tex->d * samp;
      break;
    case GL_TEXTURE_CUBE_MAP:
      memsize = bytesize * 6 * tex->w * tex->h * samp;
      break;
    case GL_TEXTURE_CUBE_MAP_ARRAY_ARB:
      memsize = bytesize * 6 * tex->w * tex->h * tex->d * samp;
      break;
    default:
      BLI_assert(0);
      return 0;
  }
  if (tex->mipmaps != 0) {
    /* Just to get an idea of the memory used here is computed
     * as if the maximum number of mipmaps was generated. */
    memsize += memsize / 3;
  }

  return memsize;
}

static void gpu_texture_memory_footprint_add(GPUTexture *tex)
{
  memory_usage += gpu_texture_memory_footprint_compute(tex);
}

static void gpu_texture_memory_footprint_remove(GPUTexture *tex)
{
  memory_usage -= gpu_texture_memory_footprint_compute(tex);
}

uint GPU_texture_memory_usage_get(void)
{
  return memory_usage;
}

/* -------------------------------- */

static const char *gl_enum_to_str(GLenum e)
{
#define ENUM_TO_STRING(e) [GL_##e] = STRINGIFY_ARG(e)
  static const char *enum_strings[] = {
      ENUM_TO_STRING(TEXTURE_CUBE_MAP),
      ENUM_TO_STRING(TEXTURE_CUBE_MAP_ARRAY),
      ENUM_TO_STRING(TEXTURE_2D),
      ENUM_TO_STRING(TEXTURE_2D_ARRAY),
      ENUM_TO_STRING(TEXTURE_1D),
      ENUM_TO_STRING(TEXTURE_1D_ARRAY),
      ENUM_TO_STRING(TEXTURE_3D),
      ENUM_TO_STRING(TEXTURE_2D_MULTISAMPLE),
      ENUM_TO_STRING(RGBA32F),
      ENUM_TO_STRING(RGBA16F),
      ENUM_TO_STRING(RGBA16UI),
      ENUM_TO_STRING(RGBA16I),
      ENUM_TO_STRING(RGBA16),
      ENUM_TO_STRING(RGBA8UI),
      ENUM_TO_STRING(RGBA8I),
      ENUM_TO_STRING(RGBA8),
      ENUM_TO_STRING(RGB16F),
      ENUM_TO_STRING(RG32F),
      ENUM_TO_STRING(RG16F),
      ENUM_TO_STRING(RG16UI),
      ENUM_TO_STRING(RG16I),
      ENUM_TO_STRING(RG16),
      ENUM_TO_STRING(RG8UI),
      ENUM_TO_STRING(RG8I),
      ENUM_TO_STRING(RG8),
      ENUM_TO_STRING(R8UI),
      ENUM_TO_STRING(R8I),
      ENUM_TO_STRING(R8),
      ENUM_TO_STRING(R32F),
      ENUM_TO_STRING(R32UI),
      ENUM_TO_STRING(R32I),
      ENUM_TO_STRING(R16F),
      ENUM_TO_STRING(R16UI),
      ENUM_TO_STRING(R16I),
      ENUM_TO_STRING(R16),
      ENUM_TO_STRING(R11F_G11F_B10F),
      ENUM_TO_STRING(SRGB8_ALPHA8),
      ENUM_TO_STRING(DEPTH24_STENCIL8),
      ENUM_TO_STRING(DEPTH32F_STENCIL8),
      ENUM_TO_STRING(DEPTH_COMPONENT32F),
      ENUM_TO_STRING(DEPTH_COMPONENT24),
      ENUM_TO_STRING(DEPTH_COMPONENT16),
  };
#undef ENUM_TO_STRING

  return enum_strings[e];
}

static int gpu_get_component_count(eGPUTextureFormat format)
{
  switch (format) {
    case GPU_RGBA8:
    case GPU_RGBA8UI:
    case GPU_RGBA16F:
    case GPU_RGBA16:
    case GPU_RGBA32F:
    case GPU_SRGB8_A8:
      return 4;
    case GPU_RGB16F:
    case GPU_R11F_G11F_B10F:
      return 3;
    case GPU_RG8:
    case GPU_RG16:
    case GPU_RG16F:
    case GPU_RG16I:
    case GPU_RG16UI:
    case GPU_RG32F:
      return 2;
    default:
      return 1;
  }
}

static uint gpu_get_data_format_bytesize(int comp, eGPUDataFormat data_format)
{
  switch (data_format) {
    case GPU_DATA_FLOAT:
      return sizeof(float) * comp;
    case GPU_DATA_INT:
    case GPU_DATA_UNSIGNED_INT:
      return sizeof(int) * comp;
    case GPU_DATA_UNSIGNED_INT_24_8:
    case GPU_DATA_10_11_11_REV:
      return sizeof(int);
    case GPU_DATA_UNSIGNED_BYTE:
      return sizeof(char) * comp;
    default:
      BLI_assert(0);
      return 0;
  }
}

/* Definitely not complete, edit according to the gl specification. */
static void gpu_validate_data_format(eGPUTextureFormat tex_format, eGPUDataFormat data_format)
{
  (void)data_format;

  if (ELEM(tex_format, GPU_DEPTH_COMPONENT24, GPU_DEPTH_COMPONENT16, GPU_DEPTH_COMPONENT32F)) {
    BLI_assert(data_format == GPU_DATA_FLOAT);
  }
  else if (ELEM(tex_format, GPU_DEPTH24_STENCIL8, GPU_DEPTH32F_STENCIL8)) {
    BLI_assert(data_format == GPU_DATA_UNSIGNED_INT_24_8);
  }
  else {
    /* Integer formats */
    if (ELEM(tex_format, GPU_RG16I, GPU_R16I, GPU_RG16UI, GPU_R16UI, GPU_R8UI, GPU_R32UI)) {
      if (ELEM(tex_format, GPU_R8UI, GPU_R16UI, GPU_RG16UI, GPU_R32UI)) {
        BLI_assert(data_format == GPU_DATA_UNSIGNED_INT);
      }
      else {
        BLI_assert(data_format == GPU_DATA_INT);
      }
    }
    /* Byte formats */
    else if (ELEM(tex_format, GPU_R8, GPU_RG8, GPU_RGBA8, GPU_RGBA8UI, GPU_SRGB8_A8)) {
      BLI_assert(ELEM(data_format, GPU_DATA_UNSIGNED_BYTE, GPU_DATA_FLOAT));
    }
    /* Special case */
    else if (ELEM(tex_format, GPU_R11F_G11F_B10F)) {
      BLI_assert(ELEM(data_format, GPU_DATA_10_11_11_REV, GPU_DATA_FLOAT));
    }
    /* Float formats */
    else {
      BLI_assert(ELEM(data_format, GPU_DATA_FLOAT));
    }
  }
}

static eGPUDataFormat gpu_get_data_format_from_tex_format(eGPUTextureFormat tex_format)
{
  if (ELEM(tex_format, GPU_DEPTH_COMPONENT24, GPU_DEPTH_COMPONENT16, GPU_DEPTH_COMPONENT32F)) {
    return GPU_DATA_FLOAT;
  }
  else if (ELEM(tex_format, GPU_DEPTH24_STENCIL8, GPU_DEPTH32F_STENCIL8)) {
    return GPU_DATA_UNSIGNED_INT_24_8;
  }
  else {
    /* Integer formats */
    if (ELEM(tex_format, GPU_RG16I, GPU_R16I, GPU_RG16UI, GPU_R8UI, GPU_R16UI, GPU_R32UI)) {
      if (ELEM(tex_format, GPU_R8UI, GPU_R16UI, GPU_RG16UI, GPU_R32UI)) {
        return GPU_DATA_UNSIGNED_INT;
      }
      else {
        return GPU_DATA_INT;
      }
    }
    /* Byte formats */
    else if (ELEM(tex_format, GPU_R8)) {
      return GPU_DATA_UNSIGNED_BYTE;
    }
    /* Special case */
    else if (ELEM(tex_format, GPU_R11F_G11F_B10F)) {
      return GPU_DATA_10_11_11_REV;
    }
    else {
      return GPU_DATA_FLOAT;
    }
  }
}

/* Definitely not complete, edit according to the gl specification. */
static GLenum gpu_get_gl_dataformat(eGPUTextureFormat data_type,
                                    eGPUTextureFormatFlag *format_flag)
{
  if (ELEM(data_type, GPU_DEPTH_COMPONENT24, GPU_DEPTH_COMPONENT16, GPU_DEPTH_COMPONENT32F)) {
    *format_flag |= GPU_FORMAT_DEPTH;
    return GL_DEPTH_COMPONENT;
  }
  else if (ELEM(data_type, GPU_DEPTH24_STENCIL8, GPU_DEPTH32F_STENCIL8)) {
    *format_flag |= GPU_FORMAT_DEPTH | GPU_FORMAT_STENCIL;
    return GL_DEPTH_STENCIL;
  }
  else {
    /* Integer formats */
    if (ELEM(data_type, GPU_R8UI, GPU_RG16I, GPU_R16I, GPU_RG16UI, GPU_R16UI, GPU_R32UI)) {
      *format_flag |= GPU_FORMAT_INTEGER;

      switch (gpu_get_component_count(data_type)) {
        case 1:
          return GL_RED_INTEGER;
          break;
        case 2:
          return GL_RG_INTEGER;
          break;
        case 3:
          return GL_RGB_INTEGER;
          break;
        case 4:
          return GL_RGBA_INTEGER;
          break;
      }
    }
    else if (ELEM(data_type, GPU_R8)) {
      *format_flag |= GPU_FORMAT_FLOAT;
      return GL_RED;
    }
    else {
      *format_flag |= GPU_FORMAT_FLOAT;

      switch (gpu_get_component_count(data_type)) {
        case 1:
          return GL_RED;
          break;
        case 2:
          return GL_RG;
          break;
        case 3:
          return GL_RGB;
          break;
        case 4:
          return GL_RGBA;
          break;
      }
    }
  }

  BLI_assert(0);
  *format_flag |= GPU_FORMAT_FLOAT;
  return GL_RGBA;
}

static uint gpu_get_bytesize(eGPUTextureFormat data_type)
{
  switch (data_type) {
    case GPU_RGBA32F:
      return 32;
    case GPU_RG32F:
    case GPU_RGBA16F:
    case GPU_RGBA16:
      return 16;
    case GPU_RGB16F:
      return 12;
    case GPU_DEPTH32F_STENCIL8: /* 32-bit depth, 8 bits stencil, and 24 unused bits. */
      return 8;
    case GPU_RG16F:
    case GPU_RG16I:
    case GPU_RG16UI:
    case GPU_RG16:
    case GPU_DEPTH24_STENCIL8:
    case GPU_DEPTH_COMPONENT32F:
    case GPU_RGBA8UI:
    case GPU_RGBA8:
    case GPU_SRGB8_A8:
    case GPU_R11F_G11F_B10F:
    case GPU_R32F:
    case GPU_R32UI:
    case GPU_R32I:
      return 4;
    case GPU_DEPTH_COMPONENT24:
      return 3;
    case GPU_DEPTH_COMPONENT16:
    case GPU_R16F:
    case GPU_R16UI:
    case GPU_R16I:
    case GPU_RG8:
    case GPU_R16:
      return 2;
    case GPU_R8:
    case GPU_R8UI:
      return 1;
    default:
      BLI_assert(!"Texture format incorrect or unsupported\n");
      return 0;
  }
}

static GLenum gpu_format_to_gl_internalformat(eGPUTextureFormat format)
{
  /* You can add any of the available type to this list
   * For available types see GPU_texture.h */
  switch (format) {
    /* Formats texture & renderbuffer */
    case GPU_RGBA8UI:
      return GL_RGBA8UI;
    case GPU_RGBA8I:
      return GL_RGBA8I;
    case GPU_RGBA8:
      return GL_RGBA8;
    case GPU_RGBA32UI:
      return GL_RGBA32UI;
    case GPU_RGBA32I:
      return GL_RGBA32I;
    case GPU_RGBA32F:
      return GL_RGBA32F;
    case GPU_RGBA16UI:
      return GL_RGBA16UI;
    case GPU_RGBA16I:
      return GL_RGBA16I;
    case GPU_RGBA16F:
      return GL_RGBA16F;
    case GPU_RGBA16:
      return GL_RGBA16;
    case GPU_RG8UI:
      return GL_RG8UI;
    case GPU_RG8I:
      return GL_RG8I;
    case GPU_RG8:
      return GL_RG8;
    case GPU_RG32UI:
      return GL_RG32UI;
    case GPU_RG32I:
      return GL_RG32I;
    case GPU_RG32F:
      return GL_RG32F;
    case GPU_RG16UI:
      return GL_RG16UI;
    case GPU_RG16I:
      return GL_RG16I;
    case GPU_RG16F:
      return GL_RG16F;
    case GPU_RG16:
      return GL_RG16;
    case GPU_R8UI:
      return GL_R8UI;
    case GPU_R8I:
      return GL_R8I;
    case GPU_R8:
      return GL_R8;
    case GPU_R32UI:
      return GL_R32UI;
    case GPU_R32I:
      return GL_R32I;
    case GPU_R32F:
      return GL_R32F;
    case GPU_R16UI:
      return GL_R16UI;
    case GPU_R16I:
      return GL_R16I;
    case GPU_R16F:
      return GL_R16F;
    case GPU_R16:
      return GL_R16;
    /* Special formats texture & renderbuffer */
    case GPU_R11F_G11F_B10F:
      return GL_R11F_G11F_B10F;
    case GPU_DEPTH32F_STENCIL8:
      return GL_DEPTH32F_STENCIL8;
    case GPU_DEPTH24_STENCIL8:
      return GL_DEPTH24_STENCIL8;
    case GPU_SRGB8_A8:
      return GL_SRGB8_ALPHA8;
    /* Texture only format */
    case GPU_RGB16F:
      return GL_RGB16F;
    /* Special formats texture only */
    /* ** Add Format here */
    /* Depth Formats */
    case GPU_DEPTH_COMPONENT32F:
      return GL_DEPTH_COMPONENT32F;
    case GPU_DEPTH_COMPONENT24:
      return GL_DEPTH_COMPONENT24;
    case GPU_DEPTH_COMPONENT16:
      return GL_DEPTH_COMPONENT16;
    default:
      BLI_assert(!"Texture format incorrect or unsupported\n");
      return 0;
  }
}

static eGPUTextureFormat gl_internalformat_to_gpu_format(const GLint glformat)
{
  /* You can add any of the available type to this list
   * For available types see GPU_texture.h */
  switch (glformat) {
    /* Formats texture & renderbuffer */
    case GL_RGBA8UI:
      return GPU_RGBA8UI;
    case GL_RGBA8I:
      return GPU_RGBA8I;
    case GL_RGBA8:
      return GPU_RGBA8;
    case GL_RGBA32UI:
      return GPU_RGBA32UI;
    case GL_RGBA32I:
      return GPU_RGBA32I;
    case GL_RGBA32F:
      return GPU_RGBA32F;
    case GL_RGBA16UI:
      return GPU_RGBA16UI;
    case GL_RGBA16I:
      return GPU_RGBA16I;
    case GL_RGBA16F:
      return GPU_RGBA16F;
    case GL_RGBA16:
      return GPU_RGBA16;
    case GL_RG8UI:
      return GPU_RG8UI;
    case GL_RG8I:
      return GPU_RG8I;
    case GL_RG8:
      return GPU_RG8;
    case GL_RG32UI:
      return GPU_RG32UI;
    case GL_RG32I:
      return GPU_RG32I;
    case GL_RG32F:
      return GPU_RG32F;
    case GL_RG16UI:
      return GPU_RG16UI;
    case GL_RG16I:
      return GPU_RG16I;
    case GL_RG16F:
      return GPU_RGBA32F;
    case GL_RG16:
      return GPU_RG16;
    case GL_R8UI:
      return GPU_R8UI;
    case GL_R8I:
      return GPU_R8I;
    case GL_R8:
      return GPU_R8;
    case GL_R32UI:
      return GPU_R32UI;
    case GL_R32I:
      return GPU_R32I;
    case GL_R32F:
      return GPU_R32F;
    case GL_R16UI:
      return GPU_R16UI;
    case GL_R16I:
      return GPU_R16I;
    case GL_R16F:
      return GPU_R16F;
    case GL_R16:
      return GPU_R16;
    /* Special formats texture & renderbuffer */
    case GL_R11F_G11F_B10F:
      return GPU_R11F_G11F_B10F;
    case GL_DEPTH32F_STENCIL8:
      return GPU_DEPTH32F_STENCIL8;
    case GL_DEPTH24_STENCIL8:
      return GPU_DEPTH24_STENCIL8;
    case GL_SRGB8_ALPHA8:
      return GPU_SRGB8_A8;
    /* Texture only format */
    case GL_RGB16F:
      return GPU_RGB16F;
    /* Special formats texture only */
    /* ** Add Format here */
    /* Depth Formats */
    case GL_DEPTH_COMPONENT32F:
      return GPU_DEPTH_COMPONENT32F;
    case GL_DEPTH_COMPONENT24:
      return GPU_DEPTH_COMPONENT24;
    case GL_DEPTH_COMPONENT16:
      return GPU_DEPTH_COMPONENT16;
    default:
      BLI_assert(!"Internal format incorrect or unsupported\n");
  }
  return -1;
}

static GLenum gpu_get_gl_datatype(eGPUDataFormat format)
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

static float *GPU_texture_rescale_3d(
    GPUTexture *tex, int w, int h, int d, int channels, const float *fpixels)
{
  const uint xf = w / tex->w, yf = h / tex->h, zf = d / tex->d;
  float *nfpixels = MEM_mallocN(channels * sizeof(float) * tex->w * tex->h * tex->d,
                                "GPUTexture Rescaled 3Dtex");

  if (nfpixels) {
    GPU_print_error_debug("You need to scale a 3D texture, feel the pain!");

    for (uint k = 0; k < tex->d; k++) {
      for (uint j = 0; j < tex->h; j++) {
        for (uint i = 0; i < tex->w; i++) {
          /* obviously doing nearest filtering here,
           * it's going to be slow in any case, let's not make it worse */
          float xb = i * xf;
          float yb = j * yf;
          float zb = k * zf;
          uint offset = k * (tex->w * tex->h) + i * tex->h + j;
          uint offset_orig = (zb) * (w * h) + (xb)*h + (yb);

          if (channels == 4) {
            nfpixels[offset * 4] = fpixels[offset_orig * 4];
            nfpixels[offset * 4 + 1] = fpixels[offset_orig * 4 + 1];
            nfpixels[offset * 4 + 2] = fpixels[offset_orig * 4 + 2];
            nfpixels[offset * 4 + 3] = fpixels[offset_orig * 4 + 3];
          }
          else {
            nfpixels[offset] = fpixels[offset_orig];
          }
        }
      }
    }
  }

  return nfpixels;
}

static bool gpu_texture_check_capacity(
    GPUTexture *tex, GLenum proxy, GLenum internalformat, GLenum data_format, GLenum data_type)
{
  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_WIN, GPU_DRIVER_ANY) ||
      GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_UNIX, GPU_DRIVER_OFFICIAL)) {
    /* Some AMD drivers have a faulty `GL_PROXY_TEXTURE_..` check.
     * (see T55888, T56185, T59351).
     * Checking with `GL_PROXY_TEXTURE_..` doesn't prevent `Out Of Memory` issue,
     * it just states that the OGL implementation can support the texture.
     * So manually check the maximum size and maximum number of layers. */
    switch (proxy) {
      case GL_PROXY_TEXTURE_2D_ARRAY:
        if ((tex->d < 0) || (tex->d > GPU_max_texture_layers())) {
          return false;
        }
        break;

      case GL_PROXY_TEXTURE_1D_ARRAY:
        if ((tex->h < 0) || (tex->h > GPU_max_texture_layers())) {
          return false;
        }
        break;
    }

    switch (proxy) {
      case GL_PROXY_TEXTURE_3D:
        if ((tex->d < 0) || (tex->d > GPU_max_texture_size())) {
          return false;
        }
        ATTR_FALLTHROUGH;

      case GL_PROXY_TEXTURE_2D:
      case GL_PROXY_TEXTURE_2D_ARRAY:
        if ((tex->h < 0) || (tex->h > GPU_max_texture_size())) {
          return false;
        }
        ATTR_FALLTHROUGH;

      case GL_PROXY_TEXTURE_1D:
      case GL_PROXY_TEXTURE_1D_ARRAY:
        if ((tex->w < 0) || (tex->w > GPU_max_texture_size())) {
          return false;
        }
        ATTR_FALLTHROUGH;
      default:
        break;
    }

    return true;
  }
  else {
    switch (proxy) {
      case GL_PROXY_TEXTURE_1D:
        glTexImage1D(proxy, 0, internalformat, tex->w, 0, data_format, data_type, NULL);
        break;
      case GL_PROXY_TEXTURE_1D_ARRAY:
      case GL_PROXY_TEXTURE_2D:
        glTexImage2D(proxy, 0, internalformat, tex->w, tex->h, 0, data_format, data_type, NULL);
        break;
      case GL_PROXY_TEXTURE_2D_ARRAY:
      case GL_PROXY_TEXTURE_3D:
        glTexImage3D(
            proxy, 0, internalformat, tex->w, tex->h, tex->d, 0, data_format, data_type, NULL);
        break;
    }
    int width = 0;
    glGetTexLevelParameteriv(proxy, 0, GL_TEXTURE_WIDTH, &width);

    return (width > 0);
  }
}

/* This tries to allocate video memory for a given texture
 * If alloc fails, lower the resolution until it fits. */
static bool gpu_texture_try_alloc(GPUTexture *tex,
                                  GLenum proxy,
                                  GLenum internalformat,
                                  GLenum data_format,
                                  GLenum data_type,
                                  int channels,
                                  bool try_rescale,
                                  const float *fpixels,
                                  float **rescaled_fpixels)
{
  bool ret;
  ret = gpu_texture_check_capacity(tex, proxy, internalformat, data_format, data_type);

  if (!ret && try_rescale) {
    BLI_assert(
        !ELEM(proxy, GL_PROXY_TEXTURE_1D_ARRAY, GL_PROXY_TEXTURE_2D_ARRAY));  // not implemented

    const int w = tex->w, h = tex->h, d = tex->d;

    /* Find largest texture possible */
    do {
      tex->w /= 2;
      tex->h /= 2;
      tex->d /= 2;

      /* really unlikely to happen but keep this just in case */
      if (tex->w == 0) {
        break;
      }
      if (tex->h == 0 && proxy != GL_PROXY_TEXTURE_1D) {
        break;
      }
      if (tex->d == 0 && proxy == GL_PROXY_TEXTURE_3D) {
        break;
      }

      ret = gpu_texture_check_capacity(tex, proxy, internalformat, data_format, data_type);
    } while (ret == false);

    /* Rescale */
    if (ret) {
      switch (proxy) {
        case GL_PROXY_TEXTURE_1D:
        case GL_PROXY_TEXTURE_2D:
          /* Do nothing for now */
          return false;
        case GL_PROXY_TEXTURE_3D:
          BLI_assert(data_type == GL_FLOAT);
          *rescaled_fpixels = GPU_texture_rescale_3d(tex, w, h, d, channels, fpixels);
          return (bool)*rescaled_fpixels;
      }
    }
  }

  return ret;
}

GPUTexture *GPU_texture_create_nD(int w,
                                  int h,
                                  int d,
                                  int n,
                                  const void *pixels,
                                  eGPUTextureFormat tex_format,
                                  eGPUDataFormat gpu_data_format,
                                  int samples,
                                  const bool can_rescale,
                                  char err_out[256])
{
  if (samples) {
    CLAMP_MAX(samples, GPU_max_color_texture_samples());
  }

  if ((tex_format == GPU_DEPTH24_STENCIL8) && GPU_depth_blitting_workaround()) {
    /* MacOS + Radeon Pro fails to blit depth on GPU_DEPTH24_STENCIL8
     * but works on GPU_DEPTH32F_STENCIL8. */
    tex_format = GPU_DEPTH32F_STENCIL8;
  }

  GPUTexture *tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
  tex->w = w;
  tex->h = h;
  tex->d = d;
  tex->samples = samples;
  tex->refcount = 1;
  tex->format = tex_format;
  tex->components = gpu_get_component_count(tex_format);
  tex->mipmaps = 0;
  tex->format_flag = 0;
  tex->number = -1;

  if (n == 2) {
    if (d == 0) {
      tex->target_base = tex->target = GL_TEXTURE_2D;
    }
    else {
      tex->target_base = tex->target = GL_TEXTURE_2D_ARRAY;
      tex->format_flag |= GPU_FORMAT_ARRAY;
    }
  }
  else if (n == 1) {
    if (h == 0) {
      tex->target_base = tex->target = GL_TEXTURE_1D;
    }
    else {
      tex->target_base = tex->target = GL_TEXTURE_1D_ARRAY;
      tex->format_flag |= GPU_FORMAT_ARRAY;
    }
  }
  else if (n == 3) {
    tex->target_base = tex->target = GL_TEXTURE_3D;
  }
  else {
    /* should never happen */
    MEM_freeN(tex);
    return NULL;
  }

  gpu_validate_data_format(tex_format, gpu_data_format);

  if (samples && n == 2 && d == 0) {
    tex->target = GL_TEXTURE_2D_MULTISAMPLE;
  }

  GLenum internalformat = gpu_format_to_gl_internalformat(tex_format);
  GLenum data_format = gpu_get_gl_dataformat(tex_format, &tex->format_flag);
  GLenum data_type = gpu_get_gl_datatype(gpu_data_format);

  /* Generate Texture object */
  tex->bindcode = GPU_tex_alloc();

  if (!tex->bindcode) {
    if (err_out) {
      BLI_strncpy(err_out, "GPUTexture: texture create failed\n", 256);
    }
    else {
      fprintf(stderr, "GPUTexture: texture create failed\n");
    }
    GPU_texture_free(tex);
    return NULL;
  }

  glBindTexture(tex->target, tex->bindcode);

  /* Check if texture fit in VRAM */
  GLenum proxy = GL_PROXY_TEXTURE_2D;

  if (n == 2) {
    if (d > 1) {
      proxy = GL_PROXY_TEXTURE_2D_ARRAY;
    }
  }
  else if (n == 1) {
    if (h == 0) {
      proxy = GL_PROXY_TEXTURE_1D;
    }
    else {
      proxy = GL_PROXY_TEXTURE_1D_ARRAY;
    }
  }
  else if (n == 3) {
    proxy = GL_PROXY_TEXTURE_3D;
  }

  float *rescaled_pixels = NULL;
  bool valid = gpu_texture_try_alloc(tex,
                                     proxy,
                                     internalformat,
                                     data_format,
                                     data_type,
                                     tex->components,
                                     can_rescale,
                                     pixels,
                                     &rescaled_pixels);

  if (G.debug & G_DEBUG_GPU || !valid) {
    printf("GPUTexture: create : %s, %s, w : %d, h : %d, d : %d, comp : %d, size : %.2f MiB\n",
           gl_enum_to_str(tex->target),
           gl_enum_to_str(internalformat),
           w,
           h,
           d,
           tex->components,
           gpu_texture_memory_footprint_compute(tex) / 1048576.0f);
  }

  if (!valid) {
    if (err_out) {
      BLI_strncpy(err_out, "GPUTexture: texture alloc failed\n", 256);
    }
    else {
      fprintf(stderr, "GPUTexture: texture alloc failed. Likely not enough Video Memory.\n");
      fprintf(stderr,
              "Current texture memory usage : %.2f MiB.\n",
              gpu_texture_memory_footprint_compute(tex) / 1048576.0f);
    }
    GPU_texture_free(tex);
    return NULL;
  }

  gpu_texture_memory_footprint_add(tex);

  /* Upload Texture */
  const float *pix = (rescaled_pixels) ? rescaled_pixels : pixels;

  if (tex->target == GL_TEXTURE_2D || tex->target == GL_TEXTURE_2D_MULTISAMPLE ||
      tex->target == GL_TEXTURE_1D_ARRAY) {
    if (samples) {
      glTexImage2DMultisample(tex->target, samples, internalformat, tex->w, tex->h, true);
      if (pix) {
        glTexSubImage2D(tex->target, 0, 0, 0, tex->w, tex->h, data_format, data_type, pix);
      }
    }
    else {
      glTexImage2D(tex->target, 0, internalformat, tex->w, tex->h, 0, data_format, data_type, pix);
    }
  }
  else if (tex->target == GL_TEXTURE_1D) {
    glTexImage1D(tex->target, 0, internalformat, tex->w, 0, data_format, data_type, pix);
  }
  else {
    glTexImage3D(
        tex->target, 0, internalformat, tex->w, tex->h, tex->d, 0, data_format, data_type, pix);
  }

  if (rescaled_pixels) {
    MEM_freeN(rescaled_pixels);
  }

  /* Texture Parameters */
  if (GPU_texture_stencil(tex) || /* Does not support filtering */
      GPU_texture_integer(tex) || /* Does not support filtering */
      GPU_texture_depth(tex)) {
    tex->sampler_state = GPU_SAMPLER_DEFAULT & ~GPU_SAMPLER_FILTER;
  }
  else {
    tex->sampler_state = GPU_SAMPLER_DEFAULT;
  }
  /* Avoid issue with incomplete textures. */
  glTexParameteri(tex->target_base, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  glBindTexture(tex->target, 0);

  return tex;
}

GPUTexture *GPU_texture_cube_create(int w,
                                    int d,
                                    const void *pixels,
                                    eGPUTextureFormat tex_format,
                                    eGPUDataFormat gpu_data_format,
                                    char err_out[256])
{
  GPUTexture *tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
  tex->w = w;
  tex->h = w;
  tex->d = d;
  tex->samples = 0;
  tex->refcount = 1;
  tex->format = tex_format;
  tex->components = gpu_get_component_count(tex_format);
  tex->mipmaps = 0;
  tex->format_flag = GPU_FORMAT_CUBE;
  tex->number = -1;

  if (d == 0) {
    tex->target_base = tex->target = GL_TEXTURE_CUBE_MAP;
  }
  else {
    tex->target_base = tex->target = GL_TEXTURE_CUBE_MAP_ARRAY_ARB;
    tex->format_flag |= GPU_FORMAT_ARRAY;

    if (!GPU_arb_texture_cube_map_array_is_supported()) {
      fprintf(stderr, "ERROR: Attempt to create a cubemap array without hardware support!\n");
      BLI_assert(0);
      GPU_texture_free(tex);
      return NULL;
    }

    if (d > GPU_max_texture_layers() / 6) {
      BLI_assert(0);
      GPU_texture_free(tex);
      return NULL;
    }
  }

  GLenum internalformat = gpu_format_to_gl_internalformat(tex_format);
  GLenum data_format = gpu_get_gl_dataformat(tex_format, &tex->format_flag);
  GLenum data_type = gpu_get_gl_datatype(gpu_data_format);

  /* Generate Texture object */
  tex->bindcode = GPU_tex_alloc();

  if (!tex->bindcode) {
    if (err_out) {
      BLI_strncpy(err_out, "GPUTexture: texture create failed\n", 256);
    }
    else {
      fprintf(stderr, "GPUTexture: texture create failed\n");
    }
    GPU_texture_free(tex);
    return NULL;
  }

  if (G.debug & G_DEBUG_GPU) {
    printf("GPUTexture: create : %s, %s, w : %d, h : %d, d : %d, comp : %d, size : %.2f MiB\n",
           gl_enum_to_str(tex->target),
           gl_enum_to_str(internalformat),
           w,
           w,
           d,
           tex->components,
           gpu_texture_memory_footprint_compute(tex) / 1048576.0f);
  }

  gpu_texture_memory_footprint_add(tex);

  glBindTexture(tex->target, tex->bindcode);

  /* Upload Texture */
  if (d == 0) {
    const char *pixels_px, *pixels_py, *pixels_pz, *pixels_nx, *pixels_ny, *pixels_nz;

    if (pixels) {
      size_t face_ofs = w * w * gpu_get_data_format_bytesize(tex->components, gpu_data_format);
      pixels_px = (char *)pixels + 0 * face_ofs;
      pixels_nx = (char *)pixels + 1 * face_ofs;
      pixels_py = (char *)pixels + 2 * face_ofs;
      pixels_ny = (char *)pixels + 3 * face_ofs;
      pixels_pz = (char *)pixels + 4 * face_ofs;
      pixels_nz = (char *)pixels + 5 * face_ofs;
    }
    else {
      pixels_px = pixels_py = pixels_pz = pixels_nx = pixels_ny = pixels_nz = NULL;
    }

    GLuint face = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    glTexImage2D(face++, 0, internalformat, tex->w, tex->h, 0, data_format, data_type, pixels_px);
    glTexImage2D(face++, 0, internalformat, tex->w, tex->h, 0, data_format, data_type, pixels_nx);
    glTexImage2D(face++, 0, internalformat, tex->w, tex->h, 0, data_format, data_type, pixels_py);
    glTexImage2D(face++, 0, internalformat, tex->w, tex->h, 0, data_format, data_type, pixels_ny);
    glTexImage2D(face++, 0, internalformat, tex->w, tex->h, 0, data_format, data_type, pixels_pz);
    glTexImage2D(face++, 0, internalformat, tex->w, tex->h, 0, data_format, data_type, pixels_nz);
  }
  else {
    glTexImage3D(tex->target,
                 0,
                 internalformat,
                 tex->w,
                 tex->h,
                 tex->d * 6,
                 0,
                 data_format,
                 data_type,
                 pixels);
  }

  /* Texture Parameters */
  if (GPU_texture_stencil(tex) || /* Does not support filtering */
      GPU_texture_integer(tex) || /* Does not support filtering */
      GPU_texture_depth(tex)) {
    tex->sampler_state = GPU_SAMPLER_DEFAULT & ~GPU_SAMPLER_FILTER;
  }
  else {
    tex->sampler_state = GPU_SAMPLER_DEFAULT;
  }
  /* Avoid issue with incomplete textures. */
  glTexParameteri(tex->target_base, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  glBindTexture(tex->target, 0);

  return tex;
}

/* Special buffer textures. tex_format must be compatible with the buffer content. */
GPUTexture *GPU_texture_create_buffer(eGPUTextureFormat tex_format, const GLuint buffer)
{
  GPUTexture *tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
  tex->refcount = 1;
  tex->format = tex_format;
  tex->components = gpu_get_component_count(tex_format);
  tex->format_flag = 0;
  tex->target_base = tex->target = GL_TEXTURE_BUFFER;
  tex->mipmaps = 0;
  tex->number = -1;

  GLenum internalformat = gpu_format_to_gl_internalformat(tex_format);

  gpu_get_gl_dataformat(tex_format, &tex->format_flag);

  if (!(ELEM(tex_format, GPU_R8, GPU_R16) || ELEM(tex_format, GPU_R16F, GPU_R32F) ||
        ELEM(tex_format, GPU_R8I, GPU_R16I, GPU_R32I) ||
        ELEM(tex_format, GPU_R8UI, GPU_R16UI, GPU_R32UI) || ELEM(tex_format, GPU_RG8, GPU_RG16) ||
        ELEM(tex_format, GPU_RG16F, GPU_RG32F) ||
        ELEM(tex_format, GPU_RG8I, GPU_RG16I, GPU_RG32I) ||
        ELEM(tex_format, GPU_RG8UI, GPU_RG16UI, GPU_RG32UI) ||
        /* Not available until gl 4.0 */
        // ELEM(tex_format, GPU_RGB32F, GPU_RGB32I, GPU_RGB32UI) ||
        ELEM(tex_format, GPU_RGBA8, GPU_RGBA16) || ELEM(tex_format, GPU_RGBA16F, GPU_RGBA32F) ||
        ELEM(tex_format, GPU_RGBA8I, GPU_RGBA16I, GPU_RGBA32I) ||
        ELEM(tex_format, GPU_RGBA8UI, GPU_RGBA16UI, GPU_RGBA32UI))) {
    fprintf(stderr, "GPUTexture: invalid format for texture buffer\n");
    GPU_texture_free(tex);
    return NULL;
  }

  /* Generate Texture object */
  tex->bindcode = GPU_tex_alloc();

  if (!tex->bindcode) {
    fprintf(stderr, "GPUTexture: texture create failed\n");
    GPU_texture_free(tex);
    BLI_assert(
        0 && "glGenTextures failed: Are you sure a valid OGL context is active on this thread?\n");
    return NULL;
  }

  glBindTexture(tex->target, tex->bindcode);
  glTexBuffer(tex->target, internalformat, buffer);
  glGetTexLevelParameteriv(tex->target, 0, GL_TEXTURE_WIDTH, &tex->w);
  glBindTexture(tex->target, 0);

  gpu_texture_memory_footprint_add(tex);

  return tex;
}

GPUTexture *GPU_texture_from_bindcode(int textarget, int bindcode)
{
  GPUTexture *tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
  tex->bindcode = bindcode;
  tex->refcount = 1;
  tex->target = textarget;
  tex->target_base = textarget;
  tex->samples = 0;
  tex->sampler_state = GPU_SAMPLER_REPEAT | GPU_SAMPLER_ANISO;
  if (GPU_get_mipmap()) {
    tex->sampler_state |= (GPU_SAMPLER_MIPMAP | GPU_SAMPLER_FILTER);
  }
  tex->number = -1;

  if (!glIsTexture(tex->bindcode)) {
    GPU_print_error_debug("Blender Texture Not Loaded");
  }
  else {
    GLint w, h, gl_format;
    GLenum gettarget;
    gettarget = (textarget == GL_TEXTURE_CUBE_MAP) ? GL_TEXTURE_CUBE_MAP_POSITIVE_X : textarget;

    glBindTexture(textarget, tex->bindcode);
    glGetTexLevelParameteriv(gettarget, 0, GL_TEXTURE_WIDTH, &w);
    glGetTexLevelParameteriv(gettarget, 0, GL_TEXTURE_HEIGHT, &h);
    glGetTexLevelParameteriv(gettarget, 0, GL_TEXTURE_INTERNAL_FORMAT, &gl_format);
    tex->w = w;
    tex->h = h;
    tex->format = gl_internalformat_to_gpu_format(gl_format);
    tex->components = gpu_get_component_count(tex->format);
    glBindTexture(textarget, 0);

    /* Depending on how this bindcode was obtained, the memory used here could
     * already have been computed.
     * But that is not the case currently. */
    gpu_texture_memory_footprint_add(tex);
  }

  return tex;
}

GPUTexture *GPU_texture_create_1d(int w,
                                  eGPUTextureFormat tex_format,
                                  const float *pixels,
                                  char err_out[256])
{
  BLI_assert(w > 0);
  eGPUDataFormat data_format = gpu_get_data_format_from_tex_format(tex_format);
  return GPU_texture_create_nD(w, 0, 0, 1, pixels, tex_format, data_format, 0, false, err_out);
}

GPUTexture *GPU_texture_create_1d_array(
    int w, int h, eGPUTextureFormat tex_format, const float *pixels, char err_out[256])
{
  BLI_assert(w > 0 && h > 0);
  eGPUDataFormat data_format = gpu_get_data_format_from_tex_format(tex_format);
  return GPU_texture_create_nD(w, h, 0, 1, pixels, tex_format, data_format, 0, false, err_out);
}

GPUTexture *GPU_texture_create_2d(
    int w, int h, eGPUTextureFormat tex_format, const float *pixels, char err_out[256])
{
  BLI_assert(w > 0 && h > 0);
  eGPUDataFormat data_format = gpu_get_data_format_from_tex_format(tex_format);
  return GPU_texture_create_nD(w, h, 0, 2, pixels, tex_format, data_format, 0, false, err_out);
}

GPUTexture *GPU_texture_create_2d_multisample(int w,
                                              int h,
                                              eGPUTextureFormat tex_format,
                                              const float *pixels,
                                              int samples,
                                              char err_out[256])
{
  BLI_assert(w > 0 && h > 0);
  eGPUDataFormat data_format = gpu_get_data_format_from_tex_format(tex_format);
  return GPU_texture_create_nD(
      w, h, 0, 2, pixels, tex_format, data_format, samples, false, err_out);
}

GPUTexture *GPU_texture_create_2d_array(
    int w, int h, int d, eGPUTextureFormat tex_format, const float *pixels, char err_out[256])
{
  BLI_assert(w > 0 && h > 0 && d > 0);
  eGPUDataFormat data_format = gpu_get_data_format_from_tex_format(tex_format);
  return GPU_texture_create_nD(w, h, d, 2, pixels, tex_format, data_format, 0, false, err_out);
}

GPUTexture *GPU_texture_create_3d(
    int w, int h, int d, eGPUTextureFormat tex_format, const float *pixels, char err_out[256])
{
  BLI_assert(w > 0 && h > 0 && d > 0);
  eGPUDataFormat data_format = gpu_get_data_format_from_tex_format(tex_format);
  return GPU_texture_create_nD(w, h, d, 3, pixels, tex_format, data_format, 0, true, err_out);
}

GPUTexture *GPU_texture_create_cube(int w,
                                    eGPUTextureFormat tex_format,
                                    const float *fpixels,
                                    char err_out[256])
{
  BLI_assert(w > 0);
  eGPUDataFormat data_format = gpu_get_data_format_from_tex_format(tex_format);
  return GPU_texture_cube_create(w, 0, fpixels, tex_format, data_format, err_out);
}

GPUTexture *GPU_texture_create_cube_array(
    int w, int d, eGPUTextureFormat tex_format, const float *fpixels, char err_out[256])
{
  BLI_assert(w > 0 && d > 0);
  eGPUDataFormat data_format = gpu_get_data_format_from_tex_format(tex_format);
  return GPU_texture_cube_create(w, d, fpixels, tex_format, data_format, err_out);
}

GPUTexture *GPU_texture_create_from_vertbuf(GPUVertBuf *vert)
{
  GPUVertFormat *format = &vert->format;
  GPUVertAttr *attr = &format->attrs[0];

  /* Detect incompatible cases (not supported by texture buffers) */
  BLI_assert(format->attr_len == 1 && vert->vbo_id != 0);
  BLI_assert(attr->comp_len != 3); /* Not until OGL 4.0 */
  BLI_assert(attr->comp_type != GPU_COMP_I10);
  BLI_assert(attr->fetch_mode != GPU_FETCH_INT_TO_FLOAT);

  uint byte_per_comp = attr->sz / attr->comp_len;
  bool is_uint = ELEM(attr->comp_type, GPU_COMP_U8, GPU_COMP_U16, GPU_COMP_U32);

  /* Cannot fetch signed int or 32bit ints as normalized float. */
  if (attr->fetch_mode == GPU_FETCH_INT_TO_FLOAT_UNIT) {
    BLI_assert(is_uint || byte_per_comp <= 2);
  }

  eGPUTextureFormat data_type;
  switch (attr->fetch_mode) {
    case GPU_FETCH_FLOAT:
      switch (attr->comp_len) {
        case 1:
          data_type = GPU_R32F;
          break;
        case 2:
          data_type = GPU_RG32F;
          break;
        // case 3: data_type = GPU_RGB32F; break; /* Not supported */
        default:
          data_type = GPU_RGBA32F;
          break;
      }
      break;
    case GPU_FETCH_INT:
      switch (attr->comp_len) {
        case 1:
          switch (byte_per_comp) {
            case 1:
              data_type = (is_uint) ? GPU_R8UI : GPU_R8I;
              break;
            case 2:
              data_type = (is_uint) ? GPU_R16UI : GPU_R16I;
              break;
            default:
              data_type = (is_uint) ? GPU_R32UI : GPU_R32I;
              break;
          }
          break;
        case 2:
          switch (byte_per_comp) {
            case 1:
              data_type = (is_uint) ? GPU_RG8UI : GPU_RG8I;
              break;
            case 2:
              data_type = (is_uint) ? GPU_RG16UI : GPU_RG16I;
              break;
            default:
              data_type = (is_uint) ? GPU_RG32UI : GPU_RG32I;
              break;
          }
          break;
        default:
          switch (byte_per_comp) {
            case 1:
              data_type = (is_uint) ? GPU_RGBA8UI : GPU_RGBA8I;
              break;
            case 2:
              data_type = (is_uint) ? GPU_RGBA16UI : GPU_RGBA16I;
              break;
            default:
              data_type = (is_uint) ? GPU_RGBA32UI : GPU_RGBA32I;
              break;
          }
          break;
      }
      break;
    case GPU_FETCH_INT_TO_FLOAT_UNIT:
      switch (attr->comp_len) {
        case 1:
          data_type = (byte_per_comp == 1) ? GPU_R8 : GPU_R16;
          break;
        case 2:
          data_type = (byte_per_comp == 1) ? GPU_RG8 : GPU_RG16;
          break;
        default:
          data_type = (byte_per_comp == 1) ? GPU_RGBA8 : GPU_RGBA16;
          break;
      }
      break;
    default:
      BLI_assert(0);
      return NULL;
  }

  return GPU_texture_create_buffer(data_type, vert->vbo_id);
}

void GPU_texture_add_mipmap(GPUTexture *tex,
                            eGPUDataFormat gpu_data_format,
                            int miplvl,
                            const void *pixels)
{
  BLI_assert((int)tex->format > -1);
  BLI_assert(tex->components > -1);
  BLI_assert(miplvl > tex->mipmaps);

  gpu_validate_data_format(tex->format, gpu_data_format);

  GLenum internalformat = gpu_format_to_gl_internalformat(tex->format);
  GLenum data_format = gpu_get_gl_dataformat(tex->format, &tex->format_flag);
  GLenum data_type = gpu_get_gl_datatype(gpu_data_format);

  glBindTexture(tex->target, tex->bindcode);

  int size[3];
  GPU_texture_get_mipmap_size(tex, miplvl, size);

  switch (tex->target) {
    case GL_TEXTURE_1D:
      glTexImage1D(
          tex->target, miplvl, internalformat, size[0], 0, data_format, data_type, pixels);
      break;
    case GL_TEXTURE_2D:
    case GL_TEXTURE_1D_ARRAY:
      glTexImage2D(tex->target,
                   miplvl,
                   internalformat,
                   size[0],
                   size[1],
                   0,
                   data_format,
                   data_type,
                   pixels);
      break;
    case GL_TEXTURE_3D:
    case GL_TEXTURE_2D_ARRAY:
    case GL_TEXTURE_CUBE_MAP_ARRAY_ARB:
      glTexImage3D(tex->target,
                   miplvl,
                   internalformat,
                   size[0],
                   size[1],
                   size[2],
                   0,
                   data_format,
                   data_type,
                   pixels);
      break;
    case GL_TEXTURE_2D_MULTISAMPLE:
      /* Multisample textures cannot have mipmaps. */
    default:
      BLI_assert(!"tex->target mode not supported");
  }

  tex->mipmaps = miplvl;
  glTexParameteri(GPU_texture_target(tex), GL_TEXTURE_MAX_LEVEL, miplvl);

  glBindTexture(tex->target, 0);
}

void GPU_texture_update_sub(GPUTexture *tex,
                            eGPUDataFormat gpu_data_format,
                            const void *pixels,
                            int offset_x,
                            int offset_y,
                            int offset_z,
                            int width,
                            int height,
                            int depth)
{
  BLI_assert((int)tex->format > -1);
  BLI_assert(tex->components > -1);

  const uint bytesize = gpu_get_bytesize(tex->format);
  GLenum data_format = gpu_get_gl_dataformat(tex->format, &tex->format_flag);
  GLenum data_type = gpu_get_gl_datatype(gpu_data_format);
  GLint alignment;

  /* The default pack size for textures is 4, which won't work for byte based textures */
  if (bytesize == 1) {
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  }

  glBindTexture(tex->target, tex->bindcode);
  switch (tex->target) {
    case GL_TEXTURE_1D:
      glTexSubImage1D(tex->target, 0, offset_x, width, data_format, data_type, pixels);
      break;
    case GL_TEXTURE_2D:
    case GL_TEXTURE_2D_MULTISAMPLE:
    case GL_TEXTURE_1D_ARRAY:
      glTexSubImage2D(
          tex->target, 0, offset_x, offset_y, width, height, data_format, data_type, pixels);
      break;
    case GL_TEXTURE_3D:
    case GL_TEXTURE_2D_ARRAY:
      glTexSubImage3D(tex->target,
                      0,
                      offset_x,
                      offset_y,
                      offset_z,
                      width,
                      height,
                      depth,
                      data_format,
                      data_type,
                      pixels);
      break;
    default:
      BLI_assert(!"tex->target mode not supported");
  }

  if (bytesize == 1) {
    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
  }

  glBindTexture(tex->target, 0);
}

void *GPU_texture_read(GPUTexture *tex, eGPUDataFormat gpu_data_format, int miplvl)
{
  BLI_assert(miplvl <= tex->mipmaps);

  int size[3] = {0, 0, 0};
  GPU_texture_get_mipmap_size(tex, miplvl, size);

  gpu_validate_data_format(tex->format, gpu_data_format);

  size_t samples_count = max_ii(1, tex->samples);
  samples_count *= size[0];
  samples_count *= max_ii(1, size[1]);
  samples_count *= max_ii(1, size[2]);
  samples_count *= (GPU_texture_cube(tex) && !GPU_texture_array(tex)) ? 6 : 1;

  size_t buf_size = samples_count * gpu_get_data_format_bytesize(tex->components, gpu_data_format);

  /* AMD Pro driver have a bug that write 8 bytes past buffer size
   * if the texture is big. (see T66573) */
  void *buf = MEM_mallocN(buf_size + 8, "GPU_texture_read");

  GLenum data_format = gpu_get_gl_dataformat(tex->format, &tex->format_flag);
  GLenum data_type = gpu_get_gl_datatype(gpu_data_format);

  glBindTexture(tex->target, tex->bindcode);

  if (GPU_texture_cube(tex) && !GPU_texture_array(tex)) {
    int cube_face_size = buf_size / 6;
    for (int i = 0; i < 6; i++) {
      glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                    miplvl,
                    data_format,
                    data_type,
                    ((char *)buf) + cube_face_size * i);
    }
  }
  else {
    glGetTexImage(tex->target, miplvl, data_format, data_type, buf);
  }

  glBindTexture(tex->target, 0);

  return buf;
}

void GPU_texture_clear(GPUTexture *tex, eGPUDataFormat gpu_data_format, const void *color)
{
  BLI_assert(color != NULL); /* Do not accept NULL as parameter. */
  gpu_validate_data_format(tex->format, gpu_data_format);

  if (false && GLEW_ARB_clear_texture) {
    GLenum data_type = gpu_get_gl_datatype(gpu_data_format);
    GLenum data_format = gpu_get_gl_dataformat(tex->format, &tex->format_flag);
    glClearTexImage(tex->bindcode, 0, data_format, data_type, color);

    if (GPU_texture_stencil(tex) && GPU_texture_depth(tex)) {
      /* TODO(clem) implement in fallback. */
      BLI_assert(0);
    }
    else if (GPU_texture_depth(tex)) {
      switch (gpu_data_format) {
        case GPU_DATA_FLOAT:
        case GPU_DATA_UNSIGNED_INT:
          break;
        default:
          /* TODO(clem) implement in fallback. */
          BLI_assert(0);
          break;
      }
    }
    else {
      switch (gpu_data_format) {
        case GPU_DATA_FLOAT:
        case GPU_DATA_UNSIGNED_INT:
        case GPU_DATA_UNSIGNED_BYTE:
          break;
        default:
          /* TODO(clem) implement in fallback. */
          BLI_assert(0);
          break;
      }
    }
  }
  else {
    /* Fallback for older GL. */
    GPUFrameBuffer *prev_fb = GPU_framebuffer_active_get();

    gpu_texture_framebuffer_ensure(tex);
    /* This means that this function can only be used in one context for each texture. */
    BLI_assert(tex->copy_fb_ctx == GPU_context_active_get());

    glBindFramebuffer(GL_FRAMEBUFFER, tex->copy_fb);
    glViewport(0, 0, tex->w, tex->h);

    /* Watch: Write mask could prevent the clear.
     * glClearTexImage does not change the state so we don't do it here either. */
    if (GPU_texture_stencil(tex) && GPU_texture_depth(tex)) {
      /* TODO(clem) implement. */
      BLI_assert(0);
    }
    else if (GPU_texture_depth(tex)) {
      float depth;
      switch (gpu_data_format) {
        case GPU_DATA_FLOAT: {
          depth = *(float *)color;
          break;
        }
        case GPU_DATA_UNSIGNED_INT: {
          depth = *(uint *)color / (float)UINT_MAX;
          break;
        }
        default:
          BLI_assert(!"Unhandled data format");
          depth = 0.0f;
          break;
      }
      glClearDepth(depth);
      glClear(GL_DEPTH_BUFFER_BIT);
    }
    else {
      float r, g, b, a;
      switch (gpu_data_format) {
        case GPU_DATA_FLOAT: {
          float *f_color = (float *)color;
          r = f_color[0];
          g = (tex->components > 1) ? f_color[1] : 0.0f;
          b = (tex->components > 2) ? f_color[2] : 0.0f;
          a = (tex->components > 3) ? f_color[3] : 0.0f;
          break;
        }
        case GPU_DATA_UNSIGNED_INT: {
          uint *u_color = (uint *)color;
          r = u_color[0] / (float)UINT_MAX;
          g = (tex->components > 1) ? u_color[1] / (float)UINT_MAX : 0.0f;
          b = (tex->components > 2) ? u_color[2] / (float)UINT_MAX : 0.0f;
          a = (tex->components > 3) ? u_color[3] / (float)UINT_MAX : 0.0f;
          break;
        }
        case GPU_DATA_UNSIGNED_BYTE: {
          uchar *ub_color = (uchar *)color;
          r = ub_color[0] / 255.0f;
          g = (tex->components > 1) ? ub_color[1] / 255.0f : 0.0f;
          b = (tex->components > 2) ? ub_color[2] / 255.0f : 0.0f;
          a = (tex->components > 3) ? ub_color[3] / 255.0f : 0.0f;
          break;
        }
        default:
          BLI_assert(!"Unhandled data format");
          r = g = b = a = 0.0f;
          break;
      }
      glClearColor(r, g, b, a);
      glClear(GL_COLOR_BUFFER_BIT);
    }

    if (prev_fb) {
      GPU_framebuffer_bind(prev_fb);
    }
  }
}

void GPU_texture_update(GPUTexture *tex, eGPUDataFormat data_format, const void *pixels)
{
  GPU_texture_update_sub(tex, data_format, pixels, 0, 0, 0, tex->w, tex->h, tex->d);
}

void GPU_invalid_tex_init(void)
{
  memory_usage = 0;
  const float color[4] = {1.0f, 0.0f, 1.0f, 1.0f};
  GG.invalid_tex_1D = GPU_texture_create_1d(1, GPU_RGBA8, color, NULL);
  GG.invalid_tex_2D = GPU_texture_create_2d(1, 1, GPU_RGBA8, color, NULL);
  GG.invalid_tex_3D = GPU_texture_create_3d(1, 1, 1, GPU_RGBA8, color, NULL);
}

void GPU_invalid_tex_bind(int mode)
{
  switch (mode) {
    case GL_TEXTURE_1D:
      glBindTexture(GL_TEXTURE_1D, GG.invalid_tex_1D->bindcode);
      break;
    case GL_TEXTURE_2D:
      glBindTexture(GL_TEXTURE_2D, GG.invalid_tex_2D->bindcode);
      break;
    case GL_TEXTURE_3D:
      glBindTexture(GL_TEXTURE_3D, GG.invalid_tex_3D->bindcode);
      break;
  }
}

void GPU_invalid_tex_free(void)
{
  if (GG.invalid_tex_1D) {
    GPU_texture_free(GG.invalid_tex_1D);
  }
  if (GG.invalid_tex_2D) {
    GPU_texture_free(GG.invalid_tex_2D);
  }
  if (GG.invalid_tex_3D) {
    GPU_texture_free(GG.invalid_tex_3D);
  }
}

/* set_number is to save the the texture unit for setting texture parameters. */
void GPU_texture_bind_ex(GPUTexture *tex, eGPUSamplerState state, int unit, const bool set_number)
{
  BLI_assert(unit >= 0);

  if (unit >= GPU_max_textures()) {
    fprintf(stderr, "Not enough texture slots.\n");
    return;
  }

  if (G.debug & G_DEBUG) {
    for (int i = 0; i < GPU_TEX_MAX_FBO_ATTACHED; i++) {
      if (tex->fb[i] && GPU_framebuffer_bound(tex->fb[i])) {
        fprintf(stderr,
                "Feedback loop warning!: Attempting to bind "
                "texture attached to current framebuffer!\n");
        BLI_assert(0); /* Should never happen! */
        break;
      }
    }
  }

  if (set_number) {
    tex->number = unit;
  }

  glActiveTexture(GL_TEXTURE0 + unit);

  state = (state < GPU_SAMPLER_MAX) ? state : tex->sampler_state;

  if (tex->bindcode != 0) {
    glBindTexture(tex->target, tex->bindcode);
    glBindSampler(unit, GG.samplers[state]);
  }
  else {
    GPU_invalid_tex_bind(tex->target_base);
    glBindSampler(unit, 0);
  }
}

void GPU_texture_bind(GPUTexture *tex, int unit)
{
  GPU_texture_bind_ex(tex, GPU_SAMPLER_MAX, unit, true);
}

void GPU_texture_unbind(GPUTexture *tex)
{
  if (tex->number == -1) {
    return;
  }

  glActiveTexture(GL_TEXTURE0 + tex->number);
  glBindTexture(tex->target, 0);
  glBindSampler(tex->number, 0);
  tex->number = -1;
}

void GPU_texture_unbind_all(void)
{
  if (GLEW_ARB_multi_bind) {
    glBindTextures(0, GPU_max_textures(), NULL);
    glBindSamplers(0, GPU_max_textures(), NULL);
    return;
  }

  for (int i = 0; i < GPU_max_textures(); i++) {
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glBindTexture(GL_TEXTURE_1D, 0);
    glBindTexture(GL_TEXTURE_1D_ARRAY, 0);
    glBindTexture(GL_TEXTURE_3D, 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glBindTexture(GL_TEXTURE_BUFFER, 0);
    if (GPU_arb_texture_cube_map_array_is_supported()) {
      glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY_ARB, 0);
    }
    glBindSampler(i, 0);
  }

  glActiveTexture(GL_TEXTURE0);
}

#define WARN_NOT_BOUND(_tex) \
  { \
    if (_tex->number == -1) { \
      fprintf(stderr, "Warning : Trying to set parameter on a texture not bound.\n"); \
      BLI_assert(0); \
      return; \
    } \
  } \
  ((void)0)

void GPU_texture_generate_mipmap(GPUTexture *tex)
{
  WARN_NOT_BOUND(tex);

  gpu_texture_memory_footprint_remove(tex);
  int levels = 1 + floor(log2(max_ii(tex->w, tex->h)));

  glActiveTexture(GL_TEXTURE0 + tex->number);

  if (GPU_texture_depth(tex)) {
    /* Some drivers have bugs when using glGenerateMipmap with depth textures (see T56789).
     * In this case we just create a complete texture with mipmaps manually without
     * down-sampling. You must initialize the texture levels using other methods like
     * GPU_framebuffer_recursive_downsample(). */
    eGPUDataFormat data_format = gpu_get_data_format_from_tex_format(tex->format);
    for (int i = 1; i < levels; i++) {
      GPU_texture_add_mipmap(tex, data_format, i, NULL);
    }
    glBindTexture(tex->target, tex->bindcode);
  }
  else {
    glGenerateMipmap(tex->target_base);
  }

  tex->mipmaps = levels;
  gpu_texture_memory_footprint_add(tex);
}

static GLenum gpu_texture_default_attachment(GPUTexture *tex)
{
  return !GPU_texture_depth(tex) ?
             GL_COLOR_ATTACHMENT0 :
             (GPU_texture_stencil(tex) ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT);
}

static void gpu_texture_framebuffer_ensure(GPUTexture *tex)
{
  if (tex->copy_fb == 0) {
    tex->copy_fb = GPU_fbo_alloc();
    tex->copy_fb_ctx = GPU_context_active_get();

    GLenum attachment = gpu_texture_default_attachment(tex);

    glBindFramebuffer(GL_FRAMEBUFFER, tex->copy_fb);
    glFramebufferTexture(GL_FRAMEBUFFER, attachment, tex->bindcode, 0);
    if (!GPU_texture_depth(tex)) {
      glReadBuffer(GL_COLOR_ATTACHMENT0);
      glDrawBuffer(GL_COLOR_ATTACHMENT0);
    }
    BLI_assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
}

/* Copy a texture content to a similar texture. Only Mip 0 is copied. */
void GPU_texture_copy(GPUTexture *dst, GPUTexture *src)
{
  BLI_assert(dst->target == src->target);
  BLI_assert(dst->w == src->w);
  BLI_assert(dst->h == src->h);
  BLI_assert(!GPU_texture_cube(src) && !GPU_texture_cube(dst));
  /* TODO support array / 3D textures. */
  BLI_assert(dst->d == 0);
  BLI_assert(dst->format == src->format);

  if (GLEW_ARB_copy_image && !GPU_texture_copy_workaround()) {
    /* Opengl 4.3 */
    glCopyImageSubData(src->bindcode,
                       src->target,
                       0,
                       0,
                       0,
                       0,
                       dst->bindcode,
                       dst->target,
                       0,
                       0,
                       0,
                       0,
                       src->w,
                       src->h,
                       1);
  }
  else {
    /* Fallback for older GL. */
    GPUFrameBuffer *prev_fb = GPU_framebuffer_active_get();

    gpu_texture_framebuffer_ensure(src);
    gpu_texture_framebuffer_ensure(dst);

    /* This means that this function can only be used in one context for each texture. */
    BLI_assert(src->copy_fb_ctx == GPU_context_active_get());
    BLI_assert(dst->copy_fb_ctx == GPU_context_active_get());

    glBindFramebuffer(GL_READ_FRAMEBUFFER, src->copy_fb);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst->copy_fb);

    GLbitfield mask = 0;
    if (GPU_texture_stencil(src)) {
      mask |= GL_STENCIL_BUFFER_BIT;
    }
    if (GPU_texture_depth(src)) {
      mask |= GL_DEPTH_BUFFER_BIT;
    }
    else {
      mask |= GL_COLOR_BUFFER_BIT;
    }

    glBlitFramebuffer(0, 0, src->w, src->h, 0, 0, src->w, src->h, mask, GL_NEAREST);

    if (prev_fb) {
      GPU_framebuffer_bind(prev_fb);
    }
  }
}

void GPU_texture_compare_mode(GPUTexture *tex, bool use_compare)
{
  /* Could become an assertion ? (fclem) */
  if (!GPU_texture_depth(tex)) {
    return;
  }
  SET_FLAG_FROM_TEST(tex->sampler_state, use_compare, GPU_SAMPLER_COMPARE);
}

void GPU_texture_filter_mode(GPUTexture *tex, bool use_filter)
{
  /* Stencil and integer format does not support filtering. */
  BLI_assert(!use_filter || !(GPU_texture_stencil(tex) || GPU_texture_integer(tex)));

  SET_FLAG_FROM_TEST(tex->sampler_state, use_filter, GPU_SAMPLER_FILTER);
}

void GPU_texture_mipmap_mode(GPUTexture *tex, bool use_mipmap, bool use_filter)
{
  /* Stencil and integer format does not support filtering. */
  BLI_assert(!(use_filter || use_mipmap) ||
             !(GPU_texture_stencil(tex) || GPU_texture_integer(tex)));

  SET_FLAG_FROM_TEST(tex->sampler_state, use_mipmap, GPU_SAMPLER_MIPMAP);
  SET_FLAG_FROM_TEST(tex->sampler_state, use_filter, GPU_SAMPLER_FILTER);
}

void GPU_texture_wrap_mode(GPUTexture *tex, bool use_repeat, bool use_clamp)
{
  SET_FLAG_FROM_TEST(tex->sampler_state, use_repeat, GPU_SAMPLER_REPEAT);
  SET_FLAG_FROM_TEST(tex->sampler_state, !use_clamp, GPU_SAMPLER_CLAMP_BORDER);
}

void GPU_texture_swizzle_channel_auto(GPUTexture *tex, int channels)
{
  WARN_NOT_BOUND(tex);

  glActiveTexture(GL_TEXTURE0 + tex->number);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_SWIZZLE_R, GL_RED);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_SWIZZLE_G, (channels >= 2) ? GL_GREEN : GL_RED);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_SWIZZLE_B, (channels >= 3) ? GL_BLUE : GL_RED);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_SWIZZLE_A, (channels >= 4) ? GL_ALPHA : GL_ONE);
}

void GPU_texture_free(GPUTexture *tex)
{
  tex->refcount--;

  if (tex->refcount < 0) {
    fprintf(stderr, "GPUTexture: negative refcount\n");
  }

  if (tex->refcount == 0) {
    for (int i = 0; i < GPU_TEX_MAX_FBO_ATTACHED; i++) {
      if (tex->fb[i] != NULL) {
        GPU_framebuffer_texture_detach_slot(tex->fb[i], tex, tex->fb_attachment[i]);
      }
    }

    if (tex->bindcode) {
      GPU_tex_free(tex->bindcode);
    }
    if (tex->copy_fb) {
      GPU_fbo_free(tex->copy_fb, tex->copy_fb_ctx);
    }

    gpu_texture_memory_footprint_remove(tex);

    MEM_freeN(tex);
  }
}

void GPU_texture_ref(GPUTexture *tex)
{
  tex->refcount++;
}

int GPU_texture_target(const GPUTexture *tex)
{
  return tex->target;
}

int GPU_texture_width(const GPUTexture *tex)
{
  return tex->w;
}

int GPU_texture_height(const GPUTexture *tex)
{
  return tex->h;
}

int GPU_texture_orig_width(const GPUTexture *tex)
{
  return tex->orig_w;
}

int GPU_texture_orig_height(const GPUTexture *tex)
{
  return tex->orig_h;
}

void GPU_texture_orig_size_set(GPUTexture *tex, int w, int h)
{
  tex->orig_w = w;
  tex->orig_h = h;
}

int GPU_texture_layers(const GPUTexture *tex)
{
  return tex->d;
}

eGPUTextureFormat GPU_texture_format(const GPUTexture *tex)
{
  return tex->format;
}

int GPU_texture_samples(const GPUTexture *tex)
{
  return tex->samples;
}

bool GPU_texture_array(const GPUTexture *tex)
{
  return (tex->format_flag & GPU_FORMAT_ARRAY) != 0;
}

bool GPU_texture_depth(const GPUTexture *tex)
{
  return (tex->format_flag & GPU_FORMAT_DEPTH) != 0;
}

bool GPU_texture_stencil(const GPUTexture *tex)
{
  return (tex->format_flag & GPU_FORMAT_STENCIL) != 0;
}

bool GPU_texture_integer(const GPUTexture *tex)
{
  return (tex->format_flag & GPU_FORMAT_INTEGER) != 0;
}

bool GPU_texture_cube(const GPUTexture *tex)
{
  return (tex->format_flag & GPU_FORMAT_CUBE) != 0;
}

int GPU_texture_opengl_bindcode(const GPUTexture *tex)
{
  return tex->bindcode;
}

void GPU_texture_attach_framebuffer(GPUTexture *tex, GPUFrameBuffer *fb, int attachment)
{
  for (int i = 0; i < GPU_TEX_MAX_FBO_ATTACHED; i++) {
    if (tex->fb[i] == NULL) {
      tex->fb[i] = fb;
      tex->fb_attachment[i] = attachment;
      return;
    }
  }

  BLI_assert(!"Error: Texture: Not enough Framebuffer slots");
}

/* Return previous attachment point */
int GPU_texture_detach_framebuffer(GPUTexture *tex, GPUFrameBuffer *fb)
{
  for (int i = 0; i < GPU_TEX_MAX_FBO_ATTACHED; i++) {
    if (tex->fb[i] == fb) {
      tex->fb[i] = NULL;
      return tex->fb_attachment[i];
    }
  }

  BLI_assert(!"Error: Texture: Framebuffer is not attached");
  return 0;
}

void GPU_texture_get_mipmap_size(GPUTexture *tex, int lvl, int *size)
{
  /* TODO assert if lvl is below the limit of 1px in each dimension. */
  int div = 1 << lvl;
  size[0] = max_ii(1, tex->w / div);

  if (tex->target == GL_TEXTURE_1D_ARRAY) {
    size[1] = tex->h;
  }
  else if (tex->h > 0) {
    size[1] = max_ii(1, tex->h / div);
  }

  if (GPU_texture_array(tex)) {
    size[2] = tex->d;
    /* Return the number of face layers. */
    if (GPU_texture_cube(tex)) {
      size[2] *= 6;
    }
  }
  else if (tex->d > 0) {
    size[2] = max_ii(1, tex->d / div);
  }
}

/* -------------------------------------------------------------------- */
/** \name GPU Sampler Objects
 *
 * Simple wrapper around opengl sampler objects.
 * Override texture sampler state for one sampler unit only.
 * \{ */

void GPU_samplers_init(void)
{
  glGenSamplers(GPU_SAMPLER_MAX, GG.samplers);
  for (int i = 0; i < GPU_SAMPLER_MAX; i++) {
    eGPUSamplerState state = i;
    GLenum clamp_type = (state & GPU_SAMPLER_CLAMP_BORDER) ? GL_CLAMP_TO_BORDER : GL_CLAMP_TO_EDGE;
    GLenum wrap_s = (state & GPU_SAMPLER_REPEAT_S) ? GL_REPEAT : clamp_type;
    GLenum wrap_t = (state & GPU_SAMPLER_REPEAT_T) ? GL_REPEAT : clamp_type;
    GLenum wrap_r = (state & GPU_SAMPLER_REPEAT_R) ? GL_REPEAT : clamp_type;
    GLenum mag_filter = (state & GPU_SAMPLER_FILTER) ? GL_LINEAR : GL_NEAREST;
    GLenum min_filter = (state & GPU_SAMPLER_FILTER) ?
                            ((state & GPU_SAMPLER_MIPMAP) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR) :
                            ((state & GPU_SAMPLER_MIPMAP) ? GL_NEAREST_MIPMAP_LINEAR : GL_NEAREST);
    GLenum compare_mode = (state & GPU_SAMPLER_COMPARE) ? GL_COMPARE_REF_TO_TEXTURE : GL_NONE;
    float aniso_filter = ((state & GPU_SAMPLER_MIPMAP) && (state & GPU_SAMPLER_ANISO)) ?
                             GPU_get_anisotropic() :
                             1.0f;

    glSamplerParameteri(GG.samplers[i], GL_TEXTURE_WRAP_S, wrap_s);
    glSamplerParameteri(GG.samplers[i], GL_TEXTURE_WRAP_T, wrap_t);
    glSamplerParameteri(GG.samplers[i], GL_TEXTURE_WRAP_R, wrap_r);
    glSamplerParameteri(GG.samplers[i], GL_TEXTURE_MIN_FILTER, min_filter);
    glSamplerParameteri(GG.samplers[i], GL_TEXTURE_MAG_FILTER, mag_filter);
    glSamplerParameteri(GG.samplers[i], GL_TEXTURE_COMPARE_MODE, compare_mode);
    glSamplerParameteri(GG.samplers[i], GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    if (GLEW_EXT_texture_filter_anisotropic) {
      glSamplerParameterf(GG.samplers[i], GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso_filter);
    }

    /** Other states are left to default:
     * - GL_TEXTURE_BORDER_COLOR is {0, 0, 0, 0}.
     * - GL_TEXTURE_MIN_LOD is -1000.
     * - GL_TEXTURE_MAX_LOD is 1000.
     * - GL_TEXTURE_LOD_BIAS is 0.0f.
     **/
  }
}

void GPU_samplers_free(void)
{
  glDeleteSamplers(GPU_SAMPLER_MAX, GG.samplers);
}

/** \} */
