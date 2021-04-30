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

#include "GPU_vertex_buffer.h"

#include "gpu_framebuffer_private.hh"

namespace blender {
namespace gpu {

typedef enum eGPUTextureFormatFlag {
  GPU_FORMAT_DEPTH = (1 << 0),
  GPU_FORMAT_STENCIL = (1 << 1),
  GPU_FORMAT_INTEGER = (1 << 2),
  GPU_FORMAT_FLOAT = (1 << 3),
  GPU_FORMAT_COMPRESSED = (1 << 4),

  GPU_FORMAT_DEPTH_STENCIL = (GPU_FORMAT_DEPTH | GPU_FORMAT_STENCIL),
} eGPUTextureFormatFlag;

ENUM_OPERATORS(eGPUTextureFormatFlag, GPU_FORMAT_DEPTH_STENCIL)

typedef enum eGPUTextureType {
  GPU_TEXTURE_1D = (1 << 0),
  GPU_TEXTURE_2D = (1 << 1),
  GPU_TEXTURE_3D = (1 << 2),
  GPU_TEXTURE_CUBE = (1 << 3),
  GPU_TEXTURE_ARRAY = (1 << 4),
  GPU_TEXTURE_BUFFER = (1 << 5),

  GPU_TEXTURE_1D_ARRAY = (GPU_TEXTURE_1D | GPU_TEXTURE_ARRAY),
  GPU_TEXTURE_2D_ARRAY = (GPU_TEXTURE_2D | GPU_TEXTURE_ARRAY),
  GPU_TEXTURE_CUBE_ARRAY = (GPU_TEXTURE_CUBE | GPU_TEXTURE_ARRAY),
} eGPUTextureType;

ENUM_OPERATORS(eGPUTextureType, GPU_TEXTURE_CUBE_ARRAY)

#ifdef DEBUG
#  define DEBUG_NAME_LEN 64
#else
#  define DEBUG_NAME_LEN 8
#endif

/* Maximum number of FBOs a texture can be attached to. */
#define GPU_TEX_MAX_FBO_ATTACHED 16

/**
 * Implementation of Textures.
 * Base class which is then specialized for each implementation (GL, VK, ...).
 */
class Texture {
 public:
  /** Internal Sampler state. */
  eGPUSamplerState sampler_state = GPU_SAMPLER_DEFAULT;
  /** Reference counter. */
  int refcount = 1;
  /** Width & Height (of source data), optional. */
  int src_w = 0, src_h = 0;
#ifndef GPU_NO_USE_PY_REFERENCES
  /**
   * Reference of a pointer that needs to be cleaned when deallocating the texture.
   * Points to #BPyGPUTexture.tex
   */
  void **py_ref = nullptr;
#endif

 protected:
  /* ---- Texture format (immutable after init). ---- */
  /** Width & Height & Depth. For cube-map arrays, d is number of face-layers. */
  int w_, h_, d_;
  /** Internal data format. */
  eGPUTextureFormat format_;
  /** Format characteristics. */
  eGPUTextureFormatFlag format_flag_;
  /** Texture type. */
  eGPUTextureType type_;

  /** Number of mipmaps this texture has (Max miplvl). */
  /* TODO(fclem): Should become immutable and the need for mipmaps should be specified upfront. */
  int mipmaps_ = -1;
  /** For error checking */
  int mip_min_ = 0, mip_max_ = 0;

  /** For debugging */
  char name_[DEBUG_NAME_LEN];

  /** Frame-buffer references to update on deletion. */
  GPUAttachmentType fb_attachment_[GPU_TEX_MAX_FBO_ATTACHED];
  FrameBuffer *fb_[GPU_TEX_MAX_FBO_ATTACHED];

 public:
  Texture(const char *name);
  virtual ~Texture();

  /* Return true on success. */
  bool init_1D(int w, int layers, eGPUTextureFormat format);
  bool init_2D(int w, int h, int layers, eGPUTextureFormat format);
  bool init_3D(int w, int h, int d, eGPUTextureFormat format);
  bool init_cubemap(int w, int layers, eGPUTextureFormat format);
  bool init_buffer(GPUVertBuf *vbo, eGPUTextureFormat format);

  virtual void generate_mipmap(void) = 0;
  virtual void copy_to(Texture *tex) = 0;
  virtual void clear(eGPUDataFormat format, const void *data) = 0;
  virtual void swizzle_set(const char swizzle_mask[4]) = 0;
  virtual void mip_range_set(int min, int max) = 0;
  virtual void *read(int mip, eGPUDataFormat format) = 0;

  void attach_to(FrameBuffer *fb, GPUAttachmentType type);
  void detach_from(FrameBuffer *fb);
  void update(eGPUDataFormat format, const void *data);

  virtual void update_sub(
      int mip, int offset[3], int extent[3], eGPUDataFormat format, const void *data) = 0;

  /* TODO(fclem): Legacy. Should be removed at some point. */
  virtual uint gl_bindcode_get(void) const = 0;

  int width_get(void) const
  {
    return w_;
  }
  int height_get(void) const
  {
    return h_;
  }
  int depth_get(void) const
  {
    return d_;
  }

  void mip_size_get(int mip, int r_size[3]) const
  {
    /* TODO assert if lvl is below the limit of 1px in each dimension. */
    int div = 1 << mip;
    r_size[0] = max_ii(1, w_ / div);

    if (type_ == GPU_TEXTURE_1D_ARRAY) {
      r_size[1] = h_;
    }
    else if (h_ > 0) {
      r_size[1] = max_ii(1, h_ / div);
    }

    if (type_ & (GPU_TEXTURE_ARRAY | GPU_TEXTURE_CUBE)) {
      r_size[2] = d_;
    }
    else if (d_ > 0) {
      r_size[2] = max_ii(1, d_ / div);
    }
  }

  int mip_width_get(int mip) const
  {
    return max_ii(1, w_ / (1 << mip));
  }
  int mip_height_get(int mip) const
  {
    return (type_ == GPU_TEXTURE_1D_ARRAY) ? h_ : max_ii(1, h_ / (1 << mip));
  }
  int mip_depth_get(int mip) const
  {
    return (type_ & (GPU_TEXTURE_ARRAY | GPU_TEXTURE_CUBE)) ? d_ : max_ii(1, d_ / (1 << mip));
  }

  /* Return number of dimension taking the array type into account. */
  int dimensions_count(void) const
  {
    const int array = (type_ & GPU_TEXTURE_ARRAY) ? 1 : 0;
    switch (type_ & ~GPU_TEXTURE_ARRAY) {
      case GPU_TEXTURE_BUFFER:
        return 1;
      case GPU_TEXTURE_1D:
        return 1 + array;
      case GPU_TEXTURE_2D:
        return 2 + array;
      case GPU_TEXTURE_CUBE:
      case GPU_TEXTURE_3D:
      default:
        return 3;
    }
  }
  /* Return number of array layer (or face layer) for texture array or 1 for the others. */
  int layer_count(void) const
  {
    switch (type_) {
      case GPU_TEXTURE_1D_ARRAY:
        return h_;
      case GPU_TEXTURE_2D_ARRAY:
      case GPU_TEXTURE_CUBE_ARRAY:
        return d_;
      default:
        return 1;
    }
  }

  eGPUTextureFormat format_get(void) const
  {
    return format_;
  }
  eGPUTextureFormatFlag format_flag_get(void) const
  {
    return format_flag_;
  }
  eGPUTextureType type_get(void) const
  {
    return type_;
  }
  GPUAttachmentType attachment_type(int slot) const
  {
    switch (format_) {
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

 protected:
  virtual bool init_internal(void) = 0;
  virtual bool init_internal(GPUVertBuf *vbo) = 0;
};

/* Syntactic sugar. */
static inline GPUTexture *wrap(Texture *vert)
{
  return reinterpret_cast<GPUTexture *>(vert);
}
static inline Texture *unwrap(GPUTexture *vert)
{
  return reinterpret_cast<Texture *>(vert);
}
static inline const Texture *unwrap(const GPUTexture *vert)
{
  return reinterpret_cast<const Texture *>(vert);
}

#undef DEBUG_NAME_LEN

inline size_t to_bytesize(eGPUTextureFormat format)
{
  switch (format) {
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
    case GPU_RGB10_A2:
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
    case GPU_SRGB8_A8_DXT1:
    case GPU_SRGB8_A8_DXT3:
    case GPU_SRGB8_A8_DXT5:
    case GPU_RGBA8_DXT1:
    case GPU_RGBA8_DXT3:
    case GPU_RGBA8_DXT5:
      return 1; /* Incorrect but actual size is fractional. */
    default:
      BLI_assert(!"Texture format incorrect or unsupported\n");
      return 0;
  }
}

inline size_t to_block_size(eGPUTextureFormat data_type)
{
  switch (data_type) {
    case GPU_SRGB8_A8_DXT1:
    case GPU_RGBA8_DXT1:
      return 8;
    case GPU_SRGB8_A8_DXT3:
    case GPU_SRGB8_A8_DXT5:
    case GPU_RGBA8_DXT3:
    case GPU_RGBA8_DXT5:
      return 16;
    default:
      BLI_assert(!"Texture format is not a compressed format\n");
      return 0;
  }
}

inline eGPUTextureFormatFlag to_format_flag(eGPUTextureFormat format)
{
  switch (format) {
    case GPU_DEPTH_COMPONENT24:
    case GPU_DEPTH_COMPONENT16:
    case GPU_DEPTH_COMPONENT32F:
      return GPU_FORMAT_DEPTH;
    case GPU_DEPTH24_STENCIL8:
    case GPU_DEPTH32F_STENCIL8:
      return GPU_FORMAT_DEPTH_STENCIL;
    case GPU_R8UI:
    case GPU_RG16I:
    case GPU_R16I:
    case GPU_RG16UI:
    case GPU_R16UI:
    case GPU_R32UI:
      return GPU_FORMAT_INTEGER;
    case GPU_SRGB8_A8_DXT1:
    case GPU_SRGB8_A8_DXT3:
    case GPU_SRGB8_A8_DXT5:
    case GPU_RGBA8_DXT1:
    case GPU_RGBA8_DXT3:
    case GPU_RGBA8_DXT5:
      return GPU_FORMAT_COMPRESSED;
    default:
      return GPU_FORMAT_FLOAT;
  }
}

inline int to_component_len(eGPUTextureFormat format)
{
  switch (format) {
    case GPU_RGBA8:
    case GPU_RGBA8UI:
    case GPU_RGBA16F:
    case GPU_RGBA16:
    case GPU_RGBA32F:
    case GPU_SRGB8_A8:
    case GPU_RGB10_A2:
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

inline size_t to_bytesize(eGPUDataFormat data_format)
{
  switch (data_format) {
    case GPU_DATA_UBYTE:
      return 1;
    case GPU_DATA_FLOAT:
    case GPU_DATA_INT:
    case GPU_DATA_UINT:
      return 4;
    case GPU_DATA_UINT_24_8:
    case GPU_DATA_10_11_11_REV:
    case GPU_DATA_2_10_10_10_REV:
      return 4;
    default:
      BLI_assert(!"Data format incorrect or unsupported\n");
      return 0;
  }
}

inline size_t to_bytesize(eGPUTextureFormat tex_format, eGPUDataFormat data_format)
{
  return to_component_len(tex_format) * to_bytesize(data_format);
}

/* Definitely not complete, edit according to the gl specification. */
inline bool validate_data_format(eGPUTextureFormat tex_format, eGPUDataFormat data_format)
{
  switch (tex_format) {
    case GPU_DEPTH_COMPONENT24:
    case GPU_DEPTH_COMPONENT16:
    case GPU_DEPTH_COMPONENT32F:
      return data_format == GPU_DATA_FLOAT;
    case GPU_DEPTH24_STENCIL8:
    case GPU_DEPTH32F_STENCIL8:
      return data_format == GPU_DATA_UINT_24_8;
    case GPU_R8UI:
    case GPU_R16UI:
    case GPU_RG16UI:
    case GPU_R32UI:
      return data_format == GPU_DATA_UINT;
    case GPU_RG16I:
    case GPU_R16I:
      return data_format == GPU_DATA_INT;
    case GPU_R8:
    case GPU_RG8:
    case GPU_RGBA8:
    case GPU_RGBA8UI:
    case GPU_SRGB8_A8:
      return ELEM(data_format, GPU_DATA_UBYTE, GPU_DATA_FLOAT);
    case GPU_RGB10_A2:
      return ELEM(data_format, GPU_DATA_2_10_10_10_REV, GPU_DATA_FLOAT);
    case GPU_R11F_G11F_B10F:
      return ELEM(data_format, GPU_DATA_10_11_11_REV, GPU_DATA_FLOAT);
    default:
      return data_format == GPU_DATA_FLOAT;
  }
}

/* Definitely not complete, edit according to the gl specification. */
inline eGPUDataFormat to_data_format(eGPUTextureFormat tex_format)
{
  switch (tex_format) {
    case GPU_DEPTH_COMPONENT24:
    case GPU_DEPTH_COMPONENT16:
    case GPU_DEPTH_COMPONENT32F:
      return GPU_DATA_FLOAT;
    case GPU_DEPTH24_STENCIL8:
    case GPU_DEPTH32F_STENCIL8:
      return GPU_DATA_UINT_24_8;
    case GPU_R8UI:
    case GPU_R16UI:
    case GPU_RG16UI:
    case GPU_R32UI:
      return GPU_DATA_UINT;
    case GPU_RG16I:
    case GPU_R16I:
      return GPU_DATA_INT;
    case GPU_R8:
    case GPU_RG8:
    case GPU_RGBA8:
    case GPU_RGBA8UI:
    case GPU_SRGB8_A8:
      return GPU_DATA_UBYTE;
    case GPU_RGB10_A2:
      return GPU_DATA_2_10_10_10_REV;
    case GPU_R11F_G11F_B10F:
      return GPU_DATA_10_11_11_REV;
    default:
      return GPU_DATA_FLOAT;
  }
}

inline eGPUFrameBufferBits to_framebuffer_bits(eGPUTextureFormat tex_format)
{
  switch (tex_format) {
    case GPU_DEPTH_COMPONENT24:
    case GPU_DEPTH_COMPONENT16:
    case GPU_DEPTH_COMPONENT32F:
      return GPU_DEPTH_BIT;
    case GPU_DEPTH24_STENCIL8:
    case GPU_DEPTH32F_STENCIL8:
      return GPU_DEPTH_BIT | GPU_STENCIL_BIT;
    default:
      return GPU_COLOR_BIT;
  }
}

static inline eGPUTextureFormat to_texture_format(const GPUVertFormat *format)
{
  if (format->attr_len > 1 || format->attr_len == 0) {
    BLI_assert(!"Incorrect vertex format for buffer texture");
    return GPU_DEPTH_COMPONENT24;
  }
  switch (format->attrs[0].comp_len) {
    case 1:
      switch (format->attrs[0].comp_type) {
        case GPU_COMP_I8:
          return GPU_R8I;
        case GPU_COMP_U8:
          return GPU_R8UI;
        case GPU_COMP_I16:
          return GPU_R16I;
        case GPU_COMP_U16:
          return GPU_R16UI;
        case GPU_COMP_I32:
          return GPU_R32I;
        case GPU_COMP_U32:
          return GPU_R32UI;
        case GPU_COMP_F32:
          return GPU_R32F;
        default:
          break;
      }
      break;
    case 2:
      switch (format->attrs[0].comp_type) {
        case GPU_COMP_I8:
          return GPU_RG8I;
        case GPU_COMP_U8:
          return GPU_RG8UI;
        case GPU_COMP_I16:
          return GPU_RG16I;
        case GPU_COMP_U16:
          return GPU_RG16UI;
        case GPU_COMP_I32:
          return GPU_RG32I;
        case GPU_COMP_U32:
          return GPU_RG32UI;
        case GPU_COMP_F32:
          return GPU_RG32F;
        default:
          break;
      }
      break;
    case 3:
      /* Not supported until GL 4.0 */
      break;
    case 4:
      switch (format->attrs[0].comp_type) {
        case GPU_COMP_I8:
          return GPU_RGBA8I;
        case GPU_COMP_U8:
          return GPU_RGBA8UI;
        case GPU_COMP_I16:
          return GPU_RGBA16I;
        case GPU_COMP_U16:
          /* Note: Checking the fetch mode to select the right GPU texture format. This can be
           * added to other formats as well. */
          switch (format->attrs[0].fetch_mode) {
            case GPU_FETCH_INT:
              return GPU_RGBA16UI;
            case GPU_FETCH_INT_TO_FLOAT_UNIT:
              return GPU_RGBA16;
            case GPU_FETCH_INT_TO_FLOAT:
              return GPU_RGBA16F;
            case GPU_FETCH_FLOAT:
              return GPU_RGBA16F;
          }
        case GPU_COMP_I32:
          return GPU_RGBA32I;
        case GPU_COMP_U32:
          return GPU_RGBA32UI;
        case GPU_COMP_F32:
          return GPU_RGBA32F;
        default:
          break;
      }
      break;
    default:
      break;
  }
  BLI_assert(!"Unsupported vertex format for buffer texture");
  return GPU_DEPTH_COMPONENT24;
}

}  // namespace gpu
}  // namespace blender
