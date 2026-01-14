/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "GPU_texture_pool.hh"

namespace blender::gpu {

/**
 * Old texture pool implementation, to be used while a backend-specific
 * implementation is not yet available.
 */
class TexturePoolImpl : public TexturePool {
  /* Defer deallocation enough cycles to avoid interleaved calls to different viewport render
   * functions (selection / display) causing constant allocation / deallocation (See #113024). */
  static constexpr int max_unused_cycles_ = 8;

  /* Internal packet for texture, which supports set insertion. */
  struct TextureHandle {
    Texture *texture;
    /* Counter to track texture acquire/retain mismatches in `acquire_`.  */
    int users_count = 1;
    /* Counter to track the number of unused cycles before deallocation in `pool_`. */
    int unused_cycles_count = 0;

    /* We use the pointer as hash/comparator, as a texture cannot be acquired twice. */
    uint64_t hash() const
    {
      return get_default_hash(texture);
    }

    bool operator==(const TextureHandle &o) const
    {
      return texture == o.texture;
    }
  };

  /* Pool of textures ready to be reused. */
  Vector<TextureHandle> pool_;
  /* Set of textures currently in use. */
  Set<TextureHandle> acquired_;

 public:
  ~TexturePoolImpl();

  Texture *acquire_texture(int2 extent,
                           TextureFormat format,
                           eGPUTextureUsage usage = GPU_TEXTURE_USAGE_GENERAL) override;

  void release_texture(Texture *tex) override;

  void reset(bool force_free = false) override;

  void offset_users_count(Texture *tex, int offset) override;
};

}  // namespace blender::gpu
