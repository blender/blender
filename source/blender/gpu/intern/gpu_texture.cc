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

bool Texture::init_1D(int w, int layers, int mip_len, TextureFormat format)
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

bool Texture::init_2D(int w, int h, int layers, int mip_len, TextureFormat format)
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

bool Texture::init_3D(int w, int h, int d, int mip_len, TextureFormat format)
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

bool Texture::init_cubemap(int w, int layers, int mip_len, TextureFormat format)
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

bool Texture::init_buffer(VertBuf *vbo, TextureFormat format)
{
  /* See to_texture_format(). */
  w_ = GPU_vertbuf_get_vertex_len(vbo);
  h_ = 0;
  d_ = 0;
  format_ = format;
  format_flag_ = to_format_flag(format);
  type_ = GPU_TEXTURE_BUFFER;
  return this->init_internal(vbo);
}

bool Texture::init_view(Texture *src,
                        TextureFormat format,
                        GPUTextureType type,
                        int mip_start,
                        int mip_len,
                        int layer_start,
                        int layer_len,
                        bool cube_as_array,
                        bool use_stencil)
{
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
  return this->init_internal(src, mip_start, layer_start, use_stencil);
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

static inline gpu::Texture *gpu_texture_create(const char *name,
                                               const int w,
                                               const int h,
                                               const int d,
                                               const GPUTextureType type,
                                               int mip_len,
                                               TextureFormat tex_format,
                                               eGPUTextureUsage usage,
                                               const void *pixels,
                                               eGPUDataFormat data_format = GPU_DATA_FLOAT)
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
  return reinterpret_cast<gpu::Texture *>(tex);
}

gpu::Texture *GPU_texture_create_1d(const char *name,
                                    int width,
                                    int mip_len,
                                    TextureFormat format,
                                    eGPUTextureUsage usage,
                                    const float *data)
{
  return gpu_texture_create(name, width, 0, 0, GPU_TEXTURE_1D, mip_len, format, usage, data);
}

gpu::Texture *GPU_texture_create_1d_array(const char *name,
                                          int width,
                                          int layer_len,
                                          int mip_len,
                                          TextureFormat format,
                                          eGPUTextureUsage usage,
                                          const float *data)
{
  return gpu_texture_create(
      name, width, layer_len, 0, GPU_TEXTURE_1D_ARRAY, mip_len, format, usage, data);
}

gpu::Texture *GPU_texture_create_2d(const char *name,
                                    int width,
                                    int height,
                                    int mip_len,
                                    TextureFormat format,
                                    eGPUTextureUsage usage,
                                    const float *data)
{
  return gpu_texture_create(name, width, height, 0, GPU_TEXTURE_2D, mip_len, format, usage, data);
}

gpu::Texture *GPU_texture_create_2d_array(const char *name,
                                          int width,
                                          int height,
                                          int layer_len,
                                          int mip_len,
                                          TextureFormat format,
                                          eGPUTextureUsage usage,
                                          const float *data)
{
  return gpu_texture_create(
      name, width, height, layer_len, GPU_TEXTURE_2D_ARRAY, mip_len, format, usage, data);
}

gpu::Texture *GPU_texture_create_3d(const char *name,
                                    int width,
                                    int height,
                                    int depth,
                                    int mip_len,
                                    TextureFormat texture_format,
                                    eGPUTextureUsage usage,
                                    const void *data)
{
  return gpu_texture_create(
      name, width, height, depth, GPU_TEXTURE_3D, mip_len, texture_format, usage, data);
}

gpu::Texture *GPU_texture_create_cube(const char *name,
                                      int width,
                                      int mip_len,
                                      TextureFormat format,
                                      eGPUTextureUsage usage,
                                      const float *data)
{
  return gpu_texture_create(name, width, width, 0, GPU_TEXTURE_CUBE, mip_len, format, usage, data);
}

gpu::Texture *GPU_texture_create_cube_array(const char *name,
                                            int width,
                                            int layer_len,
                                            int mip_len,
                                            TextureFormat format,
                                            eGPUTextureUsage usage,
                                            const float *data)
{
  return gpu_texture_create(
      name, width, width, layer_len, GPU_TEXTURE_CUBE_ARRAY, mip_len, format, usage, data);
}

gpu::Texture *GPU_texture_create_compressed_2d(const char *name,
                                               int width,
                                               int height,
                                               int mip_len,
                                               TextureFormat tex_format,
                                               eGPUTextureUsage usage,
                                               const void *data)
{
  Texture *tex = GPUBackend::get()->texture_alloc(name);
  tex->usage_set(usage);
  bool success = tex->init_2D(width, height, 0, mip_len, tex_format);

  if (!success) {
    delete tex;
    return nullptr;
  }
  if (data) {
    size_t ofs = 0;
    for (int mip = 0; mip < mip_len; mip++) {
      int extent[3], offset[3] = {0, 0, 0};
      tex->mip_size_get(mip, extent);

      size_t size = ((extent[0] + 3) / 4) * ((extent[1] + 3) / 4) * to_block_size(tex_format);
      tex->update_sub(
          mip, offset, extent, to_texture_data_format(tex_format), (uchar *)data + ofs);

      ofs += size;
    }
  }
  return reinterpret_cast<gpu::Texture *>(tex);
}

gpu::Texture *GPU_texture_create_from_vertbuf(const char *name, gpu::VertBuf *vert)
{
#ifndef NDEBUG
  /* Vertex buffers used for texture buffers must be flagged with:
   * GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY. */
  BLI_assert_msg(vert->extended_usage_ & GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY,
                 "Vertex Buffers used for textures should have usage flag "
                 "GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY.");
#endif
  TextureFormat tex_format = to_texture_format(GPU_vertbuf_get_format(vert));
  Texture *tex = GPUBackend::get()->texture_alloc(name);

  bool success = tex->init_buffer(vert, tex_format);
  if (!success) {
    delete tex;
    return nullptr;
  }
  return reinterpret_cast<gpu::Texture *>(tex);
}

gpu::Texture *GPU_texture_create_error(int dimension, bool is_array)
{
  const float pixel[4] = {1.0f, 0.0f, 1.0f, 1.0f};
  int w = 1;
  int h = (dimension < 2 && !is_array) ? 0 : 1;
  int d = (dimension < 3 && !is_array) ? 0 : 1;

  GPUTextureType type = GPU_TEXTURE_3D;
  type = (dimension == 2) ? (is_array ? GPU_TEXTURE_2D_ARRAY : GPU_TEXTURE_2D) : type;
  type = (dimension == 1) ? (is_array ? GPU_TEXTURE_1D_ARRAY : GPU_TEXTURE_1D) : type;

  return gpu_texture_create("invalid_tex",
                            w,
                            h,
                            d,
                            type,
                            1,
                            TextureFormat::UNORM_8_8_8_8,
                            GPU_TEXTURE_USAGE_GENERAL,
                            pixel);
}

gpu::Texture *GPU_texture_create_view(const char *name,
                                      gpu::Texture *source_texture,
                                      TextureFormat view_format,
                                      int mip_start,
                                      int mip_len,
                                      int layer_start,
                                      int layer_len,
                                      bool cube_as_array,
                                      bool use_stencil)
{
  BLI_assert(mip_len > 0);
  BLI_assert(layer_len > 0);
  BLI_assert_msg(use_stencil == false ||
                     (GPU_texture_usage(source_texture) & GPU_TEXTURE_USAGE_FORMAT_VIEW),
                 "Source texture of TextureView must have GPU_TEXTURE_USAGE_FORMAT_VIEW usage "
                 "flag if view texture uses stencil texturing.");
  BLI_assert_msg((view_format == GPU_texture_format(source_texture)) ||
                     (GPU_texture_usage(source_texture) & GPU_TEXTURE_USAGE_FORMAT_VIEW),
                 "Source texture of TextureView must have GPU_TEXTURE_USAGE_FORMAT_VIEW usage "
                 "flag if view texture format is different.");
  Texture *view = GPUBackend::get()->texture_alloc(name);
  view->init_view(source_texture,
                  view_format,
                  source_texture->type_get(),
                  mip_start,
                  mip_len,
                  layer_start,
                  layer_len,
                  cube_as_array,
                  use_stencil);
  return view;
}

/* ------ Usage ------ */
eGPUTextureUsage GPU_texture_usage(const gpu::Texture *texture_)
{
  const Texture *tex = reinterpret_cast<const Texture *>(texture_);
  return tex->usage_get();
}

/* ------ Update ------ */

void GPU_texture_update_mipmap(gpu::Texture *texture,
                               int mip_level,
                               eGPUDataFormat data_format,
                               const void *pixels)
{
  int extent[3] = {1, 1, 1}, offset[3] = {0, 0, 0};
  texture->mip_size_get(mip_level, extent);
  texture->update_sub(mip_level, offset, extent, data_format, pixels);
}

void GPU_texture_update_sub(gpu::Texture *tex,
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
  tex->update_sub(0, offset, extent, data_format, pixels);
}

void GPU_texture_update_sub_from_pixel_buffer(gpu::Texture *texture,
                                              eGPUDataFormat data_format,
                                              GPUPixelBuffer *pixel_buf,
                                              int offset_x,
                                              int offset_y,
                                              int offset_z,
                                              int width,
                                              int height,
                                              int depth)
{
  int offset[3] = {offset_x, offset_y, offset_z};
  int extent[3] = {width, height, depth};
  texture->update_sub(offset, extent, data_format, pixel_buf);
}

void *GPU_texture_read(gpu::Texture *texture, eGPUDataFormat data_format, int mip_level)
{
  BLI_assert_msg(
      GPU_texture_usage(texture) & GPU_TEXTURE_USAGE_HOST_READ,
      "The host-read usage flag must be specified up-front. Only textures which require data "
      "reads should be flagged, allowing the backend to make certain optimizations.");
  return texture->read(mip_level, data_format);
}

void GPU_texture_clear(gpu::Texture *tex, eGPUDataFormat data_format, const void *data)
{
  BLI_assert(data != nullptr); /* Do not accept nullptr as parameter. */
  tex->clear(data_format, data);
}

void GPU_texture_update(gpu::Texture *tex, eGPUDataFormat data_format, const void *data)
{
  tex->update(data_format, data);
}

void GPU_unpack_row_length_set(uint len)
{
  Context::get()->state_manager->texture_unpack_row_length_set(len);
}

/* ------ Binding ------ */

void GPU_texture_bind_ex(gpu::Texture *texture, GPUSamplerState state, int unit)
{
  Texture *tex = texture;
  state = (state.type == GPU_SAMPLER_STATE_TYPE_INTERNAL) ? tex->sampler_state : state;
  Context::get()->state_manager->texture_bind(tex, state, unit);
}

void GPU_texture_bind(gpu::Texture *texture, int unit)
{
  Texture *tex = texture;
  Context::get()->state_manager->texture_bind(tex, tex->sampler_state, unit);
}

void GPU_texture_unbind(gpu::Texture *texture)
{
  Texture *tex = texture;
  Context::get()->state_manager->texture_unbind(tex);
}

void GPU_texture_unbind_all()
{
  Context::get()->state_manager->texture_unbind_all();
}

void GPU_texture_image_bind(gpu::Texture *tex, int unit)
{
  Context::get()->state_manager->image_bind(tex, unit);
}

void GPU_texture_image_unbind(gpu::Texture *tex)
{
  Context::get()->state_manager->image_unbind(tex);
}

void GPU_texture_image_unbind_all()
{
  Context::get()->state_manager->image_unbind_all();
}

void GPU_texture_update_mipmap_chain(gpu::Texture *tex)
{
  tex->generate_mipmap();
}

void GPU_texture_copy(gpu::Texture *dst_, gpu::Texture *src_)
{
  Texture *src = src_;
  Texture *dst = dst_;
  src->copy_to(dst);
}

void GPU_texture_compare_mode(gpu::Texture *texture, bool use_compare)
{
  Texture *tex = texture;
  /* Only depth formats does support compare mode. */
  BLI_assert(!(use_compare) || (tex->format_flag_get() & GPU_FORMAT_DEPTH));

  tex->sampler_state.type = use_compare ? GPU_SAMPLER_STATE_TYPE_CUSTOM :
                                          GPU_SAMPLER_STATE_TYPE_PARAMETERS;
  tex->sampler_state.custom_type = GPU_SAMPLER_CUSTOM_COMPARE;
}

void GPU_texture_filter_mode(gpu::Texture *texture, bool use_filter)
{
  Texture *tex = texture;
  /* Stencil and integer format does not support filtering. */
  BLI_assert(!(use_filter) ||
             !(tex->format_flag_get() & (GPU_FORMAT_STENCIL | GPU_FORMAT_INTEGER)));
  tex->sampler_state.set_filtering_flag_from_test(GPU_SAMPLER_FILTERING_LINEAR, use_filter);
}

void GPU_texture_mipmap_mode(gpu::Texture *texture, bool use_mipmap, bool use_filter)
{
  Texture *tex = texture;
  /* Stencil and integer format does not support filtering. */
  BLI_assert(!(use_filter || use_mipmap) ||
             !(tex->format_flag_get() & (GPU_FORMAT_STENCIL | GPU_FORMAT_INTEGER)));
  tex->sampler_state.set_filtering_flag_from_test(GPU_SAMPLER_FILTERING_MIPMAP, use_mipmap);
  tex->sampler_state.set_filtering_flag_from_test(GPU_SAMPLER_FILTERING_LINEAR, use_filter);
}

void GPU_texture_anisotropic_filter(gpu::Texture *texture, bool use_aniso)
{
  Texture *tex = texture;
  /* Stencil and integer format does not support filtering. */
  BLI_assert(!(use_aniso) ||
             !(tex->format_flag_get() & (GPU_FORMAT_STENCIL | GPU_FORMAT_INTEGER)));
  tex->sampler_state.set_filtering_flag_from_test(GPU_SAMPLER_FILTERING_ANISOTROPIC, use_aniso);
}

void GPU_texture_extend_mode_x(gpu::Texture *texture, GPUSamplerExtendMode extend_mode)
{
  texture->sampler_state.extend_x = extend_mode;
}

void GPU_texture_extend_mode_y(gpu::Texture *texture, GPUSamplerExtendMode extend_mode)
{
  texture->sampler_state.extend_yz = extend_mode;
}

void GPU_texture_extend_mode(gpu::Texture *texture, GPUSamplerExtendMode extend_mode)
{
  texture->sampler_state.extend_x = extend_mode;
  texture->sampler_state.extend_yz = extend_mode;
}

void GPU_texture_swizzle_set(gpu::Texture *texture, const char swizzle[4])
{
  texture->swizzle_set(swizzle);
}

void GPU_texture_free(gpu::Texture *texture)
{
  Texture *tex = texture;
  tex->refcount--;

  if (tex->refcount < 0) {
    fprintf(stderr, "gpu::Texture: negative refcount\n");
  }

  if (tex->refcount == 0) {
    delete tex;
  }
}

void GPU_texture_ref(gpu::Texture *texture)
{
  texture->refcount++;
}

int GPU_texture_dimensions(const gpu::Texture *texture)
{
  GPUTextureType type = texture->type_get();
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

int GPU_texture_width(const gpu::Texture *texture)
{
  return texture->width_get();
}

int GPU_texture_height(const gpu::Texture *texture)
{
  return texture->height_get();
}

int GPU_texture_depth(const gpu::Texture *texture)
{
  return texture->depth_get();
}

int GPU_texture_layer_count(const gpu::Texture *texture)
{
  return texture->layer_count();
}

int GPU_texture_mip_count(const gpu::Texture *texture)
{
  return texture->mip_count();
}

int GPU_texture_original_width(const gpu::Texture *texture)
{
  return texture->src_w;
}

int GPU_texture_original_height(const gpu::Texture *texture)
{
  return texture->src_h;
}

void GPU_texture_original_size_set(gpu::Texture *texture, int w, int h)
{
  texture->src_w = w;
  texture->src_h = h;
}

TextureFormat GPU_texture_format(const gpu::Texture *texture)
{
  return texture->format_get();
}

const char *GPU_texture_format_name(TextureFormat texture_format)
{
  switch (texture_format) {
    /* Formats texture & render-buffer */
    case TextureFormat::UINT_8_8_8_8:
      return "RGBA8UI";
    case TextureFormat::SINT_8_8_8_8:
      return "RGBA8I";
    case TextureFormat::UNORM_8_8_8_8:
      return "RGBA8";
    case TextureFormat::UINT_32_32_32_32:
      return "RGBA32UI";
    case TextureFormat::SINT_32_32_32_32:
      return "RGBA32I";
    case TextureFormat::SFLOAT_32_32_32_32:
      return "RGBA32F";
    case TextureFormat::UINT_16_16_16_16:
      return "RGBA16UI";
    case TextureFormat::SINT_16_16_16_16:
      return "RGBA16I";
    case TextureFormat::SFLOAT_16_16_16_16:
      return "RGBA16F";
    case TextureFormat::UNORM_16_16_16_16:
      return "RGBA16";
    case TextureFormat::UINT_8_8:
      return "RG8UI";
    case TextureFormat::SINT_8_8:
      return "RG8I";
    case TextureFormat::UNORM_8_8:
      return "RG8";
    case TextureFormat::UINT_32_32:
      return "RG32UI";
    case TextureFormat::SINT_32_32:
      return "RG32I";
    case TextureFormat::SFLOAT_32_32:
      return "RG32F";
    case TextureFormat::UINT_16_16:
      return "RG16UI";
    case TextureFormat::SINT_16_16:
      return "RG16I";
    case TextureFormat::SFLOAT_16_16:
      return "RG16F";
    case TextureFormat::UNORM_16_16:
      return "RG16";
    case TextureFormat::UINT_8:
      return "R8UI";
    case TextureFormat::SINT_8:
      return "R8I";
    case TextureFormat::UNORM_8:
      return "R8";
    case TextureFormat::UINT_32:
      return "R32UI";
    case TextureFormat::SINT_32:
      return "R32I";
    case TextureFormat::SFLOAT_32:
      return "R32F";
    case TextureFormat::UINT_16:
      return "R16UI";
    case TextureFormat::SINT_16:
      return "R16I";
    case TextureFormat::SFLOAT_16:
      return "R16F";
    case TextureFormat::UNORM_16:
      return "R16";
    /* Special formats texture & render-buffer */
    case TextureFormat::UNORM_10_10_10_2:
      return "RGB10_A2";
    case TextureFormat::UINT_10_10_10_2:
      return "RGB10_A2UI";
    case TextureFormat::UFLOAT_11_11_10:
      return "R11F_G11F_B10F";
    case TextureFormat::SFLOAT_32_DEPTH_UINT_8:
      return "DEPTH32F_STENCIL8";
    case TextureFormat::SRGBA_8_8_8_8:
      return "SRGB8_A8";
    /* Texture only formats. */
    case TextureFormat::SFLOAT_16_16_16:
      return "RGB16F";
    case TextureFormat::SNORM_16_16_16:
      return "RGB16_SNORM";
    case TextureFormat::SINT_16_16_16:
      return "RGB16I";
    case TextureFormat::UINT_16_16_16:
      return "RGB16UI";
    case TextureFormat::UNORM_16_16_16:
      return "RGB16";
    case TextureFormat::SNORM_16_16_16_16:
      return "RGBA16_SNORM";
    case TextureFormat::SNORM_8_8_8_8:
      return "RGBA8_SNORM";
    case TextureFormat::SFLOAT_32_32_32:
      return "RGB32F";
    case TextureFormat::SINT_32_32_32:
      return "RGB32I";
    case TextureFormat::UINT_32_32_32:
      return "RGB32UI";
    case TextureFormat::SNORM_8_8_8:
      return "RGB8_SNORM";
    case TextureFormat::UNORM_8_8_8:
      return "RGB8";
    case TextureFormat::SINT_8_8_8:
      return "RGB8I";
    case TextureFormat::UINT_8_8_8:
      return "RGB8UI";
    case TextureFormat::SNORM_16_16:
      return "RG16_SNORM";
    case TextureFormat::SNORM_8_8:
      return "RG8_SNORM";
    case TextureFormat::SNORM_16:
      return "R16_SNORM";
    case TextureFormat::SNORM_8:
      return "R8_SNORM";
    /* Special formats, texture only. */
    case TextureFormat::SRGB_DXT1:
      return "SRGB8_A8_DXT1";
    case TextureFormat::SRGB_DXT3:
      return "SRGB8_A8_DXT3";
    case TextureFormat::SRGB_DXT5:
      return "SRGB8_A8_DXT5";
    case TextureFormat::SNORM_DXT1:
      return "RGBA8_DXT1";
    case TextureFormat::SNORM_DXT3:
      return "RGBA8_DXT3";
    case TextureFormat::SNORM_DXT5:
      return "RGBA8_DXT5";
    case TextureFormat::SRGBA_8_8_8:
      return "SRGB8";
    case TextureFormat::UFLOAT_9_9_9_EXP_5:
      return "RGB9_E5";
    /* Depth Formats. */
    case TextureFormat::SFLOAT_32_DEPTH:
      return "DEPTH_COMPONENT32F";
    case TextureFormat::UNORM_16_DEPTH:
      return "DEPTH_COMPONENT16";

    case TextureFormat::Invalid:
      BLI_assert_unreachable();
      return "Invalid";
  }
  BLI_assert_unreachable();
  return "";
}

bool GPU_texture_has_depth_format(const gpu::Texture *texture)
{
  return (texture->format_flag_get() & GPU_FORMAT_DEPTH) != 0;
}

bool GPU_texture_has_stencil_format(const gpu::Texture *texture)
{
  return (texture->format_flag_get() & GPU_FORMAT_STENCIL) != 0;
}

bool GPU_texture_has_integer_format(const gpu::Texture *texture)
{
  return (texture->format_flag_get() & GPU_FORMAT_INTEGER) != 0;
}

bool GPU_texture_has_float_format(const gpu::Texture *texture)
{
  return (texture->format_flag_get() & GPU_FORMAT_FLOAT) != 0;
}

bool GPU_texture_has_normalized_format(const gpu::Texture *texture)
{
  return (texture->format_flag_get() & GPU_FORMAT_NORMALIZED_INTEGER) != 0;
}

bool GPU_texture_has_signed_format(const gpu::Texture *texture)
{
  return (texture->format_flag_get() & GPU_FORMAT_SIGNED) != 0;
}

bool GPU_texture_is_cube(const gpu::Texture *texture)
{
  return (texture->type_get() & GPU_TEXTURE_CUBE) != 0;
}

bool GPU_texture_is_array(const gpu::Texture *texture)
{
  return (texture->type_get() & GPU_TEXTURE_ARRAY) != 0;
}

#ifndef GPU_NO_USE_PY_REFERENCES
void **GPU_texture_py_reference_get(gpu::Texture *texture)
{
  return texture->py_ref;
}

void GPU_texture_py_reference_set(gpu::Texture *texture, void **py_ref)
{
  BLI_assert(py_ref == nullptr || texture->py_ref == nullptr);
  texture->py_ref = py_ref;
}
#endif

void GPU_texture_get_mipmap_size(gpu::Texture *texture, int mip_level, int *r_size)
{
  texture->mip_size_get(mip_level, r_size);
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

void GPU_pixel_buffer_free(GPUPixelBuffer *pixel_buf)
{
  PixelBuffer *handle = unwrap(pixel_buf);
  delete handle;
}

void *GPU_pixel_buffer_map(GPUPixelBuffer *pixel_buf)
{
  return unwrap(pixel_buf)->map();
}

void GPU_pixel_buffer_unmap(GPUPixelBuffer *pixel_buf)
{
  unwrap(pixel_buf)->unmap();
}

size_t GPU_pixel_buffer_size(GPUPixelBuffer *pixel_buf)
{
  return unwrap(pixel_buf)->get_size();
}

GPUPixelBufferNativeHandle GPU_pixel_buffer_get_native_handle(GPUPixelBuffer *pixel_buf)
{
  return unwrap(pixel_buf)->get_native_handle();
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

size_t GPU_texture_component_len(TextureFormat tex_format)
{
  return to_component_len(tex_format);
}

size_t GPU_texture_dataformat_size(eGPUDataFormat data_format)
{
  return to_bytesize(data_format);
}

/** \} */
