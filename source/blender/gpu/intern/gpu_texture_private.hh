/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_assert.h"
#include "BLI_enum_flags.hh"

#include "GPU_vertex_buffer.hh"

#include "gpu_framebuffer_private.hh"

namespace blender::gpu {

inline bool is_half_float(TextureFormat format)
{
  switch (format) {
    case TextureFormat::SFLOAT_16_16_16_16:
    case TextureFormat::SFLOAT_16_16_16:
    case TextureFormat::SFLOAT_16_16:
    case TextureFormat::SFLOAT_16:
      return true;
    default:
      return false;
  }
}

enum GPUTextureFormatFlag {
  /* The format has a depth component and can be used as depth attachment. */
  GPU_FORMAT_DEPTH = (1 << 0),
  /* The format has a stencil component and can be used as stencil attachment. */
  GPU_FORMAT_STENCIL = (1 << 1),
  /* The format represent non-normalized integers data, either signed or unsigned. */
  GPU_FORMAT_INTEGER = (1 << 2),
  /* The format is using normalized integers, either signed or unsigned. */
  GPU_FORMAT_NORMALIZED_INTEGER = (1 << 3),
  /* The format represent floating point data, either signed or unsigned. */
  GPU_FORMAT_FLOAT = (1 << 4),
  /* The format is using block compression. */
  GPU_FORMAT_COMPRESSED = (1 << 5),
  /* The format is using sRGB encoded storage. */
  GPU_FORMAT_SRGB = (1 << 6),
  /* The format can store negative values. */
  GPU_FORMAT_SIGNED = (1 << 7),

  GPU_FORMAT_DEPTH_STENCIL = (GPU_FORMAT_DEPTH | GPU_FORMAT_STENCIL),
};

ENUM_OPERATORS(GPUTextureFormatFlag)

enum GPUTextureType {
  GPU_TEXTURE_1D = (1 << 0),
  GPU_TEXTURE_2D = (1 << 1),
  GPU_TEXTURE_3D = (1 << 2),
  GPU_TEXTURE_CUBE = (1 << 3),
  GPU_TEXTURE_ARRAY = (1 << 4),
  GPU_TEXTURE_BUFFER = (1 << 5),

  GPU_TEXTURE_1D_ARRAY = (GPU_TEXTURE_1D | GPU_TEXTURE_ARRAY),
  GPU_TEXTURE_2D_ARRAY = (GPU_TEXTURE_2D | GPU_TEXTURE_ARRAY),
  GPU_TEXTURE_CUBE_ARRAY = (GPU_TEXTURE_CUBE | GPU_TEXTURE_ARRAY),
};

ENUM_OPERATORS(GPUTextureType)

/* Format types for samplers within the shader.
 * This covers the sampler format type permutations within GLSL/MSL. */
enum GPUSamplerFormat {
  GPU_SAMPLER_TYPE_FLOAT = 0,
  GPU_SAMPLER_TYPE_INT = 1,
  GPU_SAMPLER_TYPE_UINT = 2,
  /* Special case for depth, as these require differing dummy formats. */
  GPU_SAMPLER_TYPE_DEPTH = 3,
  GPU_SAMPLER_TYPE_MAX = 4
};

ENUM_OPERATORS(GPUSamplerFormat)

#ifndef NDEBUG
#  define DEBUG_NAME_LEN 64
#else
#  define DEBUG_NAME_LEN 8
#endif

/* Maximum number of image units. */
#define GPU_MAX_IMAGE 8

/* Maximum number of FBOs a texture can be attached to. */
#define GPU_TEX_MAX_FBO_ATTACHED 32

/**
 * Implementation of Textures.
 * Base class which is then specialized for each implementation (GL, VK, ...).
 */
class Texture {
 public:
  /** Internal Sampler state. */
  GPUSamplerState sampler_state = GPUSamplerState::default_sampler();
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
  TextureFormat format_;
  /** Format characteristics. */
  GPUTextureFormatFlag format_flag_;
  /** Texture type. */
  GPUTextureType type_;
  /** Texture usage flags. */
  eGPUTextureUsage gpu_image_usage_flags_;

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
  bool init_1D(int w, int layers, int mip_len, TextureFormat format);
  bool init_2D(int w, int h, int layers, int mip_len, TextureFormat format);
  bool init_3D(int w, int h, int d, int mip_len, TextureFormat format);
  bool init_cubemap(int w, int layers, int mip_len, TextureFormat format);
  bool init_buffer(VertBuf *vbo, TextureFormat format);
  bool init_view(Texture *src,
                 TextureFormat format,
                 GPUTextureType type,
                 int mip_start,
                 int mip_len,
                 int layer_start,
                 int layer_len,
                 bool cube_as_array,
                 bool use_stencil);

  virtual void generate_mipmap() = 0;
  virtual void copy_to(Texture *tex) = 0;
  virtual void clear(eGPUDataFormat format, const void *data) = 0;
  virtual void swizzle_set(const char swizzle_mask[4]) = 0;
  virtual void mip_range_set(int min, int max) = 0;
  virtual void *read(int mip, eGPUDataFormat format) = 0;

  void attach_to(FrameBuffer *fb, GPUAttachmentType type);
  void detach_from(FrameBuffer *fb);
  void update(eGPUDataFormat format, const void *data);

  void usage_set(eGPUTextureUsage usage_flags);

  virtual void update_sub(
      int mip, int offset[3], int extent[3], eGPUDataFormat format, const void *data) = 0;
  virtual void update_sub(int offset[3],
                          int extent[3],
                          eGPUDataFormat format,
                          GPUPixelBuffer *pixbuf) = 0;

  int width_get() const
  {
    return w_;
  }
  int height_get() const
  {
    return h_;
  }
  int depth_get() const
  {
    return d_;
  }
  eGPUTextureUsage usage_get() const
  {
    return gpu_image_usage_flags_;
  }

  void mip_size_get(int mip, int r_size[3]) const
  {
    /* TODO: assert if lvl is below the limit of 1px in each dimension. */
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
  int dimensions_count() const
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
  int layer_count() const
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

  int mip_count() const
  {
    return mipmaps_;
  }

  TextureFormat format_get() const
  {
    return format_;
  }
  GPUTextureFormatFlag format_flag_get() const
  {
    return format_flag_;
  }
  GPUTextureType type_get() const
  {
    return type_;
  }
  GPUAttachmentType attachment_type(int slot) const
  {
    switch (format_) {
      case TextureFormat::SFLOAT_32_DEPTH:
      case TextureFormat::UNORM_16_DEPTH:
        BLI_assert(slot == 0);
        return GPU_FB_DEPTH_ATTACHMENT;
      case TextureFormat::SFLOAT_32_DEPTH_UINT_8:
        BLI_assert(slot == 0);
        return GPU_FB_DEPTH_STENCIL_ATTACHMENT;
      default:
        /* Valid color attachment formats. */
        return GPU_FB_COLOR_ATTACHMENT0 + slot;

      case TextureFormat::SFLOAT_16_16_16:
      case TextureFormat::SNORM_16_16_16_16:
      case TextureFormat::SNORM_8_8_8_8:
      case TextureFormat::SFLOAT_32_32_32:
      case TextureFormat::SINT_32_32_32:
      case TextureFormat::UINT_32_32_32:
      case TextureFormat::SNORM_16_16_16:
      case TextureFormat::SINT_16_16_16:
      case TextureFormat::UINT_16_16_16:
      case TextureFormat::UNORM_16_16_16:
      case TextureFormat::SNORM_8_8_8:
      case TextureFormat::UNORM_8_8_8:
      case TextureFormat::SINT_8_8_8:
      case TextureFormat::UINT_8_8_8:
      case TextureFormat::SNORM_16_16:
      case TextureFormat::SNORM_8_8:
      case TextureFormat::SNORM_16:
      case TextureFormat::SNORM_8:
      case TextureFormat::SRGB_DXT1:
      case TextureFormat::SRGB_DXT3:
      case TextureFormat::SRGB_DXT5:
      case TextureFormat::SNORM_DXT1:
      case TextureFormat::SNORM_DXT3:
      case TextureFormat::SNORM_DXT5:
      case TextureFormat::SRGBA_8_8_8:
      case TextureFormat::UFLOAT_9_9_9_EXP_5:
        BLI_assert_msg(0, "Texture cannot be attached to a framebuffer because of its type");
        return GPU_FB_COLOR_ATTACHMENT0;
    }
  }

 protected:
  virtual bool init_internal() = 0;
  virtual bool init_internal(VertBuf *vbo) = 0;
  virtual bool init_internal(blender::gpu::Texture *src,
                             int mip_offset,
                             int layer_offset,
                             bool use_stencil) = 0;
};

/* GPU pixel Buffer. */
class PixelBuffer {
 protected:
  size_t size_ = 0;

 public:
  PixelBuffer(size_t size) : size_(size) {};
  virtual ~PixelBuffer() = default;

  virtual void *map() = 0;
  virtual void unmap() = 0;
  virtual GPUPixelBufferNativeHandle get_native_handle() = 0;
  virtual size_t get_size() = 0;
};

/* Syntactic sugar. */
static inline GPUPixelBuffer *wrap(PixelBuffer *pixbuf)
{
  return reinterpret_cast<GPUPixelBuffer *>(pixbuf);
}
static inline PixelBuffer *unwrap(GPUPixelBuffer *pixbuf)
{
  return reinterpret_cast<PixelBuffer *>(pixbuf);
}
static inline const PixelBuffer *unwrap(const GPUPixelBuffer *pixbuf)
{
  return reinterpret_cast<const PixelBuffer *>(pixbuf);
}

#undef DEBUG_NAME_LEN

inline size_t to_bytesize(TextureFormat format)
{
  return to_bytesize(DataFormat(format));
}

inline size_t to_block_size(TextureFormat data_type)
{
  switch (data_type) {
    case TextureFormat::SRGB_DXT1:
    case TextureFormat::SNORM_DXT1:
      return 8;
    case TextureFormat::SRGB_DXT3:
    case TextureFormat::SRGB_DXT5:
    case TextureFormat::SNORM_DXT3:
    case TextureFormat::SNORM_DXT5:
      return 16;
    default:
      BLI_assert_msg(0, "Texture format is not a compressed format");
      return 0;
  }
}

inline GPUTextureFormatFlag to_format_flag(TextureFormat format)
{
  switch (format) {
    /* Formats texture & render-buffer */
    case TextureFormat::UINT_8_8_8_8:
      return GPU_FORMAT_INTEGER;
    case TextureFormat::SINT_8_8_8_8:
      return GPU_FORMAT_INTEGER | GPU_FORMAT_SIGNED;
    case TextureFormat::UNORM_8_8_8_8:
      return GPU_FORMAT_NORMALIZED_INTEGER;
    case TextureFormat::UINT_32_32_32_32:
      return GPU_FORMAT_INTEGER;
    case TextureFormat::SINT_32_32_32_32:
      return GPU_FORMAT_INTEGER | GPU_FORMAT_SIGNED;
    case TextureFormat::SFLOAT_32_32_32_32:
      return GPU_FORMAT_FLOAT | GPU_FORMAT_SIGNED;
    case TextureFormat::UINT_16_16_16_16:
      return GPU_FORMAT_INTEGER;
    case TextureFormat::SINT_16_16_16_16:
      return GPU_FORMAT_INTEGER | GPU_FORMAT_SIGNED;
    case TextureFormat::SFLOAT_16_16_16_16:
      return GPU_FORMAT_FLOAT | GPU_FORMAT_SIGNED;
    case TextureFormat::UNORM_16_16_16_16:
      return GPU_FORMAT_NORMALIZED_INTEGER;
    case TextureFormat::UINT_8_8:
      return GPU_FORMAT_INTEGER;
    case TextureFormat::SINT_8_8:
      return GPU_FORMAT_INTEGER | GPU_FORMAT_SIGNED;
    case TextureFormat::UNORM_8_8:
      return GPU_FORMAT_NORMALIZED_INTEGER;
    case TextureFormat::UINT_32_32:
      return GPU_FORMAT_INTEGER;
    case TextureFormat::SINT_32_32:
      return GPU_FORMAT_INTEGER | GPU_FORMAT_SIGNED;
    case TextureFormat::SFLOAT_32_32:
      return GPU_FORMAT_FLOAT | GPU_FORMAT_SIGNED;
    case TextureFormat::UINT_16_16:
      return GPU_FORMAT_INTEGER;
    case TextureFormat::SINT_16_16:
      return GPU_FORMAT_INTEGER | GPU_FORMAT_SIGNED;
    case TextureFormat::SFLOAT_16_16:
      return GPU_FORMAT_FLOAT | GPU_FORMAT_SIGNED;
    case TextureFormat::UNORM_16_16:
      return GPU_FORMAT_NORMALIZED_INTEGER;
    case TextureFormat::UINT_8:
      return GPU_FORMAT_INTEGER;
    case TextureFormat::SINT_8:
      return GPU_FORMAT_INTEGER | GPU_FORMAT_SIGNED;
    case TextureFormat::UNORM_8:
      return GPU_FORMAT_NORMALIZED_INTEGER;
    case TextureFormat::UINT_32:
      return GPU_FORMAT_INTEGER;
    case TextureFormat::SINT_32:
      return GPU_FORMAT_INTEGER | GPU_FORMAT_SIGNED;
    case TextureFormat::SFLOAT_32:
      return GPU_FORMAT_FLOAT | GPU_FORMAT_SIGNED;
    case TextureFormat::UINT_16:
      return GPU_FORMAT_INTEGER;
    case TextureFormat::SINT_16:
      return GPU_FORMAT_INTEGER | GPU_FORMAT_SIGNED;
    case TextureFormat::SFLOAT_16:
      return GPU_FORMAT_FLOAT | GPU_FORMAT_SIGNED;
    case TextureFormat::UNORM_16:
      return GPU_FORMAT_NORMALIZED_INTEGER;

    /* Special formats texture & render-buffer */
    case TextureFormat::UNORM_10_10_10_2:
      return GPU_FORMAT_NORMALIZED_INTEGER;
    case TextureFormat::UINT_10_10_10_2:
      return GPU_FORMAT_INTEGER;
    case TextureFormat::UFLOAT_11_11_10:
      return GPU_FORMAT_FLOAT;
    case TextureFormat::SFLOAT_32_DEPTH_UINT_8:
      return GPU_FORMAT_DEPTH_STENCIL;
    case TextureFormat::SRGBA_8_8_8_8:
      return GPU_FORMAT_NORMALIZED_INTEGER | GPU_FORMAT_SRGB;

    /* Texture only formats. */
    case TextureFormat::SFLOAT_16_16_16:
      return GPU_FORMAT_FLOAT | GPU_FORMAT_SIGNED;
    case TextureFormat::SNORM_16_16_16:
      return GPU_FORMAT_NORMALIZED_INTEGER | GPU_FORMAT_SIGNED;
    case TextureFormat::SINT_16_16_16:
      return GPU_FORMAT_INTEGER | GPU_FORMAT_SIGNED;
    case TextureFormat::UINT_16_16_16:
      return GPU_FORMAT_INTEGER;
    case TextureFormat::UNORM_16_16_16:
      return GPU_FORMAT_NORMALIZED_INTEGER;
    case TextureFormat::SNORM_16_16_16_16:
    case TextureFormat::SNORM_8_8_8_8:
      return GPU_FORMAT_NORMALIZED_INTEGER | GPU_FORMAT_SIGNED;
    case TextureFormat::SFLOAT_32_32_32:
      return GPU_FORMAT_FLOAT | GPU_FORMAT_SIGNED;
    case TextureFormat::SINT_32_32_32:
      return GPU_FORMAT_INTEGER | GPU_FORMAT_SIGNED;
    case TextureFormat::UINT_32_32_32:
      return GPU_FORMAT_INTEGER;
    case TextureFormat::SNORM_8_8_8:
      return GPU_FORMAT_NORMALIZED_INTEGER | GPU_FORMAT_SIGNED;
    case TextureFormat::UNORM_8_8_8:
      return GPU_FORMAT_NORMALIZED_INTEGER;
    case TextureFormat::SINT_8_8_8:
      return GPU_FORMAT_INTEGER | GPU_FORMAT_SIGNED;
    case TextureFormat::UINT_8_8_8:
      return GPU_FORMAT_INTEGER;
    case TextureFormat::SNORM_16_16:
    case TextureFormat::SNORM_8_8:
    case TextureFormat::SNORM_16:
    case TextureFormat::SNORM_8:
      return GPU_FORMAT_NORMALIZED_INTEGER | GPU_FORMAT_SIGNED;

    /* Special formats, texture only. */
    case TextureFormat::SRGB_DXT1:
    case TextureFormat::SRGB_DXT3:
    case TextureFormat::SRGB_DXT5:
      return GPU_FORMAT_NORMALIZED_INTEGER | GPU_FORMAT_COMPRESSED | GPU_FORMAT_SRGB;
    case TextureFormat::SNORM_DXT1:
    case TextureFormat::SNORM_DXT3:
    case TextureFormat::SNORM_DXT5:
      return GPU_FORMAT_NORMALIZED_INTEGER | GPU_FORMAT_COMPRESSED;
    case TextureFormat::SRGBA_8_8_8:
      return GPU_FORMAT_NORMALIZED_INTEGER | GPU_FORMAT_SRGB;
    case TextureFormat::UFLOAT_9_9_9_EXP_5:
      return GPU_FORMAT_FLOAT;

    /* Depth Formats. */
    case TextureFormat::SFLOAT_32_DEPTH:
    case TextureFormat::UNORM_16_DEPTH:
      return GPU_FORMAT_DEPTH;

    case TextureFormat::Invalid:
      BLI_assert_unreachable();
  }
  BLI_assert_unreachable();
  return GPU_FORMAT_FLOAT;
}

inline int to_component_len(TextureFormat format)
{
  return format_component_len(DataFormat(format));
}

inline size_t to_bytesize(eGPUDataFormat data_format)
{
  switch (data_format) {
    case GPU_DATA_UBYTE:
      return 1;
    case GPU_DATA_HALF_FLOAT:
      return 2;
    case GPU_DATA_FLOAT:
    case GPU_DATA_INT:
    case GPU_DATA_UINT:
      return 4;
    case GPU_DATA_UINT_24_8_DEPRECATED:
    case GPU_DATA_10_11_11_REV:
    case GPU_DATA_2_10_10_10_REV:
      return 4;
  }
  BLI_assert_unreachable();
  return 0;
}

inline size_t to_bytesize(TextureFormat tex_format, eGPUDataFormat data_format)
{
  /* Special case for compacted types.
   * Standard component len calculation does not apply, as the texture formats contain multiple
   * channels, but associated data format contains several compacted components. */
  if ((tex_format == TextureFormat::UFLOAT_11_11_10 && data_format == GPU_DATA_10_11_11_REV) ||
      ((tex_format == TextureFormat::UNORM_10_10_10_2 ||
        tex_format == TextureFormat::UINT_10_10_10_2) &&
       data_format == GPU_DATA_2_10_10_10_REV))
  {
    return 4;
  }

  return to_component_len(tex_format) * to_bytesize(data_format);
}

/* Definitely not complete, edit according to the gl specification. */
constexpr bool validate_data_format(TextureFormat tex_format, eGPUDataFormat data_format)
{
  switch (tex_format) {
    /* Formats texture & render-buffer */
    case TextureFormat::UINT_32_32_32_32:
    case TextureFormat::UINT_32_32:
    case TextureFormat::UINT_32:
      return ELEM(data_format, GPU_DATA_UINT);
    case TextureFormat::UINT_16_16_16_16:
    case TextureFormat::UINT_16_16:
    case TextureFormat::UINT_16:
      return ELEM(data_format, GPU_DATA_UINT); /* Also GPU_DATA_USHORT if needed. */
    case TextureFormat::UINT_8_8_8_8:
    case TextureFormat::UINT_8_8:
    case TextureFormat::UINT_8:
      return ELEM(data_format, GPU_DATA_UINT, GPU_DATA_UBYTE);

    case TextureFormat::SINT_32_32_32_32:
    case TextureFormat::SINT_32_32:
    case TextureFormat::SINT_32:
      return ELEM(data_format, GPU_DATA_INT);
    case TextureFormat::SINT_16_16_16_16:
    case TextureFormat::SINT_16_16:
    case TextureFormat::SINT_16:
      return ELEM(data_format, GPU_DATA_INT); /* Also GPU_DATA_SHORT if needed. */
    case TextureFormat::SINT_8_8_8_8:
    case TextureFormat::SINT_8_8:
    case TextureFormat::SINT_8:
      return ELEM(data_format, GPU_DATA_INT); /* Also GPU_DATA_BYTE if needed. */

    case TextureFormat::SFLOAT_32_32_32_32:
    case TextureFormat::SFLOAT_32_32:
    case TextureFormat::SFLOAT_32:
      return ELEM(data_format, GPU_DATA_FLOAT);
    case TextureFormat::SFLOAT_16_16_16_16:
    case TextureFormat::SFLOAT_16_16:
    case TextureFormat::SFLOAT_16:
      return ELEM(data_format, GPU_DATA_FLOAT, GPU_DATA_HALF_FLOAT);
    case TextureFormat::UNORM_16_16_16_16:
    case TextureFormat::UNORM_16_16:
    case TextureFormat::UNORM_16:
      return ELEM(data_format, GPU_DATA_FLOAT); /* Also GPU_DATA_USHORT if needed. */
    case TextureFormat::UNORM_8_8_8_8:
    case TextureFormat::UNORM_8_8:
    case TextureFormat::UNORM_8:
      return ELEM(data_format, GPU_DATA_FLOAT, GPU_DATA_UBYTE);

    /* Special formats texture & render-buffer */
    case TextureFormat::UNORM_10_10_10_2:
    case TextureFormat::UINT_10_10_10_2:
      return ELEM(data_format, GPU_DATA_FLOAT, GPU_DATA_2_10_10_10_REV);
    case TextureFormat::UFLOAT_11_11_10:
      return ELEM(data_format, GPU_DATA_FLOAT, GPU_DATA_10_11_11_REV);
    case TextureFormat::SFLOAT_32_DEPTH_UINT_8:
      /* Should have its own type. For now, we rely on the backend to do the conversion. */
      return ELEM(data_format, GPU_DATA_FLOAT, GPU_DATA_UINT_24_8_DEPRECATED, GPU_DATA_UINT);
    case TextureFormat::SRGBA_8_8_8_8:
      return ELEM(data_format, GPU_DATA_FLOAT, GPU_DATA_UBYTE);

    /* Texture only formats. */
    case TextureFormat::UINT_32_32_32:
      return ELEM(data_format, GPU_DATA_UINT);
    case TextureFormat::UINT_16_16_16:
      return ELEM(data_format, GPU_DATA_UINT); /* Also GPU_DATA_SHORT if needed. */
    case TextureFormat::UINT_8_8_8:
      return ELEM(data_format, GPU_DATA_UINT); /* Also GPU_DATA_BYTE if needed. */
    case TextureFormat::SINT_32_32_32:
      return ELEM(data_format, GPU_DATA_INT);
    case TextureFormat::SINT_16_16_16:
      return ELEM(data_format, GPU_DATA_INT); /* Also GPU_DATA_USHORT if needed. */
    case TextureFormat::SINT_8_8_8:
      return ELEM(data_format, GPU_DATA_INT, GPU_DATA_UBYTE);
    case TextureFormat::UNORM_16_16_16:
      return ELEM(data_format, GPU_DATA_FLOAT); /* Also GPU_DATA_USHORT if needed. */
    case TextureFormat::UNORM_8_8_8:
      return ELEM(data_format, GPU_DATA_FLOAT, GPU_DATA_UBYTE);
    case TextureFormat::SNORM_16_16_16_16:
    case TextureFormat::SNORM_16_16_16:
    case TextureFormat::SNORM_16_16:
    case TextureFormat::SNORM_16:
      return ELEM(data_format, GPU_DATA_FLOAT); /* Also GPU_DATA_SHORT if needed. */
    case TextureFormat::SNORM_8_8_8_8:
    case TextureFormat::SNORM_8_8_8:
    case TextureFormat::SNORM_8_8:
    case TextureFormat::SNORM_8:
      return ELEM(data_format, GPU_DATA_FLOAT); /* Also GPU_DATA_BYTE if needed. */
    case TextureFormat::SFLOAT_32_32_32:
      return ELEM(data_format, GPU_DATA_FLOAT);
    case TextureFormat::SFLOAT_16_16_16:
      return ELEM(data_format, GPU_DATA_FLOAT, GPU_DATA_HALF_FLOAT);

    /* Special formats, texture only. */
    case TextureFormat::SRGB_DXT1:
    case TextureFormat::SRGB_DXT3:
    case TextureFormat::SRGB_DXT5:
    case TextureFormat::SNORM_DXT1:
    case TextureFormat::SNORM_DXT3:
    case TextureFormat::SNORM_DXT5:
      /* TODO(fclem): GPU_DATA_COMPRESSED for each compression? Wouldn't it be overkill?
       * For now, expect format to be set to float. */
      return ELEM(data_format, GPU_DATA_FLOAT);
    case TextureFormat::SRGBA_8_8_8:
      return ELEM(data_format, GPU_DATA_FLOAT, GPU_DATA_UBYTE);
    case TextureFormat::UFLOAT_9_9_9_EXP_5:
      return ELEM(data_format, GPU_DATA_FLOAT);

    /* Depth Formats. */
    case TextureFormat::SFLOAT_32_DEPTH:
    case TextureFormat::UNORM_16_DEPTH:
      return ELEM(data_format, GPU_DATA_FLOAT, GPU_DATA_UINT);

    case TextureFormat::Invalid:
      BLI_assert_unreachable();
  }
  BLI_assert_unreachable();
  return data_format == GPU_DATA_FLOAT;
}

/* Return default data format for an internal texture format. */
inline eGPUDataFormat to_texture_data_format(TextureFormat tex_format)
{
  switch (tex_format) {
    /* Formats texture & render-buffer */
    case TextureFormat::UINT_32_32_32_32:
    case TextureFormat::UINT_32_32:
    case TextureFormat::UINT_32:
    case TextureFormat::UINT_16_16_16_16:
    case TextureFormat::UINT_16_16:
    case TextureFormat::UINT_16:
    case TextureFormat::UINT_8_8_8_8:
    case TextureFormat::UINT_8_8:
    case TextureFormat::UINT_8:
      return GPU_DATA_UINT;

    case TextureFormat::SINT_32_32_32_32:
    case TextureFormat::SINT_32_32:
    case TextureFormat::SINT_32:
    case TextureFormat::SINT_16_16_16_16:
    case TextureFormat::SINT_16_16:
    case TextureFormat::SINT_16:
    case TextureFormat::SINT_8_8_8_8:
    case TextureFormat::SINT_8_8:
    case TextureFormat::SINT_8:
      return GPU_DATA_INT;

    case TextureFormat::SFLOAT_32_32_32_32:
    case TextureFormat::SFLOAT_32_32:
    case TextureFormat::SFLOAT_32:
    case TextureFormat::SFLOAT_16_16_16_16:
    case TextureFormat::SFLOAT_16_16:
    case TextureFormat::SFLOAT_16:
    case TextureFormat::UNORM_16_16_16_16:
    case TextureFormat::UNORM_16_16:
    case TextureFormat::UNORM_16:
    case TextureFormat::UNORM_8_8_8_8:
    case TextureFormat::UNORM_8_8:
    case TextureFormat::UNORM_8:
      return GPU_DATA_FLOAT;

    /* Special formats texture & render-buffer */
    case TextureFormat::UNORM_10_10_10_2:
    case TextureFormat::UINT_10_10_10_2:
      return GPU_DATA_2_10_10_10_REV;
    case TextureFormat::UFLOAT_11_11_10:
      return GPU_DATA_10_11_11_REV;
    case TextureFormat::SFLOAT_32_DEPTH_UINT_8:
      /* Should have its own type. For now, we rely on the backend to do the conversion. */
      return GPU_DATA_UINT_24_8_DEPRECATED;
    case TextureFormat::SRGBA_8_8_8_8:
      return GPU_DATA_FLOAT;

    /* Texture only formats. */
    case TextureFormat::UINT_32_32_32:
    case TextureFormat::UINT_16_16_16:
    case TextureFormat::UINT_8_8_8:
      return GPU_DATA_UINT;
    case TextureFormat::SINT_32_32_32:
    case TextureFormat::SINT_16_16_16:
    case TextureFormat::SINT_8_8_8:
      return GPU_DATA_INT;
    case TextureFormat::UNORM_16_16_16:
    case TextureFormat::UNORM_8_8_8:
      return GPU_DATA_FLOAT;
    case TextureFormat::SNORM_16_16_16_16:
    case TextureFormat::SNORM_16_16_16:
    case TextureFormat::SNORM_16_16:
    case TextureFormat::SNORM_16:
      return GPU_DATA_FLOAT;
    case TextureFormat::SNORM_8_8_8_8:
    case TextureFormat::SNORM_8_8_8:
    case TextureFormat::SNORM_8_8:
    case TextureFormat::SNORM_8:
      return GPU_DATA_FLOAT;
    case TextureFormat::SFLOAT_32_32_32:
    case TextureFormat::SFLOAT_16_16_16:
      return GPU_DATA_FLOAT;

    /* Special formats, texture only. */
    case TextureFormat::SRGB_DXT1:
    case TextureFormat::SRGB_DXT3:
    case TextureFormat::SRGB_DXT5:
    case TextureFormat::SNORM_DXT1:
    case TextureFormat::SNORM_DXT3:
    case TextureFormat::SNORM_DXT5:
      /* TODO(fclem): GPU_DATA_COMPRESSED for each compression? Wouldn't it be overkill?
       * For now, expect format to be set to float. */
      return GPU_DATA_FLOAT;
    case TextureFormat::SRGBA_8_8_8:
      return GPU_DATA_FLOAT;
    case TextureFormat::UFLOAT_9_9_9_EXP_5:
      return GPU_DATA_FLOAT;

    /* Depth Formats. */
    case TextureFormat::SFLOAT_32_DEPTH:
    case TextureFormat::UNORM_16_DEPTH:
      return GPU_DATA_FLOAT;
    case TextureFormat::Invalid:
      BLI_assert_unreachable();
  }
  BLI_assert_unreachable();
  return GPU_DATA_FLOAT;
}

inline GPUFrameBufferBits to_framebuffer_bits(TextureFormat tex_format)
{
  switch (tex_format) {
    /* Formats texture & render-buffer */
    case TextureFormat::UINT_32_32_32_32:
    case TextureFormat::UINT_32_32:
    case TextureFormat::UINT_32:
    case TextureFormat::UINT_16_16_16_16:
    case TextureFormat::UINT_16_16:
    case TextureFormat::UINT_16:
    case TextureFormat::UINT_8_8_8_8:
    case TextureFormat::UINT_8_8:
    case TextureFormat::UINT_8:
    case TextureFormat::SINT_32_32_32_32:
    case TextureFormat::SINT_32_32:
    case TextureFormat::SINT_32:
    case TextureFormat::SINT_16_16_16_16:
    case TextureFormat::SINT_16_16:
    case TextureFormat::SINT_16:
    case TextureFormat::SINT_8_8_8_8:
    case TextureFormat::SINT_8_8:
    case TextureFormat::SINT_8:
    case TextureFormat::SFLOAT_32_32_32_32:
    case TextureFormat::SFLOAT_32_32:
    case TextureFormat::SFLOAT_32:
    case TextureFormat::SFLOAT_16_16_16_16:
    case TextureFormat::SFLOAT_16_16:
    case TextureFormat::SFLOAT_16:
    case TextureFormat::UNORM_16_16_16_16:
    case TextureFormat::UNORM_16_16:
    case TextureFormat::UNORM_16:
    case TextureFormat::UNORM_8_8_8_8:
    case TextureFormat::UNORM_8_8:
    case TextureFormat::UNORM_8:
      return GPU_COLOR_BIT;

    /* Special formats texture & render-buffer */
    case TextureFormat::UNORM_10_10_10_2:
    case TextureFormat::UINT_10_10_10_2:
    case TextureFormat::UFLOAT_11_11_10:
    case TextureFormat::SRGBA_8_8_8_8:
      return GPU_COLOR_BIT;
    case TextureFormat::SFLOAT_32_DEPTH_UINT_8:
      return GPU_DEPTH_BIT | GPU_STENCIL_BIT;

    /* Depth Formats. */
    case TextureFormat::SFLOAT_32_DEPTH:
    case TextureFormat::UNORM_16_DEPTH:
      return GPU_DEPTH_BIT;

    /* Texture only formats. */
    case TextureFormat::UINT_32_32_32:
    case TextureFormat::UINT_16_16_16:
    case TextureFormat::UINT_8_8_8:
    case TextureFormat::SINT_32_32_32:
    case TextureFormat::SINT_16_16_16:
    case TextureFormat::SINT_8_8_8:
    case TextureFormat::UNORM_16_16_16:
    case TextureFormat::UNORM_8_8_8:
    case TextureFormat::SNORM_16_16_16_16:
    case TextureFormat::SNORM_16_16_16:
    case TextureFormat::SNORM_16_16:
    case TextureFormat::SNORM_16:
    case TextureFormat::SNORM_8_8_8_8:
    case TextureFormat::SNORM_8_8_8:
    case TextureFormat::SNORM_8_8:
    case TextureFormat::SNORM_8:
    case TextureFormat::SFLOAT_32_32_32:
    case TextureFormat::SFLOAT_16_16_16:
      BLI_assert_msg(0, "This texture format is not compatible with framebuffer attachment.");
      return GPU_COLOR_BIT;

    /* Special formats, texture only. */
    case TextureFormat::SRGB_DXT1:
    case TextureFormat::SRGB_DXT3:
    case TextureFormat::SRGB_DXT5:
    case TextureFormat::SNORM_DXT1:
    case TextureFormat::SNORM_DXT3:
    case TextureFormat::SNORM_DXT5:
    case TextureFormat::SRGBA_8_8_8:
    case TextureFormat::UFLOAT_9_9_9_EXP_5:
      BLI_assert_msg(0, "This texture format is not compatible with framebuffer attachment.");
      return GPU_COLOR_BIT;

    case TextureFormat::Invalid:
      BLI_assert_unreachable();
  }
  BLI_assert_unreachable();
  return GPU_COLOR_BIT;
}

static inline TextureFormat to_texture_format(const GPUVertFormat *format)
{
  if (format->attr_len == 0) {
    BLI_assert_msg(0, "Incorrect vertex format for buffer texture");
    return TextureFormat(0);
  }
  return TextureFormat(format->attrs[0].type.format);
}

}  // namespace blender::gpu
