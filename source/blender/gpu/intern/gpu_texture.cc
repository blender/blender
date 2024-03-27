/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_string.h"

#include "GPU_framebuffer.hh"
#include "GPU_texture.hh"

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
    STRNCPY(name_, name);
  }
  else {
    name_[0] = '\0';
  }

  for (int i = 0; i < ARRAY_SIZE(fb_); i++) {
    fb_[i] = nullptr;
  }

  gpu_image_usage_flags_ = GPU_TEXTURE_USAGE_GENERAL;
}

Texture::~Texture()
{
  for (int i = 0; i < ARRAY_SIZE(fb_); i++) {
    if (fb_[i] != nullptr) {
      fb_[i]->attachment_remove(fb_attachment_[i]);
    }
  }

#ifndef GPU_NO_USE_PY_REFERENCES
  if (this->py_ref) {
    *this->py_ref = nullptr;
  }
#endif
}

bool Texture::init_1D(int w, int layers, int mip_len, eGPUTextureFormat format)
{
  w_ = w;
  h_ = layers;
  d_ = 0;
  int mip_len_max = 1 + floorf(log2f(w));
  mipmaps_ = min_ii(mip_len, mip_len_max);
  format_ = format;
  format_flag_ = to_format_flag(format);
  type_ = (layers > 0) ? GPU_TEXTURE_1D_ARRAY : GPU_TEXTURE_1D;
  if ((format_flag_ & (GPU_FORMAT_DEPTH_STENCIL | GPU_FORMAT_INTEGER)) == 0) {
    sampler_state.filtering = GPU_SAMPLER_FILTERING_LINEAR;
  }
  return this->init_internal();
}

bool Texture::init_2D(int w, int h, int layers, int mip_len, eGPUTextureFormat format)
{
  w_ = w;
  h_ = h;
  d_ = layers;
  int mip_len_max = 1 + floorf(log2f(max_ii(w, h)));
  mipmaps_ = min_ii(mip_len, mip_len_max);
  format_ = format;
  format_flag_ = to_format_flag(format);
  type_ = (layers > 0) ? GPU_TEXTURE_2D_ARRAY : GPU_TEXTURE_2D;
  if ((format_flag_ & (GPU_FORMAT_DEPTH_STENCIL | GPU_FORMAT_INTEGER)) == 0) {
    sampler_state.filtering = GPU_SAMPLER_FILTERING_LINEAR;
  }
  return this->init_internal();
}

bool Texture::init_3D(int w, int h, int d, int mip_len, eGPUTextureFormat format)
{
  w_ = w;
  h_ = h;
  d_ = d;
  int mip_len_max = 1 + floorf(log2f(max_iii(w, h, d)));
  mipmaps_ = min_ii(mip_len, mip_len_max);
  format_ = format;
  format_flag_ = to_format_flag(format);
  type_ = GPU_TEXTURE_3D;
  if ((format_flag_ & (GPU_FORMAT_DEPTH_STENCIL | GPU_FORMAT_INTEGER)) == 0) {
    sampler_state.filtering = GPU_SAMPLER_FILTERING_LINEAR;
  }
  return this->init_internal();
}

bool Texture::init_cubemap(int w, int layers, int mip_len, eGPUTextureFormat format)
{
  w_ = w;
  h_ = w;
  d_ = max_ii(1, layers) * 6;
  int mip_len_max = 1 + floorf(log2f(w));
  mipmaps_ = min_ii(mip_len, mip_len_max);
  format_ = format;
  format_flag_ = to_format_flag(format);
  type_ = (layers > 0) ? GPU_TEXTURE_CUBE_ARRAY : GPU_TEXTURE_CUBE;
  if ((format_flag_ & (GPU_FORMAT_DEPTH_STENCIL | GPU_FORMAT_INTEGER)) == 0) {
    sampler_state.filtering = GPU_SAMPLER_FILTERING_LINEAR;
  }
  return this->init_internal();
}

bool Texture::init_buffer(VertBuf *vbo, eGPUTextureFormat format)
{
  /* See to_texture_format(). */
  if (format == GPU_DEPTH_COMPONENT24) {
    return false;
  }
  w_ = GPU_vertbuf_get_vertex_len(vbo);
  h_ = 0;
  d_ = 0;
  format_ = format;
  format_flag_ = to_format_flag(format);
  type_ = GPU_TEXTURE_BUFFER;
  return this->init_internal(vbo);
}

bool Texture::init_view(GPUTexture *src_,
                        eGPUTextureFormat format,
                        eGPUTextureType type,
                        int mip_start,
                        int mip_len,
                        int layer_start,
                        int layer_len,
                        bool cube_as_array,
                        bool use_stencil)
{
  const Texture *src = unwrap(src_);
  w_ = src->w_;
  h_ = src->h_;
  d_ = src->d_;
  layer_start = min_ii(layer_start, src->layer_count() - 1);
  layer_len = min_ii(layer_len, (src->layer_count() - layer_start));
  switch (type) {
    case GPU_TEXTURE_1D_ARRAY:
      h_ = layer_len;
      break;
    case GPU_TEXTURE_CUBE_ARRAY:
      BLI_assert(layer_len % 6 == 0);
      ATTR_FALLTHROUGH;
    case GPU_TEXTURE_2D_ARRAY:
      d_ = layer_len;
      break;
    default:
      BLI_assert(layer_len == 1 && layer_start == 0);
      break;
  }
  mip_start = min_ii(mip_start, src->mipmaps_ - 1);
  mip_len = min_ii(mip_len, (src->mipmaps_ - mip_start));
  mipmaps_ = mip_len;
  format_ = format;
  format_flag_ = to_format_flag(format);
  type_ = type;
  if (cube_as_array) {
    BLI_assert(type_ & GPU_TEXTURE_CUBE);
    type_ = (type_ & ~GPU_TEXTURE_CUBE) | GPU_TEXTURE_2D_ARRAY;
  }
  sampler_state = src->sampler_state;
  return this->init_internal(src_, mip_start, layer_start, use_stencil);
}

void Texture::usage_set(eGPUTextureUsage usage_flags)
{
  gpu_image_usage_flags_ = usage_flags;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operation
 * \{ */

void Texture::attach_to(FrameBuffer *fb, GPUAttachmentType type)
{
  for (int i = 0; i < ARRAY_SIZE(fb_); i++) {
    if (fb_[i] == fb) {
      /* Already stores a reference */
      if (fb_attachment_[i] != type) {
        /* Ensure it's not attached twice to the same FrameBuffer. */
        fb_[i]->attachment_remove(fb_attachment_[i]);
        fb_attachment_[i] = type;
      }
      return;
    }
  }
  for (int i = 0; i < ARRAY_SIZE(fb_); i++) {
    if (fb_[i] == nullptr) {
      fb_attachment_[i] = type;
      fb_[i] = fb;
      return;
    }
  }
  BLI_assert_msg(0, "GPU: Error: Texture: Not enough attachment");
}

void Texture::detach_from(FrameBuffer *fb)
{
  for (int i = 0; i < ARRAY_SIZE(fb_); i++) {
    if (fb_[i] == fb) {
      fb_[i]->attachment_remove(fb_attachment_[i]);
      fb_[i] = nullptr;
      return;
    }
  }
  BLI_assert_msg(0, "GPU: Error: Texture: Framebuffer is not attached");
}

void Texture::update(eGPUDataFormat format, const void *data)
{
  int mip = 0;
  int extent[3] = {1, 1, 1};
  int offset[3] = {0, 0, 0};
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

/* ------ Memory Management ------ */

uint GPU_texture_memory_usage_get()
{
  /* TODO(fclem): Do that inside the new Texture class. */
  return 0;
}

/* ------ Creation ------ */

static inline GPUTexture *gpu_texture_create(const char *name,
                                             const int w,
                                             const int h,
                                             const int d,
                                             const eGPUTextureType type,
                                             int mip_len,
                                             eGPUTextureFormat tex_format,
                                             eGPUDataFormat data_format,
                                             eGPUTextureUsage usage,
                                             const void *pixels)
{
  BLI_assert(mip_len > 0);
  Texture *tex = GPUBackend::get()->texture_alloc(name);
  tex->usage_set(usage);

  bool success = false;
  switch (type) {
    case GPU_TEXTURE_1D:
    case GPU_TEXTURE_1D_ARRAY:
      success = tex->init_1D(w, h, mip_len, tex_format);
      break;
    case GPU_TEXTURE_2D:
    case GPU_TEXTURE_2D_ARRAY:
      success = tex->init_2D(w, h, d, mip_len, tex_format);
      break;
    case GPU_TEXTURE_3D:
      success = tex->init_3D(w, h, d, mip_len, tex_format);
      break;
    case GPU_TEXTURE_CUBE:
    case GPU_TEXTURE_CUBE_ARRAY:
      success = tex->init_cubemap(w, d, mip_len, tex_format);
      break;
    default:
      break;
  }

  if (!success) {
    delete tex;
    return nullptr;
  }
  if (pixels) {
    tex->update(data_format, pixels);
  }
  return reinterpret_cast<GPUTexture *>(tex);
}

GPUTexture *GPU_texture_create_1d(const char *name,
                                  int w,
                                  int mip_len,
                                  eGPUTextureFormat format,
                                  eGPUTextureUsage usage,
                                  const float *data)
{
  return gpu_texture_create(
      name, w, 0, 0, GPU_TEXTURE_1D, mip_len, format, GPU_DATA_FLOAT, usage, data);
}

GPUTexture *GPU_texture_create_1d_array(const char *name,
                                        int w,
                                        int h,
                                        int mip_len,
                                        eGPUTextureFormat format,
                                        eGPUTextureUsage usage,
                                        const float *data)
{
  return gpu_texture_create(
      name, w, h, 0, GPU_TEXTURE_1D_ARRAY, mip_len, format, GPU_DATA_FLOAT, usage, data);
}

GPUTexture *GPU_texture_create_2d(const char *name,
                                  int w,
                                  int h,
                                  int mip_len,
                                  eGPUTextureFormat format,
                                  eGPUTextureUsage usage,
                                  const float *data)
{
  return gpu_texture_create(
      name, w, h, 0, GPU_TEXTURE_2D, mip_len, format, GPU_DATA_FLOAT, usage, data);
}

GPUTexture *GPU_texture_create_2d_array(const char *name,
                                        int w,
                                        int h,
                                        int d,
                                        int mip_len,
                                        eGPUTextureFormat format,
                                        eGPUTextureUsage usage,
                                        const float *data)
{
  return gpu_texture_create(
      name, w, h, d, GPU_TEXTURE_2D_ARRAY, mip_len, format, GPU_DATA_FLOAT, usage, data);
}

GPUTexture *GPU_texture_create_3d(const char *name,
                                  int w,
                                  int h,
                                  int d,
                                  int mip_len,
                                  eGPUTextureFormat texture_format,
                                  eGPUTextureUsage usage,
                                  const void *data)
{
  return gpu_texture_create(
      name, w, h, d, GPU_TEXTURE_3D, mip_len, texture_format, GPU_DATA_FLOAT, usage, data);
}

GPUTexture *GPU_texture_create_cube(const char *name,
                                    int w,
                                    int mip_len,
                                    eGPUTextureFormat format,
                                    eGPUTextureUsage usage,
                                    const float *data)
{
  return gpu_texture_create(
      name, w, w, 0, GPU_TEXTURE_CUBE, mip_len, format, GPU_DATA_FLOAT, usage, data);
}

GPUTexture *GPU_texture_create_cube_array(const char *name,
                                          int w,
                                          int d,
                                          int mip_len,
                                          eGPUTextureFormat format,
                                          eGPUTextureUsage usage,
                                          const float *data)
{
  return gpu_texture_create(
      name, w, w, d, GPU_TEXTURE_CUBE_ARRAY, mip_len, format, GPU_DATA_FLOAT, usage, data);
}

GPUTexture *GPU_texture_create_compressed_2d(const char *name,
                                             int w,
                                             int h,
                                             int miplen,
                                             eGPUTextureFormat tex_format,
                                             eGPUTextureUsage usage,
                                             const void *data)
{
  Texture *tex = GPUBackend::get()->texture_alloc(name);
  tex->usage_set(usage);
  bool success = tex->init_2D(w, h, 0, miplen, tex_format);

  if (!success) {
    delete tex;
    return nullptr;
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

GPUTexture *GPU_texture_create_from_vertbuf(const char *name, blender::gpu::VertBuf *vert)
{
#ifndef NDEBUG
  /* Vertex buffers used for texture buffers must be flagged with:
   * GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY. */
  BLI_assert_msg(vert->extended_usage_ & GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY,
                 "Vertex Buffers used for textures should have usage flag "
                 "GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY.");
#endif
  eGPUTextureFormat tex_format = to_texture_format(GPU_vertbuf_get_format(vert));
  Texture *tex = GPUBackend::get()->texture_alloc(name);

  bool success = tex->init_buffer(vert, tex_format);
  if (!success) {
    delete tex;
    return nullptr;
  }
  return reinterpret_cast<GPUTexture *>(tex);
}

GPUTexture *GPU_texture_create_error(int dimension, bool is_array)
{
  float pixel[4] = {1.0f, 0.0f, 1.0f, 1.0f};
  int w = 1;
  int h = (dimension < 2 && !is_array) ? 0 : 1;
  int d = (dimension < 3 && !is_array) ? 0 : 1;

  eGPUTextureType type = GPU_TEXTURE_3D;
  type = (dimension == 2) ? (is_array ? GPU_TEXTURE_2D_ARRAY : GPU_TEXTURE_2D) : type;
  type = (dimension == 1) ? (is_array ? GPU_TEXTURE_1D_ARRAY : GPU_TEXTURE_1D) : type;

  return gpu_texture_create("invalid_tex",
                            w,
                            h,
                            d,
                            type,
                            1,
                            GPU_RGBA8,
                            GPU_DATA_FLOAT,
                            GPU_TEXTURE_USAGE_GENERAL,
                            pixel);
}

GPUTexture *GPU_texture_create_view(const char *name,
                                    GPUTexture *src,
                                    eGPUTextureFormat format,
                                    int mip_start,
                                    int mip_len,
                                    int layer_start,
                                    int layer_len,
                                    bool cube_as_array,
                                    bool use_stencil)
{
  BLI_assert(mip_len > 0);
  BLI_assert(layer_len > 0);
  BLI_assert_msg(use_stencil == false || (GPU_texture_usage(src) & GPU_TEXTURE_USAGE_FORMAT_VIEW),
                 "Source texture of TextureView must have GPU_TEXTURE_USAGE_FORMAT_VIEW usage "
                 "flag if view texture uses stencil texturing.");
  BLI_assert_msg((format == GPU_texture_format(src)) ||
                     (GPU_texture_usage(src) & GPU_TEXTURE_USAGE_FORMAT_VIEW),
                 "Source texture of TextureView must have GPU_TEXTURE_USAGE_FORMAT_VIEW usage "
                 "flag if view texture format is different.");
  Texture *view = GPUBackend::get()->texture_alloc(name);
  view->init_view(src,
                  format,
                  unwrap(src)->type_get(),
                  mip_start,
                  mip_len,
                  layer_start,
                  layer_len,
                  cube_as_array,
                  use_stencil);
  return wrap(view);
}

/* ------ Usage ------ */
eGPUTextureUsage GPU_texture_usage(const GPUTexture *texture_)
{
  const Texture *tex = reinterpret_cast<const Texture *>(texture_);
  return tex->usage_get();
}

/* ------ Update ------ */

void GPU_texture_update_mipmap(GPUTexture *tex_,
                               int miplvl,
                               eGPUDataFormat data_format,
                               const void *pixels)
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  int extent[3] = {1, 1, 1}, offset[3] = {0, 0, 0};
  tex->mip_size_get(miplvl, extent);
  reinterpret_cast<Texture *>(tex)->update_sub(miplvl, offset, extent, data_format, pixels);
}

void GPU_texture_update_sub(GPUTexture *tex,
                            eGPUDataFormat data_format,
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
  reinterpret_cast<Texture *>(tex)->update_sub(0, offset, extent, data_format, pixels);
}

void GPU_texture_update_sub_from_pixel_buffer(GPUTexture *tex,
                                              eGPUDataFormat data_format,
                                              GPUPixelBuffer *pix_buf,
                                              int offset_x,
                                              int offset_y,
                                              int offset_z,
                                              int width,
                                              int height,
                                              int depth)
{
  int offset[3] = {offset_x, offset_y, offset_z};
  int extent[3] = {width, height, depth};
  reinterpret_cast<Texture *>(tex)->update_sub(offset, extent, data_format, pix_buf);
}

void *GPU_texture_read(GPUTexture *tex_, eGPUDataFormat data_format, int miplvl)
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  BLI_assert_msg(
      GPU_texture_usage(tex_) & GPU_TEXTURE_USAGE_HOST_READ,
      "The host-read usage flag must be specified up-front. Only textures which require data "
      "reads should be flagged, allowing the backend to make certain optimisations.");
  return tex->read(miplvl, data_format);
}

void GPU_texture_clear(GPUTexture *tex, eGPUDataFormat data_format, const void *data)
{
  BLI_assert(data != nullptr); /* Do not accept nullptr as parameter. */
  reinterpret_cast<Texture *>(tex)->clear(data_format, data);
}

void GPU_texture_update(GPUTexture *tex, eGPUDataFormat data_format, const void *data)
{
  reinterpret_cast<Texture *>(tex)->update(data_format, data);
}

void GPU_unpack_row_length_set(uint len)
{
  Context::get()->state_manager->texture_unpack_row_length_set(len);
}

/* ------ Binding ------ */

void GPU_texture_bind_ex(GPUTexture *tex_, GPUSamplerState state, int unit)
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  state = (state.type == GPU_SAMPLER_STATE_TYPE_INTERNAL) ? tex->sampler_state : state;
  Context::get()->state_manager->texture_bind(tex, state, unit);
}

void GPU_texture_bind(GPUTexture *tex_, int unit)
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  Context::get()->state_manager->texture_bind(tex, tex->sampler_state, unit);
}

void GPU_texture_unbind(GPUTexture *tex_)
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  Context::get()->state_manager->texture_unbind(tex);
}

void GPU_texture_unbind_all()
{
  Context::get()->state_manager->texture_unbind_all();
}

void GPU_texture_image_bind(GPUTexture *tex, int unit)
{
  Context::get()->state_manager->image_bind(unwrap(tex), unit);
}

void GPU_texture_image_unbind(GPUTexture *tex)
{
  Context::get()->state_manager->image_unbind(unwrap(tex));
}

void GPU_texture_image_unbind_all()
{
  Context::get()->state_manager->image_unbind_all();
}

void GPU_texture_update_mipmap_chain(GPUTexture *tex)
{
  reinterpret_cast<Texture *>(tex)->generate_mipmap();
}

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

  tex->sampler_state.type = use_compare ? GPU_SAMPLER_STATE_TYPE_CUSTOM :
                                          GPU_SAMPLER_STATE_TYPE_PARAMETERS;
  tex->sampler_state.custom_type = GPU_SAMPLER_CUSTOM_COMPARE;
}

void GPU_texture_filter_mode(GPUTexture *tex_, bool use_filter)
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  /* Stencil and integer format does not support filtering. */
  BLI_assert(!(use_filter) ||
             !(tex->format_flag_get() & (GPU_FORMAT_STENCIL | GPU_FORMAT_INTEGER)));
  tex->sampler_state.set_filtering_flag_from_test(GPU_SAMPLER_FILTERING_LINEAR, use_filter);
}

void GPU_texture_mipmap_mode(GPUTexture *tex_, bool use_mipmap, bool use_filter)
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  /* Stencil and integer format does not support filtering. */
  BLI_assert(!(use_filter || use_mipmap) ||
             !(tex->format_flag_get() & (GPU_FORMAT_STENCIL | GPU_FORMAT_INTEGER)));
  tex->sampler_state.set_filtering_flag_from_test(GPU_SAMPLER_FILTERING_MIPMAP, use_mipmap);
  tex->sampler_state.set_filtering_flag_from_test(GPU_SAMPLER_FILTERING_LINEAR, use_filter);
}

void GPU_texture_anisotropic_filter(GPUTexture *tex_, bool use_aniso)
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  /* Stencil and integer format does not support filtering. */
  BLI_assert(!(use_aniso) ||
             !(tex->format_flag_get() & (GPU_FORMAT_STENCIL | GPU_FORMAT_INTEGER)));
  tex->sampler_state.set_filtering_flag_from_test(GPU_SAMPLER_FILTERING_ANISOTROPIC, use_aniso);
}

void GPU_texture_extend_mode_x(GPUTexture *tex_, GPUSamplerExtendMode extend_mode)
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  tex->sampler_state.extend_x = extend_mode;
}

void GPU_texture_extend_mode_y(GPUTexture *tex_, GPUSamplerExtendMode extend_mode)
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  tex->sampler_state.extend_yz = extend_mode;
}

void GPU_texture_extend_mode(GPUTexture *tex_, GPUSamplerExtendMode extend_mode)
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  tex->sampler_state.extend_x = extend_mode;
  tex->sampler_state.extend_yz = extend_mode;
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

int GPU_texture_dimensions(const GPUTexture *tex_)
{
  eGPUTextureType type = reinterpret_cast<const Texture *>(tex_)->type_get();
  if (type & GPU_TEXTURE_1D) {
    return 1;
  }
  if (type & GPU_TEXTURE_2D) {
    return 2;
  }
  if (type & GPU_TEXTURE_3D) {
    return 3;
  }
  if (type & GPU_TEXTURE_CUBE) {
    return 2;
  }
  /* GPU_TEXTURE_BUFFER */
  return 1;
}

int GPU_texture_width(const GPUTexture *tex)
{
  return reinterpret_cast<const Texture *>(tex)->width_get();
}

int GPU_texture_height(const GPUTexture *tex)
{
  return reinterpret_cast<const Texture *>(tex)->height_get();
}

int GPU_texture_depth(const GPUTexture *tex)
{
  return reinterpret_cast<const Texture *>(tex)->depth_get();
}

int GPU_texture_layer_count(const GPUTexture *tex)
{
  return reinterpret_cast<const Texture *>(tex)->layer_count();
}

int GPU_texture_mip_count(const GPUTexture *tex)
{
  return reinterpret_cast<const Texture *>(tex)->mip_count();
}

int GPU_texture_original_width(const GPUTexture *tex)
{
  return reinterpret_cast<const Texture *>(tex)->src_w;
}

int GPU_texture_original_height(const GPUTexture *tex)
{
  return reinterpret_cast<const Texture *>(tex)->src_h;
}

void GPU_texture_original_size_set(GPUTexture *tex_, int w, int h)
{
  Texture *tex = reinterpret_cast<Texture *>(tex_);
  tex->src_w = w;
  tex->src_h = h;
}

eGPUTextureFormat GPU_texture_format(const GPUTexture *tex)
{
  return reinterpret_cast<const Texture *>(tex)->format_get();
}

const char *GPU_texture_format_name(eGPUTextureFormat texture_format)
{
  switch (texture_format) {
    /* Formats texture & render-buffer */
    case GPU_RGBA8UI:
      return "RGBA8UI";
    case GPU_RGBA8I:
      return "RGBA8I";
    case GPU_RGBA8:
      return "RGBA8";
    case GPU_RGBA32UI:
      return "RGBA32UI";
    case GPU_RGBA32I:
      return "RGBA32I";
    case GPU_RGBA32F:
      return "RGBA32F";
    case GPU_RGBA16UI:
      return "RGBA16UI";
    case GPU_RGBA16I:
      return "RGBA16I";
    case GPU_RGBA16F:
      return "RGBA16F";
    case GPU_RGBA16:
      return "RGBA16";
    case GPU_RG8UI:
      return "RG8UI";
    case GPU_RG8I:
      return "RG8I";
    case GPU_RG8:
      return "RG8";
    case GPU_RG32UI:
      return "RG32UI";
    case GPU_RG32I:
      return "RG32I";
    case GPU_RG32F:
      return "RG32F";
    case GPU_RG16UI:
      return "RG16UI";
    case GPU_RG16I:
      return "RG16I";
    case GPU_RG16F:
      return "RG16F";
    case GPU_RG16:
      return "RG16";
    case GPU_R8UI:
      return "R8UI";
    case GPU_R8I:
      return "R8I";
    case GPU_R8:
      return "R8";
    case GPU_R32UI:
      return "R32UI";
    case GPU_R32I:
      return "R32I";
    case GPU_R32F:
      return "R32F";
    case GPU_R16UI:
      return "R16UI";
    case GPU_R16I:
      return "R16I";
    case GPU_R16F:
      return "R16F";
    case GPU_R16:
      return "R16";
    /* Special formats texture & render-buffer */
    case GPU_RGB10_A2:
      return "RGB10_A2";
    case GPU_RGB10_A2UI:
      return "RGB10_A2UI";
    case GPU_R11F_G11F_B10F:
      return "R11F_G11F_B10F";
    case GPU_DEPTH32F_STENCIL8:
      return "DEPTH32F_STENCIL8";
    case GPU_DEPTH24_STENCIL8:
      return "DEPTH24_STENCIL8";
    case GPU_SRGB8_A8:
      return "SRGB8_A8";
    /* Texture only formats. */
    case GPU_RGB16F:
      return "RGB16F";
    case GPU_RGB16_SNORM:
      return "RGB16_SNORM";
    case GPU_RGB16I:
      return "RGB16I";
    case GPU_RGB16UI:
      return "RGB16UI";
    case GPU_RGB16:
      return "RGB16";
    case GPU_RGBA16_SNORM:
      return "RGBA16_SNORM";
    case GPU_RGBA8_SNORM:
      return "RGBA8_SNORM";
    case GPU_RGB32F:
      return "RGB32F";
    case GPU_RGB32I:
      return "RGB32I";
    case GPU_RGB32UI:
      return "RGB32UI";
    case GPU_RGB8_SNORM:
      return "RGB8_SNORM";
    case GPU_RGB8:
      return "RGB8";
    case GPU_RGB8I:
      return "RGB8I";
    case GPU_RGB8UI:
      return "RGB8UI";
    case GPU_RG16_SNORM:
      return "RG16_SNORM";
    case GPU_RG8_SNORM:
      return "RG8_SNORM";
    case GPU_R16_SNORM:
      return "R16_SNORM";
    case GPU_R8_SNORM:
      return "R8_SNORM";
    /* Special formats, texture only. */
    case GPU_SRGB8_A8_DXT1:
      return "SRGB8_A8_DXT1";
    case GPU_SRGB8_A8_DXT3:
      return "SRGB8_A8_DXT3";
    case GPU_SRGB8_A8_DXT5:
      return "SRGB8_A8_DXT5";
    case GPU_RGBA8_DXT1:
      return "RGBA8_DXT1";
    case GPU_RGBA8_DXT3:
      return "RGBA8_DXT3";
    case GPU_RGBA8_DXT5:
      return "RGBA8_DXT5";
    case GPU_SRGB8:
      return "SRGB8";
    case GPU_RGB9_E5:
      return "RGB9_E5";
    /* Depth Formats. */
    case GPU_DEPTH_COMPONENT32F:
      return "DEPTH_COMPONENT32F";
    case GPU_DEPTH_COMPONENT24:
      return "DEPTH_COMPONENT24";
    case GPU_DEPTH_COMPONENT16:
      return "DEPTH_COMPONENT16";
  }
  BLI_assert_unreachable();
  return "";
}

bool GPU_texture_has_depth_format(const GPUTexture *tex)
{
  return (reinterpret_cast<const Texture *>(tex)->format_flag_get() & GPU_FORMAT_DEPTH) != 0;
}

bool GPU_texture_has_stencil_format(const GPUTexture *tex)
{
  return (reinterpret_cast<const Texture *>(tex)->format_flag_get() & GPU_FORMAT_STENCIL) != 0;
}

bool GPU_texture_has_integer_format(const GPUTexture *tex)
{
  return (reinterpret_cast<const Texture *>(tex)->format_flag_get() & GPU_FORMAT_INTEGER) != 0;
}

bool GPU_texture_has_float_format(const GPUTexture *tex)
{
  return (reinterpret_cast<const Texture *>(tex)->format_flag_get() & GPU_FORMAT_FLOAT) != 0;
}

bool GPU_texture_has_normalized_format(const GPUTexture *tex)
{
  return (reinterpret_cast<const Texture *>(tex)->format_flag_get() &
          GPU_FORMAT_NORMALIZED_INTEGER) != 0;
}

bool GPU_texture_has_signed_format(const GPUTexture *tex)
{
  return (reinterpret_cast<const Texture *>(tex)->format_flag_get() & GPU_FORMAT_SIGNED) != 0;
}

bool GPU_texture_is_cube(const GPUTexture *tex)
{
  return (reinterpret_cast<const Texture *>(tex)->type_get() & GPU_TEXTURE_CUBE) != 0;
}

bool GPU_texture_is_array(const GPUTexture *tex)
{
  return (reinterpret_cast<const Texture *>(tex)->type_get() & GPU_TEXTURE_ARRAY) != 0;
}

#ifndef GPU_NO_USE_PY_REFERENCES
void **GPU_texture_py_reference_get(GPUTexture *tex)
{
  return unwrap(tex)->py_ref;
}

void GPU_texture_py_reference_set(GPUTexture *tex, void **py_ref)
{
  BLI_assert(py_ref == nullptr || unwrap(tex)->py_ref == nullptr);
  unwrap(tex)->py_ref = py_ref;
}
#endif

/* TODO: remove. */
int GPU_texture_opengl_bindcode(const GPUTexture *tex)
{
  return reinterpret_cast<const Texture *>(tex)->gl_bindcode_get();
}

void GPU_texture_get_mipmap_size(GPUTexture *tex, int lvl, int *r_size)
{
  return reinterpret_cast<Texture *>(tex)->mip_size_get(lvl, r_size);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPU Pixel Buffer
 *
 * Pixel buffer utility functions.
 * \{ */

GPUPixelBuffer *GPU_pixel_buffer_create(size_t size)
{
  /* Ensure buffer satisfies the alignment of 256 bytes for copying
   * data between buffers and textures. As specified in:
   * https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
   *
   * Ensuring minimal size across all platforms handles cases for small-sized
   * textures and avoids issues with zero-sized buffers. */
  size = ceil_to_multiple_ul(size, 256);
  PixelBuffer *pixbuf = GPUBackend::get()->pixelbuf_alloc(size);
  return wrap(pixbuf);
}

void GPU_pixel_buffer_free(GPUPixelBuffer *pix_buf)
{
  PixelBuffer *handle = unwrap(pix_buf);
  delete handle;
}

void *GPU_pixel_buffer_map(GPUPixelBuffer *pix_buf)
{
  return reinterpret_cast<PixelBuffer *>(pix_buf)->map();
}

void GPU_pixel_buffer_unmap(GPUPixelBuffer *pix_buf)
{
  reinterpret_cast<PixelBuffer *>(pix_buf)->unmap();
}

size_t GPU_pixel_buffer_size(GPUPixelBuffer *pix_buf)
{
  return reinterpret_cast<PixelBuffer *>(pix_buf)->get_size();
}

int64_t GPU_pixel_buffer_get_native_handle(GPUPixelBuffer *pix_buf)
{
  return reinterpret_cast<PixelBuffer *>(pix_buf)->get_native_handle();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPU Sampler Objects
 *
 * Simple wrapper around opengl sampler objects.
 * Override texture sampler state for one sampler unit only.
 * \{ */

void GPU_samplers_update()
{
  /* Backend may not exist when we are updating preferences from background mode. */
  GPUBackend *backend = GPUBackend::get();
  if (backend) {
    backend->samplers_update();
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPU texture utilities
 * \{ */

size_t GPU_texture_component_len(eGPUTextureFormat tex_format)
{
  return to_component_len(tex_format);
}

size_t GPU_texture_dataformat_size(eGPUDataFormat data_format)
{
  return to_bytesize(data_format);
}

/** \} */
