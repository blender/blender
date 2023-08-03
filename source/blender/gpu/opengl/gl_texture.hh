/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "BLI_assert.h"

#include "gpu_texture_private.hh"

struct GPUFrameBuffer;

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
  GPUFrameBuffer *framebuffer_ = nullptr;
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

  /* TODO(fclem): Legacy. Should be removed at some point. */
  uint gl_bindcode_get() const override;

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
  bool init_internal(GPUVertBuf *vbo) override;
  /** Return true on success. */
  bool init_internal(GPUTexture *src, int mip_offset, int layer_offset, bool use_stencil) override;

 private:
  bool proxy_check(int mip);
  void stencil_texture_mode_set(bool use_stencil);
  void update_sub_direct_state_access(
      int mip, int offset[3], int extent[3], GLenum gl_format, GLenum gl_type, const void *data);
  GPUFrameBuffer *framebuffer_get();

  MEM_CXX_CLASS_ALLOC_FUNCS("GLTexture")
};

class GLPixelBuffer : public PixelBuffer {
 private:
  GLuint gl_id_ = 0;

 public:
  GLPixelBuffer(uint size);
  ~GLPixelBuffer();

  void *map() override;
  void unmap() override;
  int64_t get_native_handle() override;
  size_t get_size() override;

  MEM_CXX_CLASS_ALLOC_FUNCS("GLPixelBuffer")
};

inline GLenum to_gl_internal_format(eGPUTextureFormat format)
{
  switch (format) {
    /* Texture & Render-Buffer Formats. */
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
    case GPU_RGB10_A2:
      return GL_RGB10_A2;
    case GPU_RGB10_A2UI:
      return GL_RGB10_A2UI;
    case GPU_R11F_G11F_B10F:
      return GL_R11F_G11F_B10F;
    case GPU_DEPTH32F_STENCIL8:
      return GL_DEPTH32F_STENCIL8;
    case GPU_DEPTH24_STENCIL8:
      return GL_DEPTH24_STENCIL8;
    case GPU_SRGB8_A8:
      return GL_SRGB8_ALPHA8;
    /* Texture only formats. */
    case GPU_RGB16F:
      return GL_RGB16F;
    case GPU_RGBA16_SNORM:
      return GL_RGBA16_SNORM;
    case GPU_RGBA8_SNORM:
      return GL_RGBA8_SNORM;
    case GPU_RGB32F:
      return GL_RGB32F;
    case GPU_RGB32I:
      return GL_RGB32I;
    case GPU_RGB32UI:
      return GL_RGB32UI;
    case GPU_RGB16_SNORM:
      return GL_RGB16_SNORM;
    case GPU_RGB16I:
      return GL_RGB16I;
    case GPU_RGB16UI:
      return GL_RGB16UI;
    case GPU_RGB16:
      return GL_RGB16;
    case GPU_RGB8_SNORM:
      return GL_RGB8_SNORM;
    case GPU_RGB8:
      return GL_RGB8;
    case GPU_RGB8I:
      return GL_RGB8I;
    case GPU_RGB8UI:
      return GL_RGB8UI;
    case GPU_RG16_SNORM:
      return GL_RG16_SNORM;
    case GPU_RG8_SNORM:
      return GL_RG8_SNORM;
    case GPU_R16_SNORM:
      return GL_R16_SNORM;
    case GPU_R8_SNORM:
      return GL_R8_SNORM;
    /* Special formats, texture only. */
    case GPU_SRGB8_A8_DXT1:
      return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
    case GPU_SRGB8_A8_DXT3:
      return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
    case GPU_SRGB8_A8_DXT5:
      return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
    case GPU_RGBA8_DXT1:
      return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
    case GPU_RGBA8_DXT3:
      return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
    case GPU_RGBA8_DXT5:
      return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    case GPU_SRGB8:
      return GL_SRGB8;
    case GPU_RGB9_E5:
      return GL_RGB9_E5;
    /* Depth Formats. */
    case GPU_DEPTH_COMPONENT32F:
      return GL_DEPTH_COMPONENT32F;
    case GPU_DEPTH_COMPONENT24:
      return GL_DEPTH_COMPONENT24;
    case GPU_DEPTH_COMPONENT16:
      return GL_DEPTH_COMPONENT16;
  }
  BLI_assert_msg(0, "Texture format incorrect or unsupported");
  return 0;
}

inline GLenum to_gl_target(eGPUTextureType type)
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

inline GLenum to_gl_proxy(eGPUTextureType type)
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
    case GPU_DATA_UINT_24_8:
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

inline GLenum to_gl_data_format(eGPUTextureFormat format)
{
  switch (format) {
    /* Texture & Render-Buffer Formats. */
    case GPU_RGBA8:
    case GPU_RGBA32F:
    case GPU_RGBA16F:
    case GPU_RGBA16:
      return GL_RGBA;
    case GPU_RGBA8UI:
    case GPU_RGBA8I:
    case GPU_RGBA32I:
    case GPU_RGBA32UI:
    case GPU_RGBA16UI:
    case GPU_RGBA16I:
      return GL_RGBA_INTEGER;
    case GPU_RG8:
    case GPU_RG32F:
    case GPU_RG16I:
    case GPU_RG16F:
    case GPU_RG16:
      return GL_RG;
    case GPU_RG8UI:
    case GPU_RG8I:
    case GPU_RG32UI:
    case GPU_RG32I:
    case GPU_RG16UI:
      return GL_RG_INTEGER;
    case GPU_R8:
    case GPU_R32F:
    case GPU_R16F:
    case GPU_R16:
      return GL_RED;
    case GPU_R8UI:
    case GPU_R8I:
    case GPU_R32UI:
    case GPU_R32I:
    case GPU_R16UI:
    case GPU_R16I:
      return GL_RED_INTEGER;
    /* Special formats texture & renderbuffer */
    case GPU_RGB10_A2UI:
    case GPU_RGB10_A2:
    case GPU_SRGB8_A8:
      return GL_RGBA;
    case GPU_R11F_G11F_B10F:
      return GL_RGB;
    case GPU_DEPTH32F_STENCIL8:
    case GPU_DEPTH24_STENCIL8:
      return GL_DEPTH_STENCIL;
    /* Texture only formats. */
    case GPU_RGBA16_SNORM:
    case GPU_RGBA8_SNORM:
      return GL_RGBA;
    case GPU_RGB16F:
    case GPU_RGB32F:
    case GPU_RGB32I:
    case GPU_RGB32UI:
    case GPU_RGB16_SNORM:
    case GPU_RGB16I:
    case GPU_RGB16UI:
    case GPU_RGB16:
    case GPU_RGB8_SNORM:
    case GPU_RGB8:
    case GPU_RGB8I:
    case GPU_RGB8UI:
      return GL_RGB;
    case GPU_RG16_SNORM:
    case GPU_RG8_SNORM:
      return GL_RG;
    case GPU_R16_SNORM:
    case GPU_R8_SNORM:
      return GL_RED;
      /* Special formats, texture only. */
    case GPU_SRGB8_A8_DXT1:
      return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
    case GPU_SRGB8_A8_DXT3:
      return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
    case GPU_SRGB8_A8_DXT5:
      return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
    case GPU_RGBA8_DXT1:
      return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
    case GPU_RGBA8_DXT3:
      return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
    case GPU_RGBA8_DXT5:
      return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    case GPU_SRGB8:
    case GPU_RGB9_E5:
      return GL_RGB;
    /* Depth Formats. */
    case GPU_DEPTH_COMPONENT32F:
    case GPU_DEPTH_COMPONENT24:
    case GPU_DEPTH_COMPONENT16:
      return GL_DEPTH_COMPONENT;
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
