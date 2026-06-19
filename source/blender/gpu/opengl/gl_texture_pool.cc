/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_string.hh"

#include "gl_backend.hh"
#include "gl_texture_pool.hh"

#include "CLG_log.h"

#include "fmt/format.h"

namespace blender::gpu {

static CLG_LogRef LOG = {"gpu.opengl"};

/* Given a TextureFormat, return an underlying format on which to alias. If the
 * format does not support aliasing to another format, simply return the input. */
static TextureFormat get_compatible_texture_format(TextureFormat format)
{
  /* glTextureView doesn't support aliasing on depth, stencil, or most compressed formats. */
  GPUTextureFormatFlag format_flag = to_format_flag(format);
  if (bool(format_flag & GPU_FORMAT_DEPTH_STENCIL)) {
    return format;
  }
  if (bool(format_flag & GPU_FORMAT_COMPRESSED)) {
    return format;
  }

  /* Given expected byte size, we use a default format available as write/target format. */
  switch (to_bytesize(format)) {
    case 16:
      return TextureFormat::SFLOAT_32_32_32_32;
    case 8:
      return TextureFormat::SFLOAT_32_32;
    case 4:
      return TextureFormat::SFLOAT_32;
    case 2:
      return TextureFormat::SFLOAT_16;
    case 1:
      return TextureFormat::UINT_8;
    default:
      return TextureFormat::Invalid;
  }
}

GLTexturePool::~GLTexturePool()
{
  for (const TextureHandle &handle : acquired_) {
    release_texture(wrap(handle.view));
  }
  for (AllocationHandle &handle : pool_) {
    GPU_texture_free(handle.texture);
  }
}

Texture *GLTexturePool::acquire_texture_impl(int3 extent,
                                             int mip_len,
                                             GPUTextureType type,
                                             TextureFormat format,
                                             eGPUTextureUsage usage,
                                             const char *name)
{
  /* Determine format of compatible underlying texture. If there is no
   * compatible format to alias upon, we simply require an exact match
   * for the underlying texture. */
  TextureFormat compatible_format = get_compatible_texture_format(format);
  BLI_assert(compatible_format != TextureFormat::Invalid);

  /* Determine actual mipmap depth. */
  int mip_len_max = 1 + floorf(log2f(std::max({extent.x, extent.y, extent.z})));
  mip_len = min_ii(mip_len, mip_len_max);

  /* Search for the first compatible existing texture. */
  int64_t match_index = -1;
  for (uint64_t i : pool_.index_range()) {
    const AllocationHandle &handle = pool_[i];
    if (handle.texture->format_ != compatible_format) {
      continue;
    }
    if (int3(handle.texture->w_, handle.texture->h_, handle.texture->d_) != extent) {
      /* TODO(not_mark): sub-view on `texture->d_`. */
      continue;
    }
    if (handle.texture->mip_count() != mip_len) {
      /* TODO(not_mark): sub-view on mip levels. */
      continue;
    }
    match_index = i;
    break;
  }

  /* Return value. */
  TextureHandle texture_handle;

  /* Acquire the compatible texture, or create a new one as a last resort. */
  if (match_index != -1) {
    texture_handle.texture = pool_[match_index].texture;
    pool_.remove_and_reorder(match_index);
    /* Override the usage. It doesn't really apply to OpenGL in practice, but `GPU_texture_usage`
     * callers may still rely on it. */
    texture_handle.texture->usage_set(usage | GPU_TEXTURE_USAGE_FORMAT_VIEW);
  }
  else {
    /* Debug label attached to allocated texture object. */
    std::string texture_name_str;
    if (G.debug & G_DEBUG_GPU) {
      texture_name_str = fmt::format("TexFromPool_{}", pool_.size());
    }

    Texture *texture = GPUBackend::get()->texture_alloc(texture_name_str.c_str());
    texture->usage_set(usage | GPU_TEXTURE_USAGE_FORMAT_VIEW);
    bool texture_result = false;
    UNUSED_VARS_NDEBUG(texture_result);
    switch (type) {
      case GPU_TEXTURE_1D:
      case GPU_TEXTURE_1D_ARRAY:
        texture_result = texture->init_1D(extent.x, extent.y, mip_len, compatible_format);
        break;
      case GPU_TEXTURE_2D:
      case GPU_TEXTURE_2D_ARRAY:
        texture_result = texture->init_2D(
            extent.x, extent.y, extent.z, mip_len, compatible_format);
        break;
      case GPU_TEXTURE_3D:
        texture_result = texture->init_3D(
            extent.x, extent.y, extent.z, mip_len, compatible_format);
        break;
      case GPU_TEXTURE_CUBE:
      case GPU_TEXTURE_CUBE_ARRAY:
        texture_result = texture->init_cubemap(extent.x, extent.y, mip_len, compatible_format);
        break;
      default:
        BLI_assert_unreachable();
        break;
    }
    BLI_assert(texture_result);

    texture_handle.texture = unwrap(texture);
  }

  /* On acquire, issue barriers; backing texture or view may still be in flight somewhere. */
  GPUBarrier barrier = {};
  if (usage & GPU_TEXTURE_USAGE_SHADER_READ) {
    barrier |= (GPU_BARRIER_SHADER_IMAGE_ACCESS | GPU_BARRIER_TEXTURE_FETCH);
  }
  if (usage & GPU_TEXTURE_USAGE_SHADER_WRITE) {
    barrier |= GPU_BARRIER_SHADER_IMAGE_ACCESS;
  }
  if (usage & GPU_TEXTURE_USAGE_ATTACHMENT) {
    barrier |= GPU_BARRIER_FRAMEBUFFER;
  }
  GPU_memory_barrier(barrier);

  /* Debug label attached to view texture object. */
  std::string view_name_str;
  if (G.debug & G_DEBUG_GPU) {
    view_name_str = name ? name : texture_handle.texture->name_;
  }

  /* Assemble texture view and add to handle. Note, glTextureView with identical formats is
   * allowed, even if the formats are not listed for aliasing in the Internal Formats table. */
  Texture *view = GPUBackend::get()->texture_alloc(view_name_str.c_str());
  bool view_result = false;
  UNUSED_VARS_NDEBUG(view_result);
  switch (type) {
    case GPU_TEXTURE_1D:
    case GPU_TEXTURE_2D:
    case GPU_TEXTURE_3D:
    case GPU_TEXTURE_CUBE:
      view_result = view->init_view(
          texture_handle.texture, format, type, 0, mip_len, 0, 1, false, false);
      break;
    case GPU_TEXTURE_1D_ARRAY:
      view_result = view->init_view(
          texture_handle.texture, format, type, 0, mip_len, 0, extent.y, false, false);
      break;
    case GPU_TEXTURE_2D_ARRAY:
    case GPU_TEXTURE_CUBE_ARRAY:
      view_result = view->init_view(
          texture_handle.texture, format, type, 0, mip_len, 0, extent.z, false, false);
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
  BLI_assert(view_result);

  /* On integer textures, disable filtering by default, as this is not guaranteed to be
   * consistently supported across backends. */
  if (GPU_texture_has_integer_format(view)) {
    view->sampler_state.set_filtering_flag_from_test(GPU_SAMPLER_FILTERING_LINEAR, false);
    view->sampler_state.set_filtering_flag_from_test(GPU_SAMPLER_FILTERING_MIPMAP, false);
    view->sampler_state.set_filtering_flag_from_test(GPU_SAMPLER_FILTERING_ANISOTROPIC_MASK,
                                                     false);
  }

  texture_handle.view = unwrap(view);

  if (G.debug & G_DEBUG_GPU) {
    current_usage_data_.usage_count++;
    current_usage_data_.usage_count_max = std::max(current_usage_data_.usage_count,
                                                   current_usage_data_.usage_count_max);
  }

  acquired_.add(texture_handle);
  return wrap(texture_handle.view);
}

void GLTexturePool::release_texture(Texture *tex)
{
  BLI_assert_msg(acquired_.contains({unwrap(tex)}),
                 "Unacquired texture passed to TexturePool::release_texture()");
  TextureHandle texture_handle = acquired_.lookup_key({unwrap(tex), {}, 1});

  if (G.debug & G_DEBUG_GPU) {
    current_usage_data_.usage_count--;
  }

  /* Move allocation back to `pool_`. */
  AllocationHandle allocation_handle;
  allocation_handle.texture = texture_handle.texture;
  pool_.append(allocation_handle);

  /* Destroy view and handle, if a view was created. */
  GPU_texture_free(texture_handle.view);
  acquired_.remove(texture_handle);
}

void GLTexturePool::offset_users_count(Texture *tex, int offset)
{
  BLI_assert_msg(acquired_.contains({unwrap(tex)}),
                 "Unacquired texture passed to TexturePool::offset_users_count()");
  TextureHandle texture_handle = acquired_.lookup_key({unwrap(tex), {}, 1});
  texture_handle.users_count += offset;
  acquired_.add_overwrite(texture_handle);
}

void GLTexturePool::reset(bool force_free)
{
#ifndef NDEBUG
  /* Iterate acquired textures, and ensure the internal counter equals 0; otherwise
   * this indicates a missing `::retain()` or `::release()`. */
  for (const TextureHandle &tex : acquired_) {
    BLI_assert_msg(tex.users_count == 0,
                   "Missing texture release/retain. Likely TextureFromPool::release(), "
                   "TextureFromPool::retain() or TexturePool::release_texture().");
  }
#endif

  /* Reverse iterate unused allocations, to make sure we only reorder known good handles. */
  for (int i = pool_.size() - 1; i >= 0; i--) {
    AllocationHandle &handle = pool_[i];
    if (handle.unused_cycles_count >= max_unused_cycles_ || force_free) {
      GPU_texture_free(handle.texture);
      pool_.remove_and_reorder(i);
    }
    else {
      handle.unused_cycles_count++;
    }
  }

  if (G.debug & G_DEBUG_GPU) {
    /* Log debug usage if it differs from the last reset. */
    if (!(previous_usage_data_ == current_usage_data_)) {
      log_usage_data();
    }

    /* Reset usage data to track it for the next reset. */
    previous_usage_data_ = current_usage_data_;
    current_usage_data_ = {};
    current_usage_data_.usage_count = acquired_.size();
  }
}

void GLTexturePool::log_usage_data() const
{
  int64_t total_texture_count = acquired_.size() + pool_.size();
  CLOG_TRACE(&LOG,
             "GLTexturePool uses %ld textures (%ld consecutively)",
             static_cast<long>(total_texture_count),
             static_cast<long>(current_usage_data_.usage_count_max));
}
}  // namespace blender::gpu
