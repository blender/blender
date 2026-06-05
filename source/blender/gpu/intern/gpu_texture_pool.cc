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

#include "fmt/format.h"

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

Texture *TexturePoolImpl::acquire_texture_impl(int3 extent,
                                               int mip_len,
                                               GPUTextureType type,
                                               TextureFormat format,
                                               eGPUTextureUsage usage,
                                               const char * /* name */)
{
  /* Determine actual mipmap depth. */
  int mip_len_max = 1 + floorf(log2f(std::max({extent.x, extent.y, extent.z})));
  mip_len = min_ii(mip_len, mip_len_max);

  /* Search pool for compatible available texture first. */
  int64_t match_index = -1;
  for (uint64_t i : pool_.index_range()) {
    Texture *tex = pool_[i].texture;

    auto tex_args = std::tuple(tex->format_get(),
                               tex->type_get(),
                               tex->width_get(),
                               tex->height_get(),
                               tex->depth_get(),
                               tex->mip_count(),
                               tex->usage_get());
    if (std::tie(format, type, UNPACK3(extent), mip_len, usage) == tex_args) {
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

  /* Generate debug label name, if one isn't passed in `name`. TexturePoolImpl ignores the
   * name argument, as returned textures do not shadow/view/abstract the underlying texture. */
  std::string name_str;
  if (G.debug & G_DEBUG_GPU) {
    name_str = fmt::format("TexFromPool_{}", pool_.size());
  }

  /* Otherwise, allocate a new texture of the specified type. */
  TextureHandle handle = {GPUBackend::get()->texture_alloc(name_str.c_str())};
  handle.texture->usage_set(usage | GPU_TEXTURE_USAGE_FORMAT_VIEW);
  bool init_result = false;
  switch (type) {
    case GPU_TEXTURE_1D:
    case GPU_TEXTURE_1D_ARRAY:
      init_result = handle.texture->init_1D(extent.x, extent.y, mip_len, format);
      break;
    case GPU_TEXTURE_2D:
    case GPU_TEXTURE_2D_ARRAY:
      init_result = handle.texture->init_2D(extent.x, extent.y, extent.z, mip_len, format);
      break;
    case GPU_TEXTURE_3D:
      init_result = handle.texture->init_3D(extent.x, extent.y, extent.z, mip_len, format);
      break;
    case GPU_TEXTURE_CUBE:
    case GPU_TEXTURE_CUBE_ARRAY:
      init_result = handle.texture->init_cubemap(extent.x, extent.y, mip_len, format);
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
  UNUSED_VARS_NDEBUG(init_result);
  BLI_assert(init_result);

  acquired_.add(handle);
  return handle.texture;
}  // namespace blender::gpu

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
