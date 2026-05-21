/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "GPU_texture_pool.hh"
#include "gpu_texture_private.hh"

namespace blender::gpu {

/**
 * TexturePool base class, which connects `acquire_texture_*` variants to
 * virtual `acquire_texture_impl` which backend implementations implement.
 */
class TexturePoolBase : public TexturePool {
 protected:
  virtual Texture *acquire_texture_impl(int3 extent,
                                        int mip_len,
                                        GPUTextureType type,
                                        TextureFormat format,
                                        eGPUTextureUsage usage = GPU_TEXTURE_USAGE_GENERAL,
                                        const char *name = nullptr) = 0;

 public:
  Texture *acquire_texture_1d(int extent,
                              int mip_len,
                              TextureFormat format,
                              eGPUTextureUsage usage,
                              const char *name = nullptr) override
  {
    return acquire_texture_impl({extent, 0, 0}, mip_len, GPU_TEXTURE_1D, format, usage, name);
  }
  Texture *acquire_texture_1d_array(int extent,
                                    int layer_len,
                                    int mip_len,
                                    TextureFormat format,
                                    eGPUTextureUsage usage,
                                    const char *name = nullptr) override
  {
    return acquire_texture_impl(
        {extent, layer_len, 0}, mip_len, GPU_TEXTURE_1D_ARRAY, format, usage, name);
  }
  Texture *acquire_texture_2d(int2 extent,
                              int mip_len,
                              TextureFormat format,
                              eGPUTextureUsage usage,
                              const char *name = nullptr) override
  {
    return acquire_texture_impl(
        {extent.x, extent.y, 0}, mip_len, GPU_TEXTURE_2D, format, usage, name);
  }
  Texture *acquire_texture_2d_array(int2 extent,
                                    int layer_len,
                                    int mip_len,
                                    TextureFormat format,
                                    eGPUTextureUsage usage,
                                    const char *name = nullptr) override
  {
    return acquire_texture_impl(
        {extent.x, extent.y, layer_len}, mip_len, GPU_TEXTURE_2D_ARRAY, format, usage, name);
  }
  Texture *acquire_texture_3d(int3 extent,
                              int mip_len,
                              TextureFormat format,
                              eGPUTextureUsage usage,
                              const char *name = nullptr) override
  {
    return acquire_texture_impl(extent, mip_len, GPU_TEXTURE_3D, format, usage, name);
  }
  Texture *acquire_texture_cube(int extent,
                                int mip_len,
                                TextureFormat format,
                                eGPUTextureUsage usage,
                                const char *name = nullptr) override
  {
    return acquire_texture_impl(
        {extent, extent, 0}, mip_len, GPU_TEXTURE_CUBE, format, usage, name);
  }
  Texture *acquire_texture_cube_array(int extent,
                                      int layer_len,
                                      int mip_len,
                                      TextureFormat format,
                                      eGPUTextureUsage usage,
                                      const char *name = nullptr) override
  {
    return acquire_texture_impl(
        {extent, extent, layer_len}, mip_len, GPU_TEXTURE_CUBE_ARRAY, format, usage, name);
  }
};

/**
 * Old texture pool implementation, to be used while a backend-specific
 * implementation is not yet available.
 */
class TexturePoolImpl : public TexturePoolBase {
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

 protected:
  Texture *acquire_texture_impl(int3 extent,
                                int mip_len,
                                GPUTextureType type,
                                TextureFormat format,
                                eGPUTextureUsage usage = GPU_TEXTURE_USAGE_GENERAL,
                                const char *name = nullptr) override;

 public:
  ~TexturePoolImpl() override;

  void release_texture(Texture *tex) override;

  void reset(bool force_free = false) override;

  void offset_users_count(Texture *tex, int offset) override;
};

}  // namespace blender::gpu
