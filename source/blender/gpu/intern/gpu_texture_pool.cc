/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BKE_global.hh"
#include "BLI_string.h"

#include "GPU_texture_pool.hh"

#include "gpu_backend.hh"
#include "gpu_context_private.hh"
#include "gpu_texture_pool_private.hh"

namespace blender::gpu {

TexturePool &TexturePool::get()
{
  BLI_assert(GPU_context_active_get() != nullptr);
  return *unwrap(GPU_context_active_get())->texture_pool;
}

TexturePoolImpl::~TexturePoolImpl()
{
  for (TextureHandle tex : acquired_) {
    GPU_texture_free(tex.texture);
  }
  for (TextureHandle tex : pool_) {
    GPU_texture_free(tex.texture);
  }
}

Texture *TexturePoolImpl::acquire_texture(int2 extent,
                                          TextureFormat format,
                                          eGPUTextureUsage usage)
{
  /* Search pool for compatible available texture first. */
  int64_t match_index = -1;
  for (uint64_t i : pool_.index_range()) {
    Texture *tex = pool_[i].texture;
    if ((GPU_texture_format(tex) == format) && (GPU_texture_width(tex) == extent.x) &&
        (GPU_texture_height(tex) == extent.y) && (GPU_texture_usage(tex) == usage))
    {
      match_index = i;
      break;
    }
  }

  /* If a compatible pool texture was found, acquire and return it. */
  if (match_index != -1) {
    TextureHandle handle = {pool_[match_index].texture};
    acquired_.add(handle);
    pool_.remove_and_reorder(match_index);
    return handle.texture;
  }

  /* Otherwise, allocate a new texture as a last resort. */
  char name[16] = "TexFromPool";
  if (G.debug & G_DEBUG_GPU) {
    int texture_id = pool_.size();
    SNPRINTF(name, "TexFromPool_%d", texture_id);
  }
  TextureHandle handle = {GPU_texture_create_2d(name, UNPACK2(extent), 1, format, usage, nullptr)};
  acquired_.add(handle);
  return handle.texture;
}

void TexturePoolImpl::release_texture(Texture *tex)
{
  BLI_assert_msg(acquired_.contains({tex}),
                 "Unacquired texture passed to TexturePool::release_texture()");
  acquired_.remove({tex});
  pool_.append({tex});
}

void TexturePoolImpl::offset_users_count(Texture *tex, int offset)
{
  BLI_assert_msg(acquired_.contains({tex}),
                 "Unacquired texture passed to TexturePool::offset_users_count()");
  int users_count = acquired_.lookup_key({tex}).users_count;
  acquired_.add_overwrite({tex, users_count + offset, 0});
}

void TexturePoolImpl::reset(bool force_free)
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

  /* Reverse iterate pool textures, to make sure we only reorder known good handles. */
  for (int i = pool_.size() - 1; i >= 0; i--) {
    TextureHandle &tex = pool_[i];
    if (tex.unused_cycles_count >= max_unused_cycles_ || force_free) {
      GPU_texture_free(tex.texture);
      pool_.remove_and_reorder(i);
    }
    else {
      tex.unused_cycles_count++;
    }
  }
}
}  // namespace blender::gpu
