/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_utildefines.h"

#include "GPU_state.h"

struct GPUVertBuf;

/** Opaque type hiding blender::gpu::Texture. */
typedef struct GPUTexture GPUTexture;

/** Opaque type hiding blender::gpu::PixelBuffer. */
typedef struct GPUPixelBuffer GPUPixelBuffer;

/**
 * GPU Samplers state
 * - Specify the sampler state to bind a texture with.
 * - Internally used by textures.
 * - All states are created at startup to avoid runtime costs.
 */
typedef enum eGPUSamplerState {
  GPU_SAMPLER_DEFAULT = 0,
  GPU_SAMPLER_FILTER = (1 << 0),
  GPU_SAMPLER_MIPMAP = (1 << 1),
  GPU_SAMPLER_REPEAT_S = (1 << 2),
  GPU_SAMPLER_REPEAT_T = (1 << 3),
  GPU_SAMPLER_REPEAT_R = (1 << 4),
  GPU_SAMPLER_CLAMP_BORDER = (1 << 5), /* Clamp to border color instead of border texel. */
  GPU_SAMPLER_COMPARE = (1 << 6),
  GPU_SAMPLER_ANISO = (1 << 7),
  GPU_SAMPLER_MIRROR_REPEAT = (1 << 8), /* Requires any REPEAT flag to be set. */
  GPU_SAMPLER_ICON = (1 << 9),

  GPU_SAMPLER_REPEAT = (GPU_SAMPLER_REPEAT_S | GPU_SAMPLER_REPEAT_T | GPU_SAMPLER_REPEAT_R),
} eGPUSamplerState;

#define GPU_TEXTURE_FREE_SAFE(texture) \
  do { \
    if (texture != NULL) { \
      GPU_texture_free(texture); \
      texture = NULL; \
    } \
  } while (0)

/**
 * #GPU_SAMPLER_MAX is not a valid enum value, but only a limit.
 * It also creates a bad mask for the `NOT` operator in #ENUM_OPERATORS.
 */
#ifdef __cplusplus
static constexpr eGPUSamplerState GPU_SAMPLER_MAX = eGPUSamplerState(GPU_SAMPLER_ICON + 1);
#else
static const int GPU_SAMPLER_MAX = (GPU_SAMPLER_ICON + 1);
#endif

ENUM_OPERATORS(eGPUSamplerState, GPU_SAMPLER_ICON)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Update user defined sampler states.
 */
void GPU_samplers_update(void);

/* GPU Texture
 * - always returns unsigned char RGBA textures
 * - if texture with non square dimensions is created, depending on the
 *   graphics card capabilities the texture may actually be stored in a
 *   larger texture with power of two dimensions.
 * - can use reference counting:
 *   - reference counter after GPU_texture_create is 1
 *   - GPU_texture_ref increases by one
 *   - GPU_texture_free decreases by one, and frees if 0
 * - if created with from_blender, will not free the texture
 */

/**
 * Wrapper to supported OpenGL/Vulkan texture internal storage
 * If you need a type just un-comment it. Be aware that some formats
 * are not supported by render-buffers. All of the following formats
 * are part of the OpenGL 3.3 core
 * specification.
 */
typedef enum eGPUTextureFormat {
  /* Formats texture & render-buffer. */
  GPU_RGBA8UI,
  GPU_RGBA8I,
  GPU_RGBA8,
  GPU_RGBA32UI,
  GPU_RGBA32I,
  GPU_RGBA32F,
  GPU_RGBA16UI,
  GPU_RGBA16I,
  GPU_RGBA16F,
  GPU_RGBA16,
  GPU_RG8UI,
  GPU_RG8I,
  GPU_RG8,
  GPU_RG32UI,
  GPU_RG32I,
  GPU_RG32F,
  GPU_RG16UI,
  GPU_RG16I,
  GPU_RG16F,
  GPU_RG16,
  GPU_R8UI,
  GPU_R8I,
  GPU_R8,
  GPU_R32UI,
  GPU_R32I,
  GPU_R32F,
  GPU_R16UI,
  GPU_R16I,
  GPU_R16F,
  GPU_R16, /* Max texture buffer format. */

  /* Special formats texture & render-buffer. */
  GPU_RGB10_A2,
  GPU_R11F_G11F_B10F,
  GPU_DEPTH32F_STENCIL8,
  GPU_DEPTH24_STENCIL8,
  GPU_SRGB8_A8,
#if 0
  GPU_RGB10_A2UI,
#endif

  /* Texture only format */
  GPU_RGB16F,
#if 0
  GPU_RGBA16_SNORM,
  GPU_RGBA8_SNORM,
  GPU_RGB32F,
  GPU_RGB32I,
  GPU_RGB32UI,
  GPU_RGB16_SNORM,
  GPU_RGB16I,
  GPU_RGB16UI,
  GPU_RGB16,
  GPU_RGB8_SNORM,
  GPU_RGB8,
  GPU_RGB8I,
  GPU_RGB8UI,
  GPU_RG16_SNORM,
  GPU_RG8_SNORM,
  GPU_R16_SNORM,
  GPU_R8_SNORM,
#endif

  /* Special formats texture only */
  GPU_SRGB8_A8_DXT1,
  GPU_SRGB8_A8_DXT3,
  GPU_SRGB8_A8_DXT5,
  GPU_RGBA8_DXT1,
  GPU_RGBA8_DXT3,
  GPU_RGBA8_DXT5,
#if 0
  GPU_SRGB8,
  GPU_RGB9_E5,
  GPU_COMPRESSED_RG_RGTC2,
  GPU_COMPRESSED_SIGNED_RG_RGTC2,
  GPU_COMPRESSED_RED_RGTC1,
  GPU_COMPRESSED_SIGNED_RED_RGTC1,
#endif

  /* Depth Formats */
  GPU_DEPTH_COMPONENT32F,
  GPU_DEPTH_COMPONENT24,
  GPU_DEPTH_COMPONENT16,
} eGPUTextureFormat;

typedef enum eGPUDataFormat {
  GPU_DATA_FLOAT,
  GPU_DATA_INT,
  GPU_DATA_UINT,
  GPU_DATA_UBYTE,
  GPU_DATA_UINT_24_8,
  GPU_DATA_10_11_11_REV,
  GPU_DATA_2_10_10_10_REV,
  GPU_DATA_HALF_FLOAT
} eGPUDataFormat;

/** Texture usage flags.
 * Texture usage flags allow backend implementations to contextually optimize texture resources.
 * Any texture with an explicit flag should not perform operations which are not explicitly
 * specified in the usage flags. If usage is unknown upfront, then GPU_TEXTURE_USAGE_GENERAL can be
 * used.
 *
 * NOTE: These usage flags act as hints for the backend implementations. There may be no benefit in
 * some circumstances, and certain resource types may insert additional usage as required. However,
 * explicit usage can ensure that hardware features such as render target/texture compression can
 * be used. For explicit APIs such as Metal/Vulkan, texture usage needs to be specified up-front.
 */
typedef enum eGPUTextureUsage {
  /* Whether texture is sampled or read during a shader. */
  GPU_TEXTURE_USAGE_SHADER_READ = (1 << 0),
  /* Whether the texture is written to by a shader using imageStore. */
  GPU_TEXTURE_USAGE_SHADER_WRITE = (1 << 1),
  /* Whether a texture is used as an attachment in a frame-buffer. */
  GPU_TEXTURE_USAGE_ATTACHMENT = (1 << 2),
  /* Whether the texture is used as a texture view, uses mip-map layer adjustment,
   * OR, uses swizzle access masks. Mip-map base layer adjustment and texture channel swizzling
   * requires a texture view under-the-hood. */
  GPU_TEXTURE_USAGE_MIP_SWIZZLE_VIEW = (1 << 3),
  /* Whether a texture can be allocated without any backing memory. It is used as an
   * attachment to store data, but is not needed by any future passes.
   * This usage mode should be used in scenarios where an attachment has no previous
   * contents and is not stored after a render pass. */
  GPU_TEXTURE_USAGE_MEMORYLESS = (1 << 4),
  /* Whether the texture needs to be read from by the CPU. */
  GPU_TEXTURE_USAGE_HOST_READ = (1 << 5),
  GPU_TEXTURE_USAGE_GENERAL = 0xFF,
} eGPUTextureUsage;

ENUM_OPERATORS(eGPUTextureUsage, GPU_TEXTURE_USAGE_GENERAL);

unsigned int GPU_texture_memory_usage_get(void);

/**
 * \note \a data is expected to be float. If the \a format is not compatible with float data or if
 * the data is not in float format, use GPU_texture_update to upload the data with the right data
 * format.
 * NOTE: `_ex` variants of texture creation functions allow specification of explicit usage for
 * optimal performance. Using standard texture creation will use the `GPU_TEXTURE_USAGE_GENERAL`.
 *
 * Textures created via other means will either inherit usage from the source resource, or also
 * be initialized with `GPU_TEXTURE_USAGE_GENERAL`.
 *
 * flag. \a mips is the number of mip level to allocate. It must be >= 1.
 */
GPUTexture *GPU_texture_create_1d_ex(const char *name,
                                     int w,
                                     int mip_len,
                                     eGPUTextureFormat format,
                                     eGPUTextureUsage usage,
                                     const float *data);
GPUTexture *GPU_texture_create_1d_array_ex(const char *name,
                                           int w,
                                           int h,
                                           int mip_len,
                                           eGPUTextureFormat format,
                                           eGPUTextureUsage usage,
                                           const float *data);
GPUTexture *GPU_texture_create_2d_ex(const char *name,
                                     int w,
                                     int h,
                                     int mips,
                                     eGPUTextureFormat format,
                                     eGPUTextureUsage usage,
                                     const float *data);
GPUTexture *GPU_texture_create_2d_array_ex(const char *name,
                                           int w,
                                           int h,
                                           int d,
                                           int mip_len,
                                           eGPUTextureFormat format,
                                           eGPUTextureUsage usage,
                                           const float *data);
GPUTexture *GPU_texture_create_3d_ex(const char *name,
                                     int w,
                                     int h,
                                     int d,
                                     int mip_len,
                                     eGPUTextureFormat texture_format,
                                     eGPUDataFormat data_format,
                                     eGPUTextureUsage usage,
                                     const void *data);
GPUTexture *GPU_texture_create_cube_ex(const char *name,
                                       int w,
                                       int mip_len,
                                       eGPUTextureFormat format,
                                       eGPUTextureUsage usage,
                                       const float *data);
GPUTexture *GPU_texture_create_cube_array_ex(const char *name,
                                             int w,
                                             int d,
                                             int mip_len,
                                             eGPUTextureFormat format,
                                             eGPUTextureUsage usage,
                                             const float *data);

/* Standard texture functions. */
GPUTexture *GPU_texture_create_1d(
    const char *name, int w, int mip_len, eGPUTextureFormat format, const float *data);
GPUTexture *GPU_texture_create_1d_array(
    const char *name, int w, int h, int mip_len, eGPUTextureFormat format, const float *data);
GPUTexture *GPU_texture_create_2d(
    const char *name, int w, int h, int mips, eGPUTextureFormat format, const float *data);
GPUTexture *GPU_texture_create_2d_array(const char *name,
                                        int w,
                                        int h,
                                        int d,
                                        int mip_len,
                                        eGPUTextureFormat format,
                                        const float *data);
GPUTexture *GPU_texture_create_3d(const char *name,
                                  int w,
                                  int h,
                                  int d,
                                  int mip_len,
                                  eGPUTextureFormat texture_format,
                                  eGPUDataFormat data_format,
                                  const void *data);
GPUTexture *GPU_texture_create_cube(
    const char *name, int w, int mip_len, eGPUTextureFormat format, const float *data);
GPUTexture *GPU_texture_create_cube_array(
    const char *name, int w, int d, int mip_len, eGPUTextureFormat format, const float *data);

/* Fetch Usage. */
eGPUTextureUsage GPU_texture_usage(const GPUTexture *texture);

/* Special textures. */

GPUTexture *GPU_texture_create_from_vertbuf(const char *name, struct GPUVertBuf *vert);
/**
 * \a data should hold all the data for all mipmaps.
 */
/**
 * DDS texture loading. Return NULL if support is not available.
 */
GPUTexture *GPU_texture_create_compressed_2d_ex(const char *name,
                                                int w,
                                                int h,
                                                int miplen,
                                                eGPUTextureFormat format,
                                                eGPUTextureUsage usage,
                                                const void *data);
GPUTexture *GPU_texture_create_compressed_2d(
    const char *name, int w, int h, int miplen, eGPUTextureFormat format, const void *data);

/**
 * Create an error texture that will bind an invalid texture (pink) at draw time.
 */
GPUTexture *GPU_texture_create_error(int dimension, bool array);
/**
 * Create an alias of the source texture data.
 * If \a src is freed, the texture view will continue to be valid.
 * If \a mip_start or \a mip_len is bigger than available mips they will be clamped.
 * If \a cube_as_array is true, then the texture cube (array) becomes a 2D array texture.
 * TODO(@fclem): Target conversion is not implemented yet.
 */
GPUTexture *GPU_texture_create_view(const char *name,
                                    const GPUTexture *src,
                                    eGPUTextureFormat format,
                                    int mip_start,
                                    int mip_len,
                                    int layer_start,
                                    int layer_len,
                                    bool cube_as_array);

GPUTexture *GPU_texture_create_single_layer_view(const char *name, const GPUTexture *src);

/**
 * Create an alias of the source texture as a texture array with only one layer.
 * Works for 1D, 2D and cube-map source texture.
 * If \a src is freed, the texture view will continue to be valid.
 */
GPUTexture *GPU_texture_create_single_layer_array_view(const char *name, const GPUTexture *src);

void GPU_texture_update_mipmap(GPUTexture *tex,
                               int miplvl,
                               eGPUDataFormat gpu_data_format,
                               const void *pixels);

/**
 * \note Updates only mip 0.
 */
void GPU_texture_update(GPUTexture *tex, eGPUDataFormat data_format, const void *data);
void GPU_texture_update_sub(GPUTexture *tex,
                            eGPUDataFormat data_format,
                            const void *pixels,
                            int offset_x,
                            int offset_y,
                            int offset_z,
                            int width,
                            int height,
                            int depth);

/* Update from API Buffer. */
void GPU_texture_update_sub_from_pixel_buffer(GPUTexture *tex,
                                              eGPUDataFormat data_format,
                                              GPUPixelBuffer *pix_buf,
                                              int offset_x,
                                              int offset_y,
                                              int offset_z,
                                              int width,
                                              int height,
                                              int depth);
/**
 * Makes data interpretation aware of the source layout.
 * Skipping pixels correctly when changing rows when doing partial update.
 */
void GPU_unpack_row_length_set(uint len);

void *GPU_texture_read(GPUTexture *tex, eGPUDataFormat data_format, int miplvl);
/**
 * Fills the whole texture with the same data for all pixels.
 * \warning Only work for 2D texture for now.
 * \warning Only clears the MIP 0 of the texture.
 * \param data_format: data format of the pixel data.
 * \note The format is float for UNORM textures.
 * \param data: 1 pixel worth of data to fill the texture with.
 */
void GPU_texture_clear(GPUTexture *tex, eGPUDataFormat data_format, const void *data);

void GPU_texture_free(GPUTexture *tex);

void GPU_texture_ref(GPUTexture *tex);
void GPU_texture_bind(GPUTexture *tex, int unit);
void GPU_texture_bind_ex(GPUTexture *tex, eGPUSamplerState state, int unit, bool set_number);
void GPU_texture_unbind(GPUTexture *tex);
void GPU_texture_unbind_all(void);

void GPU_texture_image_bind(GPUTexture *tex, int unit);
void GPU_texture_image_unbind(GPUTexture *tex);
void GPU_texture_image_unbind_all(void);

/**
 * Copy a texture content to a similar texture. Only MIP 0 is copied.
 */
void GPU_texture_copy(GPUTexture *dst, GPUTexture *src);

void GPU_texture_generate_mipmap(GPUTexture *tex);
void GPU_texture_anisotropic_filter(GPUTexture *tex, bool use_aniso);
void GPU_texture_compare_mode(GPUTexture *tex, bool use_compare);
void GPU_texture_filter_mode(GPUTexture *tex, bool use_filter);
void GPU_texture_mipmap_mode(GPUTexture *tex, bool use_mipmap, bool use_filter);
void GPU_texture_wrap_mode(GPUTexture *tex, bool use_repeat, bool use_clamp);
void GPU_texture_swizzle_set(GPUTexture *tex, const char swizzle[4]);
/**
 * Set depth stencil texture sampling behavior. Can work on texture views.
 * If stencil sampling is enabled, an unsigned integer sampler is required.
 */
void GPU_texture_stencil_texture_mode_set(GPUTexture *tex, bool use_stencil);

/**
 * Return the number of dimensions of the texture ignoring dimension of layers (1, 2 or 3).
 * Cube textures are considered 2D.
 */
int GPU_texture_dimensions(const GPUTexture *tex);

int GPU_texture_width(const GPUTexture *tex);
int GPU_texture_height(const GPUTexture *tex);
int GPU_texture_layer_count(const GPUTexture *tex);
int GPU_texture_mip_count(const GPUTexture *tex);
int GPU_texture_orig_width(const GPUTexture *tex);
int GPU_texture_orig_height(const GPUTexture *tex);
void GPU_texture_orig_size_set(GPUTexture *tex, int w, int h);
eGPUTextureFormat GPU_texture_format(const GPUTexture *tex);
const char *GPU_texture_format_description(eGPUTextureFormat texture_format);
bool GPU_texture_array(const GPUTexture *tex);
bool GPU_texture_cube(const GPUTexture *tex);
bool GPU_texture_depth(const GPUTexture *tex);
bool GPU_texture_stencil(const GPUTexture *tex);
bool GPU_texture_integer(const GPUTexture *tex);

#ifndef GPU_NO_USE_PY_REFERENCES
void **GPU_texture_py_reference_get(GPUTexture *tex);
void GPU_texture_py_reference_set(GPUTexture *tex, void **py_ref);
#endif

int GPU_texture_opengl_bindcode(const GPUTexture *tex);

void GPU_texture_get_mipmap_size(GPUTexture *tex, int lvl, int *size);

/* Utilities. */

size_t GPU_texture_component_len(eGPUTextureFormat format);
size_t GPU_texture_dataformat_size(eGPUDataFormat data_format);

/* GPU Pixel Buffer. */
GPUPixelBuffer *GPU_pixel_buffer_create(uint size);
void GPU_pixel_buffer_free(GPUPixelBuffer *pix_buf);

void *GPU_pixel_buffer_map(GPUPixelBuffer *pix_buf);
void GPU_pixel_buffer_unmap(GPUPixelBuffer *pix_buf);
uint GPU_pixel_buffer_size(GPUPixelBuffer *pix_buf);
int64_t GPU_pixel_buffer_get_native_handle(GPUPixelBuffer *pix_buf);

#ifdef __cplusplus
}
#endif
