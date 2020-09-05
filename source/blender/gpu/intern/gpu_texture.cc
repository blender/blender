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

#include "BLI_string.h"

#include "GPU_framebuffer.h"
#include "GPU_texture.h"

#include "gpu_backend.hh"
#include "gpu_context_private.hh"
#include "gpu_framebuffer_private.hh"

#include "gpu_texture_private.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

Texture::Texture(const char *name)
{
  if (name) {
    BLI_strncpy(name_, name, sizeof(name_));
  }
  else {
    name_[0] = '\0';
  }

  for (int i = 0; i < GPU_TEX_MAX_FBO_ATTACHED; i++) {
    fb[i] = NULL;
  }
}

Texture::~Texture()
{
  for (int i = 0; i < GPU_TEX_MAX_FBO_ATTACHED; i++) {
    if (fb[i] != NULL) {
      FrameBuffer *framebuffer = reinterpret_cast<FrameBuffer *>(fb[i]);
      framebuffer->attachment_set((GPUAttachmentType)fb_attachment[i], GPU_ATTACHMENT_NONE);
    }
  }
}

bool Texture::init_1D(int w, int layers, eGPUTextureFormat format)
{
  w_ = w;
  h_ = layers;
  d_ = 0;
  format_ = format;
  format_flag_ = to_format_flag(format);
  type_ = (layers > 0) ? GPU_TEXTURE_1D_ARRAY : GPU_TEXTURE_1D;
  if ((format_flag_ & (GPU_FORMAT_DEPTH_STENCIL | GPU_FORMAT_INTEGER)) == 0) {
    sampler_state = GPU_SAMPLER_FILTER;
  }
  return this->init_internal();
}

bool Texture::init_2D(int w, int h, int layers, eGPUTextureFormat format)
{
  w_ = w;
  h_ = h;
  d_ = layers;
  format_ = format;
  format_flag_ = to_format_flag(format);
  type_ = (layers > 0) ? GPU_TEXTURE_2D_ARRAY : GPU_TEXTURE_2D;
  if ((format_flag_ & (GPU_FORMAT_DEPTH_STENCIL | GPU_FORMAT_INTEGER)) == 0) {
    sampler_state = GPU_SAMPLER_FILTER;
  }
  return this->init_internal();
}

bool Texture::init_3D(int w, int h, int d, eGPUTextureFormat format)
{
  w_ = w;
  h_ = h;
  d_ = d;
  format_ = format;
  format_flag_ = to_format_flag(format);
  type_ = GPU_TEXTURE_3D;
  if ((format_flag_ & (GPU_FORMAT_DEPTH_STENCIL | GPU_FORMAT_INTEGER)) == 0) {
    sampler_state = GPU_SAMPLER_FILTER;
  }
  return this->init_internal();
}

bool Texture::init_cubemap(int w, int layers, eGPUTextureFormat format)
{
  w_ = w;
  h_ = w;
  d_ = max_ii(1, layers) * 6;
  format_ = format;
  format_flag_ = to_format_flag(format);
  type_ = (layers > 0) ? GPU_TEXTURE_CUBE_ARRAY : GPU_TEXTURE_CUBE;
  if ((format_flag_ & (GPU_FORMAT_DEPTH_STENCIL | GPU_FORMAT_INTEGER)) == 0) {
    sampler_state = GPU_SAMPLER_FILTER;
  }
  return this->init_internal();
}

bool Texture::init_buffer(GPUVertBuf *vbo, eGPUTextureFormat format)
{
  /* See to_texture_format(). */
  if (format == GPU_DEPTH_COMPONENT24) {
    return false;
  }
  w_ = vbo->vertex_len;
  h_ = 0;
  d_ = 0;
  format_ = format;
  format_flag_ = to_format_flag(format);
  type_ = GPU_TEXTURE_BUFFER;
  return this->init_internal(vbo);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operation
 * \{ */

void Texture::attach_to(FrameBuffer *)
{
}

void Texture::update(eGPUDataFormat format, const void *data)
{
  int mip = 0;
  int extent[3], offset[3] = {0, 0, 0};
  this->mip_size_get(mip, extent);
  this->update_sub(mip, offset, extent, format, data);
}

/** \} */

}  // namespace blender::gpu

/* -------------------------------------------------------------------- */
/** \name C-API
 * \{ */

using namespace blender;
using namespace blender::gpu;

/* ------ Memory Management ------- */

uint GPU_texture_memory_usage_get(void)
{
  /* TODO(fclem) Do that inside the new Texture class. */
  return 0;
}

/* -------------------------------- */

GPUTexture *GPU_texture_create_nD(int w,
                                  int h,
                                  int d,
                                  int n,
                                  const void *pixels,
                                  eGPUTextureFormat tex_format,
                                  eGPUDataFormat data_format,
                                  int UNUSED(samples),
                                  const bool UNUSED(can_rescale),
                                  char UNUSED(err_out[256]))
{
  Texture *tex = GPUBackend::get()->texture_alloc("nD");
  bool success = false;
  switch (n) {
    case 1:
      success = tex->init_1D(w, h, tex_format);
      break;
    case 2:
      success = tex->init_2D(w, h, d, tex_format);
      break;
    case 3:
      success = tex->init_3D(w, h, d, tex_format);
      break;
    default:
      break;
  }

#if 0 /* TODO */
  if (can_rescale && n == 3) {
    /* Search small enough supported texture on the system. */
    /* Rescale input. */
  }
#endif

  if (!success) {
    delete tex;
    return NULL;
  }
  if (pixels) {
    tex->update(data_format, pixels);
  }
  return reinterpret_cast<GPUTexture *>(tex);
}

GPUTexture *GPU_texture_cube_create(int w,
                                    int d,
                                    const void *pixels,
                                    eGPUTextureFormat tex_format,
                                    eGPUDataFormat data_format,
                                    char UNUSED(err_out[256]))
{
  Texture *tex = GPUBackend::get()->texture_alloc("Cube");
  bool success = tex->init_cubemap(w, d, tex_format);

  if (!success) {
    delete tex;
    return NULL;
  }
  if (pixels) {
    tex->update(data_format, pixels);
  }
  return reinterpret_cast<GPUTexture *>(tex);
}

/* DDS texture loading. Return NULL if support is not available. */
GPUTexture *GPU_texture_create_compressed(
    int w, int h, int miplen, eGPUTextureFormat tex_format, const void *data)
{
  Texture *tex = GPUBackend::get()->texture_alloc("Cube");
  bool success = tex->init_2D(w, h, 0, tex_format);

  if (!success) {
    delete tex;
    return NULL;
  }
  if (data) {
    size_t ofs = 0;
    for (int mip = 0; mip < miplen; mip++) {
      int extent[3], offset[3] = {0, 0, 0};
      tex->mip_size_get(mip, extent);

      size_t size = ((extent[0] + 3) / 4) * ((extent[1] + 3) / 4) * to_block_size(tex_format);
      tex->update_sub(mip, offset, extent, to_data_format(tex_format), (uchar *)data + ofs);

      ofs += size;
    }
  }
  return reinterpret_cast<GPUTexture *>(tex);
}

/* Create an error texture that will bind an invalid texture (pink) at draw time. */
GPUTexture *GPU_texture_create_error(int dimension, bool is_array)
{
  float pixel[4] = {1.0f, 0.0f, 1.0f, 1.0f};
  int w = 1;
  int h = (dimension < 2 && !is_array) ? 0 : 1;
  int d = (dimension < 3 && !is_array) ? 0 : 1;

  fprintf(stderr, "GPUTexture: Blender Texture Not Loaded!");
  return GPU_texture_create_nD(
      w, h, d, dimension, pixel, GPU_RGBA8, GPU_DATA_FLOAT, 0, false, NULL);
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
          return GPU_RGBA16UI;
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

GPUTexture *GPU_texture_create_from_vertbuf(GPUVertBuf *vert)
{
  eGPUTextureFormat tex_format = to_texture_format(&vert->format);
  Texture *tex = GPUBackend::get()->texture_alloc("Cube");

  bool success = tex->init_buffer(vert, tex_format);
  if (!success) {
    delete tex;
    return NULL;
  }
  return reinterpret_cast<GPUTexture *>(tex);
}

GPUTexture *GPU_texture_create_1d(int w,
                                  eGPUTextureFormat format,
                                  const float *pixels,
                                  char out[256])
{
  return GPU_texture_create_nD(w, 0, 0, 1, pixels, format, GPU_DATA_FLOAT, 0, false, out);
}

GPUTexture *GPU_texture_create_1d_array(
    int w, int h, eGPUTextureFormat format, const float *pixels, char out[256])
{
  return GPU_texture_create_nD(w, h, 0, 1, pixels, format, GPU_DATA_FLOAT, 0, false, out);
}

GPUTexture *GPU_texture_create_2d(
    int w, int h, eGPUTextureFormat format, const float *pixels, char out[256])
{
  return GPU_texture_create_nD(w, h, 0, 2, pixels, format, GPU_DATA_FLOAT, 0, false, out);
}

GPUTexture *GPU_texture_create_2d_array(
    int w, int h, int d, eGPUTextureFormat format, const float *pixels, char out[256])
{
  return GPU_texture_create_nD(w, h, d, 2, pixels, format, GPU_DATA_FLOAT, 0, false, out);
}

GPUTexture *GPU_texture_create_3d(
    int w, int h, int d, eGPUTextureFormat format, const float *pixels, char out[256])
{
  return GPU_texture_create_nD(w, h, d, 3, pixels, format, GPU_DATA_FLOAT, 0, true, out);
}

GPUTexture *GPU_texture_create_cube(int w,
                                    eGPUTextureFormat format,
                                    const float *fpixels,
                                    char out[256])
{
  return GPU_texture_cube_create(w, 0, fpixels, format, GPU_DATA_FLOAT, out);
}

GPUTexture *GPU_texture_create_cube_array(
    int w, int d, eGPUTextureFormat format, const float *fpixels, char out[256])
{
  return GPU_texture_cube_create(w, d, fpixels, format, GPU_DATA_FLOAT, out);
}

void GPU_texture_add_mipmap(GPUTexture *tex_,
                            eGPUDataFormat gpu_data_format,
                            int miplvl,
                            const void *pixels)
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  int extent[3] = {1, 1, 1}, offset[3] = {0, 0, 0};
  tex->mip_size_get(miplvl, extent);
  reinterpret_cast<Texture *>(tex)->update_sub(miplvl, offset, extent, gpu_data_format, pixels);
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
  int offset[3] = {offset_x, offset_y, offset_z};
  int extent[3] = {width, height, depth};
  reinterpret_cast<Texture *>(tex)->update_sub(0, offset, extent, gpu_data_format, pixels);
}

void *GPU_texture_read(GPUTexture *tex_, eGPUDataFormat data_format, int miplvl)
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  return tex->read(miplvl, data_format);
}

/**
 * Fills the whole texture with the same data for all pixels.
 * \warning Only work for 2D texture for now.
 * \warning Only clears the mip 0 of the texture.
 * \param data_format  data format of the pixel data.
 * \param data         1 pixel worth of data to fill the texture with.
 */
void GPU_texture_clear(GPUTexture *tex, eGPUDataFormat data_format, const void *data)
{
  BLI_assert(data != NULL); /* Do not accept NULL as parameter. */
  reinterpret_cast<Texture *>(tex)->clear(data_format, data);
}

void GPU_texture_update(GPUTexture *tex, eGPUDataFormat data_format, const void *data)
{
  reinterpret_cast<Texture *>(tex)->update(data_format, data);
}

void GPU_invalid_tex_init(void)
{
  /* TODO remove */
}

void GPU_invalid_tex_bind(int UNUSED(mode))
{
  /* TODO remove */
}

void GPU_invalid_tex_free(void)
{
  /* TODO remove */
}

void GPU_texture_bind_ex(GPUTexture *tex_,
                         eGPUSamplerState state,
                         int unit,
                         const bool UNUSED(set_number))
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  state = (state >= GPU_SAMPLER_MAX) ? tex->sampler_state : state;
  GPU_context_active_get()->state_manager->texture_bind(tex, state, unit);
}

void GPU_texture_bind(GPUTexture *tex_, int unit)
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  GPU_context_active_get()->state_manager->texture_bind(tex, tex->sampler_state, unit);
}

void GPU_texture_unbind(GPUTexture *tex_)
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  GPU_context_active_get()->state_manager->texture_unbind(tex);
}

void GPU_texture_unbind_all(void)
{
  GPU_context_active_get()->state_manager->texture_unbind_all();
}

void GPU_texture_generate_mipmap(GPUTexture *tex)
{
  // gpu_texture_memory_footprint_remove(tex);

  reinterpret_cast<Texture *>(tex)->generate_mipmap();

  // gpu_texture_memory_footprint_add(tex);
}

/* Copy a texture content to a similar texture. Only Mip 0 is copied. */
void GPU_texture_copy(GPUTexture *dst_, GPUTexture *src_)
{
  Texture *src = reinterpret_cast<Texture *>(src_);
  Texture *dst = reinterpret_cast<Texture *>(dst_);
  src->copy_to(dst);
}

void GPU_texture_compare_mode(GPUTexture *tex_, bool use_compare)
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  /* Only depth formats does support compare mode. */
  BLI_assert(!(use_compare) || (tex->format_flag_get() & GPU_FORMAT_DEPTH));
  SET_FLAG_FROM_TEST(tex->sampler_state, use_compare, GPU_SAMPLER_COMPARE);
}

void GPU_texture_filter_mode(GPUTexture *tex_, bool use_filter)
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  /* Stencil and integer format does not support filtering. */
  BLI_assert(!(use_filter) ||
             !(tex->format_flag_get() & (GPU_FORMAT_STENCIL | GPU_FORMAT_INTEGER)));
  SET_FLAG_FROM_TEST(tex->sampler_state, use_filter, GPU_SAMPLER_FILTER);
}

void GPU_texture_mipmap_mode(GPUTexture *tex_, bool use_mipmap, bool use_filter)
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  /* Stencil and integer format does not support filtering. */
  BLI_assert(!(use_filter || use_mipmap) ||
             !(tex->format_flag_get() & (GPU_FORMAT_STENCIL | GPU_FORMAT_INTEGER)));
  SET_FLAG_FROM_TEST(tex->sampler_state, use_mipmap, GPU_SAMPLER_MIPMAP);
  SET_FLAG_FROM_TEST(tex->sampler_state, use_filter, GPU_SAMPLER_FILTER);
}

void GPU_texture_anisotropic_filter(GPUTexture *tex_, bool use_aniso)
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  /* Stencil and integer format does not support filtering. */
  BLI_assert(!(use_aniso) ||
             !(tex->format_flag_get() & (GPU_FORMAT_STENCIL | GPU_FORMAT_INTEGER)));
  SET_FLAG_FROM_TEST(tex->sampler_state, use_aniso, GPU_SAMPLER_ANISO);
}

void GPU_texture_wrap_mode(GPUTexture *tex_, bool use_repeat, bool use_clamp)
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  SET_FLAG_FROM_TEST(tex->sampler_state, use_repeat, GPU_SAMPLER_REPEAT);
  SET_FLAG_FROM_TEST(tex->sampler_state, !use_clamp, GPU_SAMPLER_CLAMP_BORDER);
}

void GPU_texture_swizzle_set(GPUTexture *tex, const char swizzle[4])
{
  reinterpret_cast<Texture *>(tex)->swizzle_set(swizzle);
}

void GPU_texture_free(GPUTexture *tex_)
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  tex->refcount--;

  if (tex->refcount < 0) {
    fprintf(stderr, "GPUTexture: negative refcount\n");
  }

  if (tex->refcount == 0) {
    delete tex;
  }
}

void GPU_texture_ref(GPUTexture *tex)
{
  reinterpret_cast<Texture *>(tex)->refcount++;
}

/* TODO(fclem) Remove! This is broken as it is! */
int GPU_texture_target(const GPUTexture *UNUSED(tex))
{
  return GL_TEXTURE_2D;
}

int GPU_texture_width(const GPUTexture *tex)
{
  return reinterpret_cast<const Texture *>(tex)->width_get();
}

int GPU_texture_height(const GPUTexture *tex)
{
  return reinterpret_cast<const Texture *>(tex)->height_get();
}

int GPU_texture_orig_width(const GPUTexture *tex)
{
  return reinterpret_cast<const Texture *>(tex)->src_w;
}

int GPU_texture_orig_height(const GPUTexture *tex)
{
  return reinterpret_cast<const Texture *>(tex)->src_h;
}

void GPU_texture_orig_size_set(GPUTexture *tex_, int w, int h)
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  tex->src_w = w;
  tex->src_h = h;
}

eGPUTextureFormat GPU_texture_format(const GPUTexture *tex)
{
  return reinterpret_cast<const Texture *>(tex)->format_get();
}

/* TODO remove */
int GPU_texture_samples(const GPUTexture *UNUSED(tex))
{
  return 0;
}

bool GPU_texture_depth(const GPUTexture *tex)
{
  return (reinterpret_cast<const Texture *>(tex)->format_flag_get() & GPU_FORMAT_DEPTH) != 0;
}

bool GPU_texture_stencil(const GPUTexture *tex)
{
  return (reinterpret_cast<const Texture *>(tex)->format_flag_get() & GPU_FORMAT_STENCIL) != 0;
}

bool GPU_texture_integer(const GPUTexture *tex)
{
  return (reinterpret_cast<const Texture *>(tex)->format_flag_get() & GPU_FORMAT_INTEGER) != 0;
}

bool GPU_texture_cube(const GPUTexture *tex)
{
  return (reinterpret_cast<const Texture *>(tex)->type_get() & GPU_TEXTURE_CUBE) != 0;
}

bool GPU_texture_array(const GPUTexture *tex)
{
  return (reinterpret_cast<const Texture *>(tex)->type_get() & GPU_TEXTURE_ARRAY) != 0;
}

/* TODO remove */
int GPU_texture_opengl_bindcode(const GPUTexture *tex)
{
  return reinterpret_cast<const Texture *>(tex)->gl_bindcode_get();
}

void GPU_texture_attach_framebuffer(GPUTexture *tex_, GPUFrameBuffer *fb, int attachment)
{
  /* TODO cleanup casts */
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  for (int i = 0; i < GPU_TEX_MAX_FBO_ATTACHED; i++) {
    if (tex->fb[i] == NULL) {
      tex->fb[i] = reinterpret_cast<FrameBuffer *>(fb);
      tex->fb_attachment[i] = (GPUAttachmentType)attachment;
      return;
    }
  }

  BLI_assert(!"Error: Texture: Not enough Framebuffer slots");
}

/* Return previous attachment point */
void GPU_texture_detach_framebuffer(GPUTexture *tex_, GPUFrameBuffer *fb)
{
  /* TODO cleanup casts */
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  for (int i = 0; i < GPU_TEX_MAX_FBO_ATTACHED; i++) {
    if (tex->fb[i] == reinterpret_cast<FrameBuffer *>(fb)) {
      tex->fb[i] = NULL;
      return;
    }
  }
  BLI_assert(!"Error: Texture: Framebuffer is not attached");
}

/* Return attachment type for the given framebuffer or -1 if not attached. */
int GPU_texture_framebuffer_attachment_get(GPUTexture *tex_, GPUFrameBuffer *fb)
{
  /* TODO cleanup casts */
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  for (int i = 0; i < GPU_TEX_MAX_FBO_ATTACHED; i++) {
    if (tex->fb[i] == reinterpret_cast<FrameBuffer *>(fb)) {
      return tex->fb_attachment[i];
    }
  }
  return -1;
}

void GPU_texture_get_mipmap_size(GPUTexture *tex, int lvl, int *r_size)
{
  return reinterpret_cast<Texture *>(tex)->mip_size_get(lvl, r_size);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPU Sampler Objects
 *
 * Simple wrapper around opengl sampler objects.
 * Override texture sampler state for one sampler unit only.
 * \{ */

/* Update user defined sampler states. */
void GPU_samplers_update(void)
{
  GPUBackend::get()->samplers_update();
}

/** \} */
