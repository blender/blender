/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gl_texture.hh"
#include "gpu_texture_pool_private.hh"

namespace blender::gpu {

class GLTexturePool : public TexturePool {
  /* Defer deallocation enough cycles to avoid interleaved calls to different viewport render
   * functions (selection / display) causing constant allocation / deallocation (See #113024). */
  static constexpr int max_unused_cycles_ = 8;

  struct AllocationHandle {
    GLTexture *texture = nullptr;

    /* Counter to track the number of unused cycles before deallocation in `pool_`. */
    int unused_cycles_count = 0;
  };

  struct TextureHandle {
    GLTexture *view = nullptr;
    GLTexture *texture = nullptr;

    /* Counter to track texture acquire/retain mismatches in `acquire_`.  */
    int users_count = 1;

    /* We use the pointer as hash/comparator, as a texture cannot be acquired twice. */
    uint64_t hash() const
    {
      return get_default_hash(view);
    }

    bool operator==(const TextureHandle &o) const
    {
      return view == o.view;
    }
  };

  Vector<AllocationHandle> pool_;
  Set<TextureHandle> acquired_;

  /* Debug storage to log memory usage. Log is only output
   * if values have changed since the last `::reset()`. */
  struct LogUsageData {
    int64_t usage_count = 0;
    int64_t usage_count_max = 0;

    bool operator==(const LogUsageData &o) const
    {
      return std::tie(usage_count, usage_count_max) == std::tie(o.usage_count, o.usage_count_max);
    }
  };
  LogUsageData previous_usage_data_ = {};
  LogUsageData current_usage_data_ = {};

  /* Output usage data to debug log. Called on `--debug-gpu` */
  void log_usage_data() const;

 public:
  ~GLTexturePool();

  Texture *acquire_texture(int2 extent,
                           TextureFormat format,
                           eGPUTextureUsage usage = GPU_TEXTURE_USAGE_GENERAL,
                           const char *name = nullptr) override;

  void release_texture(Texture *tex) override;

  void reset(bool force_free = false) override;

  void offset_users_count(Texture *tex, int offset) override;
};

}  // namespace blender::gpu
