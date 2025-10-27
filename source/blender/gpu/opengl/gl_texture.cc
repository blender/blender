/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include <string>

#include "BLI_assert.h"
#include "BLI_math_half.hh"
#include "BLI_string.h"

#include "DNA_userdef_types.h"

#include "GPU_capabilities.hh"
#include "GPU_framebuffer.hh"
#include "GPU_platform.hh"

#include "GPU_vertex_buffer.hh" /* TODO: should be `gl_vertex_buffer.hh`. */
#include "gl_backend.hh"
#include "gl_debug.hh"
#include "gl_state.hh"

#include "gl_texture.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

GLTexture::GLTexture(const char *name) : Texture(name)
{
  BLI_assert(GLContext::get() != nullptr);

  glGenTextures(1, &tex_id_);
}

GLTexture::~GLTexture()
{
  if (framebuffer_) {
    GPU_framebuffer_free(framebuffer_);
  }
  GLContext *ctx = GLContext::get();
  if (ctx != nullptr && is_bound_) {
    /* This avoid errors when the texture is still inside the bound texture array. */
    ctx->state_manager->texture_unbind(this);
    ctx->state_manager->image_unbind(this);
  }
  GLContext::texture_free(tex_id_);
}

bool GLTexture::init_internal()
{
  target_ = to_gl_target(type_);

  /* We need to bind once to define the texture type. */
  GLContext::state_manager_active_get()->texture_bind_temp(this);

  if (!this->proxy_check(0)) {
    return false;
  }

  GLenum internal_format = to_gl_internal_format(format_);
  const bool is_cubemap = bool(type_ == GPU_TEXTURE_CUBE);
  const int dimensions = (is_cubemap) ? 2 : this->dimensions_count();

  switch (dimensions) {
    default:
    case 1:
      glTexStorage1D(target_, mipmaps_, internal_format, w_);
      break;
    case 2:
      glTexStorage2D(target_, mipmaps_, internal_format, w_, h_);
      break;
    case 3:
      glTexStorage3D(target_, mipmaps_, internal_format, w_, h_, d_);
      break;
  }
  this->mip_range_set(0, mipmaps_ - 1);

  /* Avoid issue with formats not supporting filtering. Nearest by default. */
  if (GLContext::direct_state_access_support) {
    glTextureParameteri(tex_id_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  }
  else {
    glTexParameteri(target_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  }

  debug::object_label(GL_TEXTURE, tex_id_, name_);
  return true;
}

bool GLTexture::init_internal(VertBuf *vbo)
{
  GLVertBuf *gl_vbo = static_cast<GLVertBuf *>(vbo);
  target_ = to_gl_target(type_);

  /* We need to bind once to define the texture type. */
  GLContext::state_manager_active_get()->texture_bind_temp(this);

  GLenum internal_format = to_gl_internal_format(format_);

  if (GLContext::direct_state_access_support) {
    glTextureBuffer(tex_id_, internal_format, gl_vbo->vbo_id_);
  }
  else {
    glTexBuffer(target_, internal_format, gl_vbo->vbo_id_);
  }

  debug::object_label(GL_TEXTURE, tex_id_, name_);

  return true;
}

bool GLTexture::init_internal(gpu::Texture *src,
                              int mip_offset,
                              int layer_offset,
                              bool use_stencil)
{
  const GLTexture *gl_src = static_cast<const GLTexture *>(src);
  GLenum internal_format = to_gl_internal_format(format_);
  target_ = to_gl_target(type_);

  glTextureView(tex_id_,
                target_,
                gl_src->tex_id_,
                internal_format,
                mip_offset,
                mipmaps_,
                layer_offset,
                this->layer_count());

  debug::object_label(GL_TEXTURE, tex_id_, name_);

  /* Stencil view support. */
  if (ELEM(format_, TextureFormat::SFLOAT_32_DEPTH_UINT_8)) {
    stencil_texture_mode_set(use_stencil);
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operations
 * \{ */

void GLTexture::update_sub_direct_state_access(
    int mip, int offset[3], int extent[3], GLenum format, GLenum type, const void *data)
{
  if (format_flag_ & GPU_FORMAT_COMPRESSED) {
    size_t size = ((extent[0] + 3) / 4) * ((extent[1] + 3) / 4) * to_block_size(format_);
    switch (this->dimensions_count()) {
      default:
      case 1:
        glCompressedTextureSubImage1D(tex_id_, mip, offset[0], extent[0], format, size, data);
        break;
      case 2:
        glCompressedTextureSubImage2D(
            tex_id_, mip, UNPACK2(offset), UNPACK2(extent), format, size, data);
        break;
      case 3:
        glCompressedTextureSubImage3D(
            tex_id_, mip, UNPACK3(offset), UNPACK3(extent), format, size, data);
        break;
    }
  }
  else {
    switch (this->dimensions_count()) {
      default:
      case 1:
        glTextureSubImage1D(tex_id_, mip, offset[0], extent[0], format, type, data);
        break;
      case 2:
        glTextureSubImage2D(tex_id_, mip, UNPACK2(offset), UNPACK2(extent), format, type, data);
        break;
      case 3:
        glTextureSubImage3D(tex_id_, mip, UNPACK3(offset), UNPACK3(extent), format, type, data);
        break;
    }
  }

  has_pixels_ = true;
}

void GLTexture::update_sub(
    int mip, int offset[3], int extent[3], eGPUDataFormat type, const void *data)
{
  BLI_assert(validate_data_format(format_, type));
  BLI_assert(data != nullptr);

  if (mip >= mipmaps_) {
    debug::raise_gl_error("Updating a miplvl on a texture too small to have this many levels.");
    return;
  }

  std::unique_ptr<uint16_t, MEM_freeN_smart_ptr_deleter> clamped_half_buffer = nullptr;

  if (data != nullptr && type == GPU_DATA_FLOAT && is_half_float(format_)) {
    size_t pixel_count = max_ii(extent[0], 1) * max_ii(extent[1], 1) * max_ii(extent[2], 1);
    size_t total_component_count = to_component_len(format_) * pixel_count;

    clamped_half_buffer.reset(
        (uint16_t *)MEM_mallocN_aligned(sizeof(uint16_t) * total_component_count, 128, __func__));

    Span<float> src(static_cast<const float *>(data), total_component_count);
    MutableSpan<uint16_t> dst(static_cast<uint16_t *>(clamped_half_buffer.get()),
                              total_component_count);

    constexpr int64_t chunk_size = 4 * 1024 * 1024;

    threading::parallel_for(
        IndexRange(total_component_count), chunk_size, [&](const IndexRange range) {
          /* Doing float to half conversion manually to avoid implementation specific behavior
           * regarding Inf and NaNs. Use make finite version to avoid unexpected black pixels on
           * certain implementation. For platform parity we clamp these infinite values to finite
           * values. */
          blender::math::float_to_half_make_finite_array(
              src.slice(range).data(), dst.slice(range).data(), range.size());
        });
    data = clamped_half_buffer.get();
    type = GPU_DATA_HALF_FLOAT;
  }

  const int dimensions = this->dimensions_count();
  GLenum gl_format = to_gl_data_format(format_);
  GLenum gl_type = to_gl(type);

  /* Some drivers have issues with cubemap & glTextureSubImage3D even if it is correct. */
  if (GLContext::direct_state_access_support && (type_ != GPU_TEXTURE_CUBE)) {
    this->update_sub_direct_state_access(mip, offset, extent, gl_format, gl_type, data);
    return;
  }

  GLContext::state_manager_active_get()->texture_bind_temp(this);
  if (type_ == GPU_TEXTURE_CUBE) {
    for (int i = 0; i < extent[2]; i++) {
      GLenum target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + offset[2] + i;
      glTexSubImage2D(target, mip, UNPACK2(offset), UNPACK2(extent), gl_format, gl_type, data);
    }
  }
  else if (format_flag_ & GPU_FORMAT_COMPRESSED) {
    size_t size = ((extent[0] + 3) / 4) * ((extent[1] + 3) / 4) * to_block_size(format_);
    switch (dimensions) {
      default:
      case 1:
        glCompressedTexSubImage1D(target_, mip, offset[0], extent[0], gl_format, size, data);
        break;
      case 2:
        glCompressedTexSubImage2D(
            target_, mip, UNPACK2(offset), UNPACK2(extent), gl_format, size, data);
        break;
      case 3:
        glCompressedTexSubImage3D(
            target_, mip, UNPACK3(offset), UNPACK3(extent), gl_format, size, data);
        break;
    }
  }
  else {
    switch (dimensions) {
      default:
      case 1:
        glTexSubImage1D(target_, mip, offset[0], extent[0], gl_format, gl_type, data);
        break;
      case 2:
        glTexSubImage2D(target_, mip, UNPACK2(offset), UNPACK2(extent), gl_format, gl_type, data);
        break;
      case 3:
        glTexSubImage3D(target_, mip, UNPACK3(offset), UNPACK3(extent), gl_format, gl_type, data);
        break;
    }
  }

  has_pixels_ = true;
}

void GLTexture::update_sub(int offset[3],
                           int extent[3],
                           eGPUDataFormat format,
                           GPUPixelBuffer *pixbuf)
{
  /* Update texture from pixel buffer. */
  BLI_assert(validate_data_format(format_, format));
  BLI_assert(pixbuf != nullptr);

  const int dimensions = this->dimensions_count();
  GLenum gl_format = to_gl_data_format(format_);
  GLenum gl_type = to_gl(format);

  /* Temporarily Bind texture. */
  GLContext::state_manager_active_get()->texture_bind_temp(this);

  /* Bind pixel buffer for source data. */
  GLint pix_buf_handle = (GLint)GPU_pixel_buffer_get_native_handle(pixbuf).handle;
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pix_buf_handle);

  switch (dimensions) {
    default:
    case 1:
      glTexSubImage1D(target_, 0, offset[0], extent[0], gl_format, gl_type, nullptr);
      break;
    case 2:
      glTexSubImage2D(target_, 0, UNPACK2(offset), UNPACK2(extent), gl_format, gl_type, nullptr);
      break;
    case 3:
      glTexSubImage3D(target_, 0, UNPACK3(offset), UNPACK3(extent), gl_format, gl_type, nullptr);
      break;
  }

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void GLTexture::generate_mipmap()
{
  /* Allow users to provide mipmaps stored in compressed textures.
   * Skip generating mipmaps to avoid overriding the existing ones. */
  if (format_flag_ & GPU_FORMAT_COMPRESSED) {
    return;
  }

  /* Some drivers have bugs when using #glGenerateMipmap with depth textures (see #56789).
   * In this case we just create a complete texture with mipmaps manually without
   * down-sampling. You must initialize the texture levels using other methods. */
  if (format_flag_ & GPU_FORMAT_DEPTH) {
    return;
  }

  if (GLContext::generate_mipmap_workaround) {
    /* Broken glGenerateMipmap, don't call it and render without mipmaps.
     * If no top level pixels have been filled in, the levels will get filled by
     * other means and there is no need to disable mipmapping. */
    if (has_pixels_) {
      this->mip_range_set(0, 0);
    }
    return;
  }

  /* Down-sample from mip 0 using implementation. */
  if (GLContext::direct_state_access_support) {
    glGenerateTextureMipmap(tex_id_);
  }
  else {
    GLContext::state_manager_active_get()->texture_bind_temp(this);
    glGenerateMipmap(target_);
  }
}

void GLTexture::clear(eGPUDataFormat data_format, const void *data)
{
  BLI_assert(validate_data_format(format_, data_format));

  /* Note: do not use glClearTexImage, even if it is available (via
   * extension or GL 4.4). It causes GL framebuffer binding to be
   * way slower at least on some drivers (e.g. Win10 / NV RTX 3080,
   * but also reportedly others), as if glClearTexImage causes
   * "pixel data" to exist which is then uploaded CPU -> GPU at bind
   * time. */

  gpu::FrameBuffer *prev_fb = GPU_framebuffer_active_get();

  FrameBuffer *fb = this->framebuffer_get();
  fb->bind(true);
  fb->clear_attachment(this->attachment_type(0), data_format, data);

  GPU_framebuffer_bind(prev_fb);
}

void GLTexture::copy_to(Texture *dst_)
{
  GLTexture *dst = static_cast<GLTexture *>(dst_);
  GLTexture *src = this;

  BLI_assert((dst->w_ == src->w_) && (dst->h_ == src->h_) && (dst->d_ == src->d_));
  BLI_assert(dst->format_ == src->format_);
  BLI_assert(dst->type_ == src->type_);

  int mip = 0;
  /* NOTE: mip_size_get() won't override any dimension that is equal to 0. */
  int extent[3] = {1, 1, 1};
  this->mip_size_get(mip, extent);
  glCopyImageSubData(
      src->tex_id_, target_, mip, 0, 0, 0, dst->tex_id_, target_, mip, 0, 0, 0, UNPACK3(extent));

  has_pixels_ = true;
}

void *GLTexture::read(int mip, eGPUDataFormat type)
{
  BLI_assert(!(format_flag_ & GPU_FORMAT_COMPRESSED));
  BLI_assert(mip <= mipmaps_ || mip == 0);
  BLI_assert(validate_data_format(format_, type));

  /* NOTE: mip_size_get() won't override any dimension that is equal to 0. */
  int extent[3] = {1, 1, 1};
  this->mip_size_get(mip, extent);

  size_t sample_len = extent[0] * extent[1] * extent[2];
  size_t sample_size = to_bytesize(format_, type);
  size_t texture_size = sample_len * sample_size;

  /* AMD Pro driver have a bug that write 8 bytes past buffer size
   * if the texture is big. (see #66573) */
  void *data = MEM_mallocN(texture_size + 8, "GPU_texture_read");

  GLenum gl_format = to_gl_data_format(
      format_ == TextureFormat::SFLOAT_32_DEPTH_UINT_8 ? TextureFormat::SFLOAT_32_DEPTH : format_);
  GLenum gl_type = to_gl(type);

  if (GLContext::direct_state_access_support) {
    glGetTextureImage(tex_id_, mip, gl_format, gl_type, texture_size, data);
  }
  else {
    GLContext::state_manager_active_get()->texture_bind_temp(this);
    if (type_ == GPU_TEXTURE_CUBE) {
      size_t cube_face_size = texture_size / 6;
      char *pdata = (char *)data;
      for (int i = 0; i < 6; i++, pdata += cube_face_size) {
        glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, mip, gl_format, gl_type, pdata);
      }
    }
    else {
      glGetTexImage(target_, mip, gl_format, gl_type, data);
    }
  }
  return data;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Getters & setters
 * \{ */

void GLTexture::swizzle_set(const char swizzle[4])
{
  GLint gl_swizzle[4] = {(GLint)swizzle_to_gl(swizzle[0]),
                         (GLint)swizzle_to_gl(swizzle[1]),
                         (GLint)swizzle_to_gl(swizzle[2]),
                         (GLint)swizzle_to_gl(swizzle[3])};
  if (GLContext::direct_state_access_support) {
    glTextureParameteriv(tex_id_, GL_TEXTURE_SWIZZLE_RGBA, gl_swizzle);
  }
  else {
    GLContext::state_manager_active_get()->texture_bind_temp(this);
    glTexParameteriv(target_, GL_TEXTURE_SWIZZLE_RGBA, gl_swizzle);
  }
}

void GLTexture::stencil_texture_mode_set(bool use_stencil)
{
  BLI_assert(GLContext::stencil_texturing_support);
  GLint value = use_stencil ? GL_STENCIL_INDEX : GL_DEPTH_COMPONENT;
  if (GLContext::direct_state_access_support) {
    glTextureParameteri(tex_id_, GL_DEPTH_STENCIL_TEXTURE_MODE, value);
  }
  else {
    GLContext::state_manager_active_get()->texture_bind_temp(this);
    glTexParameteri(target_, GL_DEPTH_STENCIL_TEXTURE_MODE, value);
  }
}

void GLTexture::mip_range_set(int min, int max)
{
  BLI_assert(min <= max && min >= 0 && max <= mipmaps_);
  mip_min_ = min;
  mip_max_ = max;
  if (GLContext::direct_state_access_support) {
    glTextureParameteri(tex_id_, GL_TEXTURE_BASE_LEVEL, min);
    glTextureParameteri(tex_id_, GL_TEXTURE_MAX_LEVEL, max);
  }
  else {
    GLContext::state_manager_active_get()->texture_bind_temp(this);
    glTexParameteri(target_, GL_TEXTURE_BASE_LEVEL, min);
    glTexParameteri(target_, GL_TEXTURE_MAX_LEVEL, max);
  }
}

FrameBuffer *GLTexture::framebuffer_get()
{
  if (framebuffer_) {
    return framebuffer_;
  }
  BLI_assert(!(type_ & GPU_TEXTURE_1D));
  framebuffer_ = GPU_framebuffer_create(name_);
  framebuffer_->attachment_set(this->attachment_type(0), GPU_ATTACHMENT_TEXTURE(this));
  has_pixels_ = true;
  return framebuffer_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sampler objects
 * \{ */

/** A function that maps GPUSamplerExtendMode values to their OpenGL enum counterparts. */
static inline GLenum to_gl(GPUSamplerExtendMode extend_mode)
{
  switch (extend_mode) {
    case GPU_SAMPLER_EXTEND_MODE_EXTEND:
      return GL_CLAMP_TO_EDGE;
    case GPU_SAMPLER_EXTEND_MODE_REPEAT:
      return GL_REPEAT;
    case GPU_SAMPLER_EXTEND_MODE_MIRRORED_REPEAT:
      return GL_MIRRORED_REPEAT;
    case GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER:
      return GL_CLAMP_TO_BORDER;
    default:
      BLI_assert_unreachable();
      return GL_CLAMP_TO_EDGE;
  }
}

GLuint GLTexture::samplers_state_cache_[GPU_SAMPLER_EXTEND_MODES_COUNT]
                                       [GPU_SAMPLER_EXTEND_MODES_COUNT]
                                       [GPU_SAMPLER_FILTERING_TYPES_COUNT] = {};
GLuint GLTexture::custom_samplers_state_cache_[GPU_SAMPLER_CUSTOM_TYPES_COUNT] = {};

void GLTexture::samplers_init()
{
  glGenSamplers(samplers_state_cache_count_, &samplers_state_cache_[0][0][0]);

  for (int extend_yz_i = 0; extend_yz_i < GPU_SAMPLER_EXTEND_MODES_COUNT; extend_yz_i++) {
    const GPUSamplerExtendMode extend_yz = static_cast<GPUSamplerExtendMode>(extend_yz_i);
    const GLenum extend_t = to_gl(extend_yz);

    for (int extend_x_i = 0; extend_x_i < GPU_SAMPLER_EXTEND_MODES_COUNT; extend_x_i++) {
      const GPUSamplerExtendMode extend_x = static_cast<GPUSamplerExtendMode>(extend_x_i);
      const GLenum extend_s = to_gl(extend_x);

      for (int filtering_i = 0; filtering_i < GPU_SAMPLER_FILTERING_TYPES_COUNT; filtering_i++) {
        const GPUSamplerFiltering filtering = GPUSamplerFiltering(filtering_i);

        const GLenum mag_filter = (filtering & GPU_SAMPLER_FILTERING_LINEAR) ? GL_LINEAR :
                                                                               GL_NEAREST;
        const GLenum linear_min_filter = (filtering & GPU_SAMPLER_FILTERING_MIPMAP) ?
                                             GL_LINEAR_MIPMAP_LINEAR :
                                             GL_LINEAR;
        const GLenum nearest_min_filter = (filtering & GPU_SAMPLER_FILTERING_MIPMAP) ?
                                              GL_NEAREST_MIPMAP_LINEAR :
                                              GL_NEAREST;
        const GLenum min_filter = (filtering & GPU_SAMPLER_FILTERING_LINEAR) ? linear_min_filter :
                                                                               nearest_min_filter;

        GLuint sampler = samplers_state_cache_[extend_yz_i][extend_x_i][filtering_i];
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, extend_s);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, extend_t);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, extend_t);
        glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, min_filter);
        glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, mag_filter);

        /* Other states are left to default:
         * - GL_TEXTURE_BORDER_COLOR is {0, 0, 0, 0}.
         * - GL_TEXTURE_MIN_LOD is -1000.
         * - GL_TEXTURE_MAX_LOD is 1000.
         * - GL_TEXTURE_LOD_BIAS is 0.0f.
         */

        const GPUSamplerState sampler_state = {filtering, extend_x, extend_yz};
        const std::string sampler_name = sampler_state.to_string();
        debug::object_label(GL_SAMPLER, sampler, sampler_name.c_str());
      }
    }
  }
  samplers_update();

  glGenSamplers(GPU_SAMPLER_CUSTOM_TYPES_COUNT, custom_samplers_state_cache_);

  /* Compare sampler for depth textures. */
  GLuint compare_sampler = custom_samplers_state_cache_[GPU_SAMPLER_CUSTOM_COMPARE];
  glSamplerParameteri(compare_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glSamplerParameteri(compare_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glSamplerParameteri(compare_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glSamplerParameteri(compare_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glSamplerParameteri(compare_sampler, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glSamplerParameteri(compare_sampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
  glSamplerParameteri(compare_sampler, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

  debug::object_label(GL_SAMPLER, compare_sampler, "compare");

  /* Custom sampler for icons. The icon texture is sampled within the shader using a -0.5f LOD
   * bias. */
  GLuint icon_sampler = custom_samplers_state_cache_[GPU_SAMPLER_CUSTOM_ICON];
  glSamplerParameteri(icon_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
  glSamplerParameteri(icon_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  debug::object_label(GL_SAMPLER, icon_sampler, "icons");
}

void GLTexture::samplers_update()
{
  if (!GLContext::texture_filter_anisotropic_support) {
    return;
  }

  float max_anisotropy = 1.0f;
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropy);

  const float anisotropic_filter = min_ff(max_anisotropy, U.anisotropic_filter);

  for (int extend_yz_i = 0; extend_yz_i < GPU_SAMPLER_EXTEND_MODES_COUNT; extend_yz_i++) {
    for (int extend_x_i = 0; extend_x_i < GPU_SAMPLER_EXTEND_MODES_COUNT; extend_x_i++) {
      for (int filtering_i = 0; filtering_i < GPU_SAMPLER_FILTERING_TYPES_COUNT; filtering_i++) {
        const GPUSamplerFiltering filtering = GPUSamplerFiltering(filtering_i);

        if ((filtering & GPU_SAMPLER_FILTERING_ANISOTROPIC) &&
            (filtering & GPU_SAMPLER_FILTERING_MIPMAP))
        {
          glSamplerParameterf(samplers_state_cache_[extend_yz_i][extend_x_i][filtering_i],
                              GL_TEXTURE_MAX_ANISOTROPY_EXT,
                              anisotropic_filter);
        }
      }
    }
  }
}

void GLTexture::samplers_free()
{
  glDeleteSamplers(samplers_state_cache_count_, &samplers_state_cache_[0][0][0]);
  glDeleteSamplers(GPU_SAMPLER_CUSTOM_TYPES_COUNT, custom_samplers_state_cache_);
}

GLuint GLTexture::get_sampler(const GPUSamplerState &sampler_state)
{
  /* Internal sampler states are signal values and do not correspond to actual samplers. */
  BLI_assert(sampler_state.type != GPU_SAMPLER_STATE_TYPE_INTERNAL);

  if (sampler_state.type == GPU_SAMPLER_STATE_TYPE_CUSTOM) {
    return custom_samplers_state_cache_[sampler_state.custom_type];
  }

  return samplers_state_cache_[sampler_state.extend_yz][sampler_state.extend_x]
                              [sampler_state.filtering];
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Proxy texture
 *
 * Dummy texture to see if the implementation supports the requested size.
 * \{ */

bool GLTexture::proxy_check(int mip)
{
  /* NOTE: This only checks if this mipmap is valid / supported.
   * TODO(fclem): make the check cover the whole mipmap chain. */

  /* Manual validation first, since some implementation have issues with proxy creation. */
  int max_size = GPU_max_texture_size();
  int max_3d_size = GPU_max_texture_3d_size();
  int max_cube_size = GLContext::max_cubemap_size;
  int size[3] = {1, 1, 1};
  this->mip_size_get(mip, size);

  if (type_ & GPU_TEXTURE_ARRAY) {
    if (this->layer_count() > GPU_max_texture_layers()) {
      return false;
    }
  }

  if (type_ == GPU_TEXTURE_3D) {
    if (size[0] > max_3d_size || size[1] > max_3d_size || size[2] > max_3d_size) {
      return false;
    }
  }
  else if ((type_ & ~GPU_TEXTURE_ARRAY) == GPU_TEXTURE_2D) {
    if (size[0] > max_size || size[1] > max_size) {
      return false;
    }
  }
  else if ((type_ & ~GPU_TEXTURE_ARRAY) == GPU_TEXTURE_1D) {
    if (size[0] > max_size) {
      return false;
    }
  }
  else if ((type_ & ~GPU_TEXTURE_ARRAY) == GPU_TEXTURE_CUBE) {
    if (size[0] > max_cube_size) {
      return false;
    }
  }

  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_WIN, GPU_DRIVER_ANY) ||
      GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_UNIX, GPU_DRIVER_OFFICIAL))
  {
    /* Some AMD drivers have a faulty `GL_PROXY_TEXTURE_..` check.
     * (see #55888, #56185, #59351).
     * Checking with `GL_PROXY_TEXTURE_..` doesn't prevent `Out Of Memory` issue,
     * it just states that the OGL implementation can support the texture.
     * So we already manually check the maximum size and maximum number of layers.
     * Same thing happens on Nvidia/macOS 10.15 (#78175). */
    return true;
  }

  GLenum gl_proxy = to_gl_proxy(type_);
  GLenum internal_format = to_gl_internal_format(format_);
  GLenum gl_format = to_gl_data_format(format_);
  GLenum gl_type = to_gl(to_texture_data_format(format_));
  /* Small exception. */
  int dimensions = (type_ == GPU_TEXTURE_CUBE) ? 2 : this->dimensions_count();

  if (format_flag_ & GPU_FORMAT_COMPRESSED) {
    size_t img_size = ((size[0] + 3) / 4) * ((size[1] + 3) / 4) * to_block_size(format_);
    switch (dimensions) {
      default:
      case 1:
        glCompressedTexImage1D(gl_proxy, mip, size[0], 0, gl_format, img_size, nullptr);
        break;
      case 2:
        glCompressedTexImage2D(gl_proxy, mip, UNPACK2(size), 0, gl_format, img_size, nullptr);
        break;
      case 3:
        glCompressedTexImage3D(gl_proxy, mip, UNPACK3(size), 0, gl_format, img_size, nullptr);
        break;
    }
  }
  else {
    switch (dimensions) {
      default:
      case 1:
        glTexImage1D(gl_proxy, mip, internal_format, size[0], 0, gl_format, gl_type, nullptr);
        break;
      case 2:
        glTexImage2D(
            gl_proxy, mip, internal_format, UNPACK2(size), 0, gl_format, gl_type, nullptr);
        break;
      case 3:
        glTexImage3D(
            gl_proxy, mip, internal_format, UNPACK3(size), 0, gl_format, gl_type, nullptr);
        break;
    }
  }

  int width = 0;
  glGetTexLevelParameteriv(gl_proxy, 0, GL_TEXTURE_WIDTH, &width);
  return (width > 0);
}

/** \} */

void GLTexture::check_feedback_loop()
{
  /* Do not check if using compute shader. */
  GLShader *sh = dynamic_cast<GLShader *>(Context::get()->shader);
  if (sh && sh->is_compute()) {
    return;
  }
  GLFrameBuffer *fb = static_cast<GLFrameBuffer *>(GLContext::get()->active_fb);
  for (int i = 0; i < ARRAY_SIZE(fb_); i++) {
    if (fb_[i] == fb) {
      GPUAttachmentType type = fb_attachment_[i];
      GPUAttachment attachment = fb->attachments_[type];
      /* Check for when texture is used with texture barrier. */
      GPUAttachment attachment_read = fb->tmp_detached_[type];
      if (attachment.mip <= mip_max_ && attachment.mip >= mip_min_ &&
          attachment_read.tex == nullptr)
      {
        char msg[256];
        SNPRINTF(msg,
                 "Feedback loop: Trying to bind a texture (%s) with mip range %d-%d but mip %d is "
                 "attached to the active framebuffer (%s)",
                 name_,
                 mip_min_,
                 mip_max_,
                 attachment.mip,
                 fb->name_);
        debug::raise_gl_error(msg);
      }
      return;
    }
  }
}

/* -------------------------------------------------------------------- */
/** \name Pixel Buffer
 * \{ */

GLPixelBuffer::GLPixelBuffer(size_t size) : PixelBuffer(size)
{
  glGenBuffers(1, &gl_id_);
  BLI_assert(gl_id_);

  if (!gl_id_) {
    return;
  }

  /* Ensure size is non-zero for pixel buffer backing storage creation. */
  size = max_ii(size, 32);

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_id_);
  glBufferData(GL_PIXEL_UNPACK_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

GLPixelBuffer::~GLPixelBuffer()
{
  if (!gl_id_) {
    return;
  }
  glDeleteBuffers(1, &gl_id_);
}

void *GLPixelBuffer::map()
{
  if (!gl_id_) {
    BLI_assert(false);
    return nullptr;
  }

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_id_);
  void *ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
  BLI_assert(ptr);
  return ptr;
}

void GLPixelBuffer::unmap()
{
  glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

GPUPixelBufferNativeHandle GLPixelBuffer::get_native_handle()
{
  GPUPixelBufferNativeHandle native_handle;
  native_handle.handle = int64_t(gl_id_);
  native_handle.size = size_;
  return native_handle;
}

size_t GLPixelBuffer::get_size()
{
  return size_;
}

/** \} */
}  // namespace blender::gpu
