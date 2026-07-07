/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * A `gpu::TextureFromPool` is a wrapper around backend specific texture objects whose usage is
 * transient and can be shared between parts of an engine or across several parts of blender.
 */

#pragma once

#include "BLI_math_vector_types.hh"
#include "BLI_set.hh"
#include "BLI_vector.hh"
#include "GPU_texture.hh"

namespace blender::gpu {

class TexturePool {
 public:
  virtual ~TexturePool() = default;

  /* Return the texture pool from the active GPUContext.
   * Only valid if a context is active. */
  static TexturePool &get();

  /* Acquire a 2D texture from the pool with the given characteristics. */
  virtual Texture *acquire_texture(int2 extent,
                                   TextureFormat format,
                                   eGPUTextureUsage usage = GPU_TEXTURE_USAGE_GENERAL,
                                   const char *name = nullptr) = 0;

  /* Release the texture back into the pool so it can be reused. */
  virtual void release_texture(Texture *tex) = 0;

  /* Validate acquired texture counters and release unused textures.
   * If `force_free` is true, free unused texture memory inside the pool. */
  virtual void reset(bool force_free = false) = 0;

  /* Modify the internal counter of an acquired texture.
   * Used by `TextureFromPool::retain()` in `DRW_gpu_wrapper.hh`. */
  virtual void offset_users_count(Texture *tex, int offset) = 0;
};

}  // namespace blender::gpu
