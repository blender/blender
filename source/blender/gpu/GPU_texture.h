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

#ifndef __GPU_TEXTURE_H__
#define __GPU_TEXTURE_H__

#include "GPU_state.h"

#ifdef __cplusplus
extern "C" {
#endif

struct GPUVertBuf;
struct Image;
struct ImageUser;
struct PreviewImage;
struct rcti;

struct GPUFrameBuffer;
typedef struct GPUTexture GPUTexture;

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
 * If you need a type just uncomment it. Be aware that some formats
 * are not supported by renderbuffers. All of the following formats
 * are part of the OpenGL 3.3 core
 * specification. */
typedef enum eGPUTextureFormat {
  /* Formats texture & renderbuffer */
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
#if 0
  GPU_RGB10_A2,
  GPU_RGB10_A2UI,
#endif
  GPU_R11F_G11F_B10F,
  GPU_DEPTH32F_STENCIL8,
  GPU_DEPTH24_STENCIL8,

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
#if 0
  GPU_SRGB8_A8,
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
  GPU_DATA_UNSIGNED_INT,
  GPU_DATA_UNSIGNED_BYTE,
  GPU_DATA_UNSIGNED_INT_24_8,
  GPU_DATA_10_11_11_REV,
} eGPUDataFormat;

unsigned int GPU_texture_memory_usage_get(void);

/* TODO make it static function again. (create function with eGPUDataFormat exposed) */
GPUTexture *GPU_texture_create_nD(int w,
                                  int h,
                                  int d,
                                  int n,
                                  const void *pixels,
                                  eGPUTextureFormat tex_format,
                                  eGPUDataFormat gpu_data_format,
                                  int samples,
                                  const bool can_rescale,
                                  char err_out[256]);

GPUTexture *GPU_texture_create_1d(int w,
                                  eGPUTextureFormat data_type,
                                  const float *pixels,
                                  char err_out[256]);
GPUTexture *GPU_texture_create_1d_array(
    int w, int h, eGPUTextureFormat data_type, const float *pixels, char err_out[256]);
GPUTexture *GPU_texture_create_2d(
    int w, int h, eGPUTextureFormat data_type, const float *pixels, char err_out[256]);
GPUTexture *GPU_texture_create_2d_multisample(int w,
                                              int h,
                                              eGPUTextureFormat data_type,
                                              const float *pixels,
                                              int samples,
                                              char err_out[256]);
GPUTexture *GPU_texture_create_2d_array(
    int w, int h, int d, eGPUTextureFormat data_type, const float *pixels, char err_out[256]);
GPUTexture *GPU_texture_create_3d(
    int w, int h, int d, eGPUTextureFormat data_type, const float *pixels, char err_out[256]);
GPUTexture *GPU_texture_create_cube(int w,
                                    eGPUTextureFormat data_type,
                                    const float *pixels,
                                    char err_out[256]);
GPUTexture *GPU_texture_create_from_vertbuf(struct GPUVertBuf *vert);
GPUTexture *GPU_texture_create_buffer(eGPUTextureFormat data_type, const uint buffer);

GPUTexture *GPU_texture_from_bindcode(int textarget, int bindcode);
GPUTexture *GPU_texture_from_blender(struct Image *ima, struct ImageUser *iuser, int textarget);
GPUTexture *GPU_texture_from_preview(struct PreviewImage *prv, int mipmap);

void GPU_texture_add_mipmap(GPUTexture *tex,
                            eGPUDataFormat gpu_data_format,
                            int miplvl,
                            const void *pixels);

void GPU_texture_update(GPUTexture *tex, eGPUDataFormat data_format, const void *pixels);
void GPU_texture_update_sub(GPUTexture *tex,
                            eGPUDataFormat gpu_data_format,
                            const void *pixels,
                            int offset_x,
                            int offset_y,
                            int offset_z,
                            int width,
                            int height,
                            int depth);

void *GPU_texture_read(GPUTexture *tex, eGPUDataFormat gpu_data_format, int miplvl);
void GPU_texture_read_rect(GPUTexture *tex,
                           eGPUDataFormat gpu_data_format,
                           const struct rcti *rect,
                           void *r_buf);

void GPU_invalid_tex_init(void);
void GPU_invalid_tex_bind(int mode);
void GPU_invalid_tex_free(void);

void GPU_texture_free(GPUTexture *tex);

void GPU_texture_ref(GPUTexture *tex);
void GPU_texture_bind(GPUTexture *tex, int number);
void GPU_texture_unbind(GPUTexture *tex);
int GPU_texture_bound_number(GPUTexture *tex);

void GPU_texture_generate_mipmap(GPUTexture *tex);
void GPU_texture_compare_mode(GPUTexture *tex, bool use_compare);
void GPU_texture_filter_mode(GPUTexture *tex, bool use_filter);
void GPU_texture_mipmap_mode(GPUTexture *tex, bool use_mipmap, bool use_filter);
void GPU_texture_wrap_mode(GPUTexture *tex, bool use_repeat);
void GPU_texture_filters(GPUTexture *tex,
                         eGPUFilterFunction min_filter,
                         eGPUFilterFunction mag_filter);

void GPU_texture_attach_framebuffer(GPUTexture *tex, struct GPUFrameBuffer *fb, int attachment);
int GPU_texture_detach_framebuffer(GPUTexture *tex, struct GPUFrameBuffer *fb);

int GPU_texture_target(const GPUTexture *tex);
int GPU_texture_width(const GPUTexture *tex);
int GPU_texture_height(const GPUTexture *tex);
int GPU_texture_layers(const GPUTexture *tex);
eGPUTextureFormat GPU_texture_format(const GPUTexture *tex);
int GPU_texture_samples(const GPUTexture *tex);
bool GPU_texture_cube(const GPUTexture *tex);
bool GPU_texture_depth(const GPUTexture *tex);
bool GPU_texture_stencil(const GPUTexture *tex);
bool GPU_texture_integer(const GPUTexture *tex);
int GPU_texture_opengl_bindcode(const GPUTexture *tex);

void GPU_texture_get_mipmap_size(GPUTexture *tex, int lvl, int *size);

#ifdef __cplusplus
}
#endif

#endif /* __GPU_TEXTURE_H__ */
