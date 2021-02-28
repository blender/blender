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

#pragma once

#include "BLI_utildefines.h"

#include "GPU_state.h"

struct GPUVertBuf;

/** Opaque type hiding blender::gpu::Texture. */
typedef struct GPUTexture GPUTexture;

/* GPU Samplers state
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
  GPU_SAMPLER_ICON = (1 << 8),

  GPU_SAMPLER_REPEAT = (GPU_SAMPLER_REPEAT_S | GPU_SAMPLER_REPEAT_T | GPU_SAMPLER_REPEAT_R),
} eGPUSamplerState;

/* `GPU_SAMPLER_MAX` is not a valid enum value, but only a limit.
 * It also creates a bad mask for the `NOT` operator in `ENUM_OPERATORS`.
 */
static const int GPU_SAMPLER_MAX = (GPU_SAMPLER_ICON + 1);
ENUM_OPERATORS(eGPUSamplerState, GPU_SAMPLER_ICON)

#ifdef __cplusplus
extern "C" {
#endif

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

/* Wrapper to supported OpenGL/Vulkan texture internal storage
 * If you need a type just un-comment it. Be aware that some formats
 * are not supported by render-buffers. All of the following formats
 * are part of the OpenGL 3.3 core
 * specification. */
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

  /* Special formats texture & renderbuffer */
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
} eGPUDataFormat;

unsigned int GPU_texture_memory_usage_get(void);

/**
 * \note \a data is expected to be float. If the \a format is not compatible with float data or if
 * the data is not in float format, use GPU_texture_update to upload the data with the right data
 * format.
 * \a mips is the number of mip level to allocate. It must be >= 1.
 */
GPUTexture *GPU_texture_create_1d(
    const char *name, int w, int mips, eGPUTextureFormat format, const float *data);
GPUTexture *GPU_texture_create_1d_array(
    const char *name, int w, int h, int mips, eGPUTextureFormat format, const float *data);
GPUTexture *GPU_texture_create_2d(
    const char *name, int w, int h, int mips, eGPUTextureFormat format, const float *data);
GPUTexture *GPU_texture_create_2d_array(
    const char *name, int w, int h, int d, int mips, eGPUTextureFormat format, const float *data);
GPUTexture *GPU_texture_create_3d(const char *name,
                                  int w,
                                  int h,
                                  int d,
                                  int mips,
                                  eGPUTextureFormat texture_format,
                                  eGPUDataFormat data_format,
                                  const void *data);
GPUTexture *GPU_texture_create_cube(
    const char *name, int w, int mips, eGPUTextureFormat format, const float *data);
GPUTexture *GPU_texture_create_cube_array(
    const char *name, int w, int d, int mips, eGPUTextureFormat format, const float *data);

/* Special textures. */
GPUTexture *GPU_texture_create_from_vertbuf(const char *name, struct GPUVertBuf *vert);
/**
 * \a data should hold all the data for all mipmaps.
 */
GPUTexture *GPU_texture_create_compressed_2d(
    const char *name, int w, int h, int miplen, eGPUTextureFormat format, const void *data);
GPUTexture *GPU_texture_create_error(int dimension, bool array);

void GPU_texture_update_mipmap(GPUTexture *tex,
                               int miplvl,
                               eGPUDataFormat gpu_data_format,
                               const void *pixels);

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
void GPU_unpack_row_length_set(uint len);

void *GPU_texture_read(GPUTexture *tex, eGPUDataFormat data_format, int miplvl);
void GPU_texture_clear(GPUTexture *tex, eGPUDataFormat data_format, const void *data);

void GPU_texture_free(GPUTexture *tex);

void GPU_texture_ref(GPUTexture *tex);
void GPU_texture_bind(GPUTexture *tex, int unit);
void GPU_texture_bind_ex(GPUTexture *tex, eGPUSamplerState state, int unit, const bool set_number);
void GPU_texture_unbind(GPUTexture *tex);
void GPU_texture_unbind_all(void);

void GPU_texture_image_bind(GPUTexture *tex, int unit);
void GPU_texture_image_unbind(GPUTexture *tex);
void GPU_texture_image_unbind_all(void);

void GPU_texture_copy(GPUTexture *dst, GPUTexture *src);

void GPU_texture_generate_mipmap(GPUTexture *tex);
void GPU_texture_anisotropic_filter(GPUTexture *tex, bool use_aniso);
void GPU_texture_compare_mode(GPUTexture *tex, bool use_compare);
void GPU_texture_filter_mode(GPUTexture *tex, bool use_filter);
void GPU_texture_mipmap_mode(GPUTexture *tex, bool use_mipmap, bool use_filter);
void GPU_texture_wrap_mode(GPUTexture *tex, bool use_repeat, bool use_clamp);
void GPU_texture_swizzle_set(GPUTexture *tex, const char swizzle[4]);

int GPU_texture_width(const GPUTexture *tex);
int GPU_texture_height(const GPUTexture *tex);
int GPU_texture_orig_width(const GPUTexture *tex);
int GPU_texture_orig_height(const GPUTexture *tex);
void GPU_texture_orig_size_set(GPUTexture *tex, int w, int h);
eGPUTextureFormat GPU_texture_format(const GPUTexture *tex);
bool GPU_texture_array(const GPUTexture *tex);
bool GPU_texture_cube(const GPUTexture *tex);
bool GPU_texture_depth(const GPUTexture *tex);
bool GPU_texture_stencil(const GPUTexture *tex);
bool GPU_texture_integer(const GPUTexture *tex);
int GPU_texture_opengl_bindcode(const GPUTexture *tex);

void GPU_texture_get_mipmap_size(GPUTexture *tex, int lvl, int *size);

/* utilities */
size_t GPU_texture_component_len(eGPUTextureFormat format);
size_t GPU_texture_dataformat_size(eGPUDataFormat data_format);

#ifdef __cplusplus
}
#endif
