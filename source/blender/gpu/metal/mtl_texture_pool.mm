/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_string.h"

#include "BKE_global.hh"

#include "mtl_backend.hh"
#include "mtl_texture_pool.hh"

#include "CLG_log.h"

#include "fmt/format.h"

namespace blender::gpu {

static CLG_LogRef LOG = {"gpu.metal"};

/* Given if the formats can alias. */
static bool check_texture_format_compatability(TextureFormat src_format, TextureFormat dst_format)
{
  /* If the format matches we're good. */
  if (src_format == dst_format) {
    return true;
  }

  /* Metal's newTextureViewWithPixelFormat doesn't support aliasing on depth, stencil,
   * or compressed formats. */
  GPUTextureFormatFlag format_flag = to_format_flag(src_format);
  if (bool(format_flag & GPU_FORMAT_DEPTH_STENCIL)) {
    return false;
  }
  if (bool(format_flag & GPU_FORMAT_COMPRESSED)) {
    return false;
  }

  /* Metal docs say: All 8-, 16-, 32-, 64-, and 128-bit color formats are compatible
   * with other formats with the same bit length. */
  return to_bytesize(src_format) == to_bytesize(dst_format);
}

MTLTexturePool::~MTLTexturePool()
{
  for (const TextureHandle &handle : acquired_) {
    release_texture(wrap(handle.view));
  }
  for (AllocationHandle &handle : pool_) {
    GPU_texture_free(handle.texture);
  }
}

Texture *MTLTexturePool::acquire_texture(int2 extent,
                                         TextureFormat format,
                                         eGPUTextureUsage usage,
                                         const char *name)
{
  eGPUTextureUsage usage_flag = usage | GPU_TEXTURE_USAGE_FORMAT_VIEW;

  /* Search for the first compatible existing texture. */
  int64_t match_index = -1;
  bool exact_format_match = false;
  for (uint64_t i : pool_.index_range()) {
    const AllocationHandle &handle = pool_[i];
    /* Do dimensions match? */
    if (int2(handle.texture->w_, handle.texture->h_) != extent) {
      continue;
    }
    /* Are the formats compatible? */
    if (!check_texture_format_compatability(format, handle.texture->format_get())) {
      continue;
    }
    /* Metal enforces usage flags at texture creation time, so the pooled texture
     * must have been created with at least all the usage flags the caller needs. */
    if ((handle.texture->usage_get() & usage_flag) != usage_flag) {
      continue;
    }
    /* Is the format an exact match? */
    exact_format_match = (format == handle.texture->format_get());

    /* Do we need a writable texture? */
    bool writable_texture = usage &
                            (GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_SHADER_WRITE);

    /* Annoyingly Metal's newTextureViewWithPixelFormat strips MTLTextureUsageRenderTarget
     * and MTLTextureUsageShaderWrite from cross-format views (of course this is not
     * documented). If the caller needs either of these, only accept an exact format match
     * as in that case we don't actually create a view. */
    if (!exact_format_match && writable_texture) {
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
  }
  else {
    /* Debug label attached to allocated texture object. */
    std::string texture_name_str;
    if (G.debug & G_DEBUG_GPU) {
      texture_name_str = fmt::format("TexFromPool_{}", pool_.size());
    }
    texture_handle.texture = unwrap(GPU_texture_create_2d(
        texture_name_str.c_str(), extent.x, extent.y, 1, format, usage_flag, nullptr));
    exact_format_match = true;
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

  if (G.debug & G_DEBUG_GPU) {
    current_usage_data_.usage_count++;
    current_usage_data_.usage_count_max = std::max(current_usage_data_.usage_count,
                                                   current_usage_data_.usage_count_max);
  }

  /* Only create a view if the format actually differs.
   * This removes the limitation that the view will strip
   * any render-target or shader-write usage flags. */
  if (exact_format_match) {
    /* No view needed; set view to texture so lookups in release_texture work
     * (the returned pointer is used as the key in the acquired set). */
    texture_handle.view = texture_handle.texture;
  }
  else {
    /* Debug label attached to view texture object. */
    std::string view_name_str;
    if (G.debug & G_DEBUG_GPU) {
      view_name_str = name ? name : texture_handle.texture->name_;
    }

    /* Create texture view and add to handle. */
    texture_handle.view = unwrap(GPU_texture_create_view(
        view_name_str.c_str(), texture_handle.texture, format, 0, 1, 0, 1, false, false));
  }

  acquired_.add(texture_handle);
  return wrap(texture_handle.view);
}

void MTLTexturePool::release_texture(Texture *tex)
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

  /* Destroy view if one was created (view equals texture when no aliasing was needed). */
  if (texture_handle.view && texture_handle.view != texture_handle.texture) {
    GPU_texture_free(texture_handle.view);
  }
  acquired_.remove(texture_handle);
}

void MTLTexturePool::offset_users_count(Texture *tex, int offset)
{
  BLI_assert_msg(acquired_.contains({unwrap(tex)}),
                 "Unacquired texture passed to TexturePool::offset_users_count()");
  TextureHandle texture_handle = acquired_.lookup_key({unwrap(tex), {}, 1});
  texture_handle.users_count += offset;
  acquired_.add_overwrite(texture_handle);
}

void MTLTexturePool::reset(bool force_free)
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

void MTLTexturePool::log_usage_data() const
{
  int64_t total_texture_count = acquired_.size() + pool_.size();
  CLOG_TRACE(&LOG,
             "MTLTexturePool uses %ld textures (%ld consecutively)",
             static_cast<long>(total_texture_count),
             static_cast<long>(current_usage_data_.usage_count_max));
}
}  // namespace blender::gpu
