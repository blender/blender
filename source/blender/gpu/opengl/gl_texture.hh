/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "BLI_assert.h"

#include "gpu_texture_private.hh"

namespace blender {
namespace gpu {

class GLTexture : public Texture {
  friend class GLStateManager;
  friend class GLFrameBuffer;

 private:
  /**
   * A cache of all possible sampler configurations stored along each of the three axis of
   * variation. The first and second variation axis are the wrap mode along x and y axis
   * respectively, and the third variation axis is the filtering type. See the samplers_init()
   * method for more information.
   */
  static GLuint samplers_state_cache_[GPU_SAMPLER_EXTEND_MODES_COUNT]
                                     [GPU_SAMPLER_EXTEND_MODES_COUNT]
                                     [GPU_SAMPLER_FILTERING_TYPES_COUNT];
  static const int samplers_state_cache_count_ = GPU_SAMPLER_EXTEND_MODES_COUNT *
                                                 GPU_SAMPLER_EXTEND_MODES_COUNT *
                                                 GPU_SAMPLER_FILTERING_TYPES_COUNT;
  /**
   * A cache of all custom sampler configurations described in GPUSamplerCustomType. See the
   * samplers_init() method for more information.
   */
  static GLuint custom_samplers_state_cache_[GPU_SAMPLER_CUSTOM_TYPES_COUNT];

  /** Target to bind the texture to (#GL_TEXTURE_1D, #GL_TEXTURE_2D, etc...). */
  GLenum target_ = -1;
  /** opengl identifier for texture. */
  GLuint tex_id_ = 0;
  /** Legacy workaround for texture copy. Created when using framebuffer_get(). */
  FrameBuffer *framebuffer_ = nullptr;
  /** True if this texture is bound to at least one texture unit. */
  /* TODO(fclem): How do we ensure thread safety here? */
  bool is_bound_ = false;
  /** Same as is_bound_ but for image slots. */
  bool is_bound_image_ = false;
  /** True if pixels in the texture have been initialized. */
  bool has_pixels_ = false;

 public:
  GLTexture(const char *name);
  ~GLTexture();

  void update_sub(
      int mip, int offset[3], int extent[3], eGPUDataFormat type, const void *data) override;
  void update_sub(int offset[3],
                  int extent[3],
                  eGPUDataFormat format,
                  GPUPixelBuffer *pixbuf) override;

  /**
   * This will create the mipmap images and populate them with filtered data from base level.
   *
   * \warning Depth textures are not populated but they have their mips correctly defined.
   * \warning This resets the mipmap range.
   */
  void generate_mipmap() override;
  void copy_to(Texture *dst) override;
  void clear(eGPUDataFormat format, const void *data) override;
  void swizzle_set(const char swizzle_mask[4]) override;
  void mip_range_set(int min, int max) override;
  void *read(int mip, eGPUDataFormat type) override;

  void check_feedback_loop();

  /**
   * Pre-generate, setup all possible samplers and cache them in the samplers_state_cache_ and
   * custom_samplers_state_cache_ arrays. This is done to avoid the runtime cost associated with
   * setting up a sampler at draw time.
   */
  static void samplers_init();

  /**
   * Free the samplers cache generated in samplers_init() method.
   */
  static void samplers_free();

  /**
   * Updates the anisotropic filter parameters of samplers that enables anisotropic filtering. This
   * is not done as a one time initialization in samplers_init() method because the user might
   * change the anisotropic filtering samples in the user preferences. So it is called in
   * samplers_init() method as well as every time the user preferences change.
   */
  static void samplers_update();

  /**
   * Get the handle of the OpenGL sampler that corresponds to the given sampler state.
   * The sampler is retrieved from the cached samplers computed in the samplers_init() method.
   */
  static GLuint get_sampler(const GPUSamplerState &sampler_state);

 protected:
  /** Return true on success. */
  bool init_internal() override;
  /** Return true on success. */
  bool init_internal(VertBuf *vbo) override;
  /** Return true on success. */
  bool init_internal(gpu::Texture *src,
                     int mip_offset,
                     int layer_offset,
                     bool use_stencil) override;

 private:
  bool proxy_check(int mip);
  void stencil_texture_mode_set(bool use_stencil);
  void update_sub_direct_state_access(
      int mip, int offset[3], int extent[3], GLenum gl_format, GLenum gl_type, const void *data);
  FrameBuffer *framebuffer_get();

  MEM_CXX_CLASS_ALLOC_FUNCS("GLTexture")
};

class GLPixelBuffer : public PixelBuffer {
 private:
  GLuint gl_id_ = 0;

 public:
  GLPixelBuffer(size_t size);
  ~GLPixelBuffer();

  void *map() override;
  void unmap() override;
  GPUPixelBufferNativeHandle get_native_handle() override;
  size_t get_size() override;

  MEM_CXX_CLASS_ALLOC_FUNCS("GLPixelBuffer")
};

inline GLenum to_gl_internal_format(TextureFormat format)
{
#define CASE(a, b, c, blender_enum, d, e, f, gl_pixel_enum, h) \
  case TextureFormat::blender_enum: \
    return GL_##gl_pixel_enum;

  switch (format) {
    GPU_TEXTURE_FORMAT_EXPAND(CASE)
    case TextureFormat::Invalid:
      break;
  }
#undef CASE
  BLI_assert_msg(0, "Texture format incorrect or unsupported");
  return 0;
}

inline GLenum to_gl_target(GPUTextureType type)
{
  switch (type) {
    case GPU_TEXTURE_1D:
      return GL_TEXTURE_1D;
    case GPU_TEXTURE_1D_ARRAY:
      return GL_TEXTURE_1D_ARRAY;
    case GPU_TEXTURE_2D:
      return GL_TEXTURE_2D;
    case GPU_TEXTURE_2D_ARRAY:
      return GL_TEXTURE_2D_ARRAY;
    case GPU_TEXTURE_3D:
      return GL_TEXTURE_3D;
    case GPU_TEXTURE_CUBE:
      return GL_TEXTURE_CUBE_MAP;
    case GPU_TEXTURE_CUBE_ARRAY:
      return GL_TEXTURE_CUBE_MAP_ARRAY_ARB;
    case GPU_TEXTURE_BUFFER:
      return GL_TEXTURE_BUFFER;
    default:
      BLI_assert(0);
      return GL_TEXTURE_1D;
  }
}

inline GLenum to_gl_proxy(GPUTextureType type)
{
  switch (type) {
    case GPU_TEXTURE_1D:
      return GL_PROXY_TEXTURE_1D;
    case GPU_TEXTURE_1D_ARRAY:
      return GL_PROXY_TEXTURE_1D_ARRAY;
    case GPU_TEXTURE_2D:
      return GL_PROXY_TEXTURE_2D;
    case GPU_TEXTURE_2D_ARRAY:
      return GL_PROXY_TEXTURE_2D_ARRAY;
    case GPU_TEXTURE_3D:
      return GL_PROXY_TEXTURE_3D;
    case GPU_TEXTURE_CUBE:
      return GL_PROXY_TEXTURE_CUBE_MAP;
    case GPU_TEXTURE_CUBE_ARRAY:
      return GL_PROXY_TEXTURE_CUBE_MAP_ARRAY_ARB;
    case GPU_TEXTURE_BUFFER:
    default:
      BLI_assert(0);
      return GL_TEXTURE_1D;
  }
}

inline GLenum swizzle_to_gl(const char swizzle)
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

inline GLenum to_gl(eGPUDataFormat format)
{
  switch (format) {
    case GPU_DATA_FLOAT:
      return GL_FLOAT;
    case GPU_DATA_INT:
      return GL_INT;
    case GPU_DATA_UINT:
      return GL_UNSIGNED_INT;
    case GPU_DATA_UBYTE:
      return GL_UNSIGNED_BYTE;
    case GPU_DATA_UINT_24_8_DEPRECATED:
      return GL_UNSIGNED_INT_24_8;
    case GPU_DATA_2_10_10_10_REV:
      return GL_UNSIGNED_INT_2_10_10_10_REV;
    case GPU_DATA_10_11_11_REV:
      return GL_UNSIGNED_INT_10F_11F_11F_REV;
    case GPU_DATA_HALF_FLOAT:
      return GL_HALF_FLOAT;
    default:
      BLI_assert_msg(0, "Unhandled data format");
      return GL_FLOAT;
  }
}

inline GLenum to_gl_data_format(TextureFormat format)
{
  switch (format) {
    /* Texture & Render-Buffer Formats. */
    case TextureFormat::UNORM_8_8_8_8:
    case TextureFormat::SFLOAT_32_32_32_32:
    case TextureFormat::SFLOAT_16_16_16_16:
    case TextureFormat::UNORM_16_16_16_16:
      return GL_RGBA;
    case TextureFormat::UINT_8_8_8_8:
    case TextureFormat::SINT_8_8_8_8:
    case TextureFormat::SINT_32_32_32_32:
    case TextureFormat::UINT_32_32_32_32:
    case TextureFormat::UINT_16_16_16_16:
    case TextureFormat::SINT_16_16_16_16:
      return GL_RGBA_INTEGER;
    case TextureFormat::UNORM_8_8:
    case TextureFormat::SFLOAT_32_32:
    case TextureFormat::SFLOAT_16_16:
    case TextureFormat::UNORM_16_16:
      return GL_RG;
    case TextureFormat::UINT_8_8:
    case TextureFormat::SINT_8_8:
    case TextureFormat::UINT_32_32:
    case TextureFormat::SINT_32_32:
    case TextureFormat::SINT_16_16:
    case TextureFormat::UINT_16_16:
      return GL_RG_INTEGER;
    case TextureFormat::UNORM_8:
    case TextureFormat::SFLOAT_32:
    case TextureFormat::SFLOAT_16:
    case TextureFormat::UNORM_16:
      return GL_RED;
    case TextureFormat::UINT_8:
    case TextureFormat::SINT_8:
    case TextureFormat::UINT_32:
    case TextureFormat::SINT_32:
    case TextureFormat::UINT_16:
    case TextureFormat::SINT_16:
      return GL_RED_INTEGER;
    /* Special formats texture & render-buffer. */
    case TextureFormat::UINT_10_10_10_2:
    case TextureFormat::UNORM_10_10_10_2:
    case TextureFormat::SRGBA_8_8_8_8:
      return GL_RGBA;
    case TextureFormat::UFLOAT_11_11_10:
      return GL_RGB;
    case TextureFormat::SFLOAT_32_DEPTH_UINT_8:
      return GL_DEPTH_STENCIL;
    /* Texture only formats. */
    case TextureFormat::SNORM_16_16_16_16:
    case TextureFormat::SNORM_8_8_8_8:
      return GL_RGBA;
    case TextureFormat::SFLOAT_16_16_16:
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
      return GL_RGB;
    case TextureFormat::SNORM_16_16:
    case TextureFormat::SNORM_8_8:
      return GL_RG;
    case TextureFormat::SNORM_16:
    case TextureFormat::SNORM_8:
      return GL_RED;
      /* Special formats, texture only. */
    case TextureFormat::SRGB_DXT1:
      return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
    case TextureFormat::SRGB_DXT3:
      return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
    case TextureFormat::SRGB_DXT5:
      return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
    case TextureFormat::SNORM_DXT1:
      return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
    case TextureFormat::SNORM_DXT3:
      return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
    case TextureFormat::SNORM_DXT5:
      return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    case TextureFormat::SRGBA_8_8_8:
    case TextureFormat::UFLOAT_9_9_9_EXP_5:
      return GL_RGB;
    /* Depth Formats. */
    case TextureFormat::SFLOAT_32_DEPTH:
    case TextureFormat::UNORM_16_DEPTH:
      return GL_DEPTH_COMPONENT;
    case TextureFormat::Invalid:
      break;
  }
  BLI_assert_msg(0, "Texture format incorrect or unsupported\n");
  return 0;
}

/**
 * Assume UNORM/Float target. Used with #glReadPixels.
 */
inline GLenum channel_len_to_gl(int channel_len)
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
      BLI_assert_msg(0, "Wrong number of texture channels");
      return GL_RED;
  }
}

}  // namespace gpu
}  // namespace blender
