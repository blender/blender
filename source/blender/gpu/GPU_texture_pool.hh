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

#include "BLI_vector.hh"

#include "GPU_texture.hh"

namespace blender::gpu {

class TexturePool {
 private:
  /* Defer deallocation enough cycles to avoid interleaved calls to different viewport render
   * functions (selection / display) causing constant allocation / deallocation (See #113024). */
  static constexpr int max_unused_cycles_ = 8;

  struct TextureHandle {
    blender::gpu::Texture *texture;
    /* Counts the number of `reset()` call since the last use.
     * The texture memory is deallocated after a certain number of cycles. */
    int unused_cycles;
  };

  /* Pool of texture ready to be reused. */
  blender::Vector<TextureHandle> pool_;
  /* List of textures that are currently being used. Tracked to check memory leak. */
  blender::Vector<blender::gpu::Texture *> acquired_;

 public:
  ~TexturePool();

  /* Return the texture pool from the active GPUContext.
   * Only valid if a context is active. */
  static TexturePool &get();

  /* Acquire a texture from the pool with the given characteristics. */
  blender::gpu::Texture *acquire_texture(int width,
                                         int height,
                                         blender::gpu::TextureFormat format,
                                         eGPUTextureUsage usage);
  /* Release the texture so that its memory can be reused at some other point. */
  void release_texture(blender::gpu::Texture *tmp_tex);

  /* Transfer ownership of a texture from the pool to the caller. */
  void take_texture_ownership(blender::gpu::Texture *tex);
  /* Transfer back ownership to the pool. The texture will become part of the pool. */
  void give_texture_ownership(blender::gpu::Texture *tex);

  /* Ensure no texture is still acquired and release unused textures.
   * If `force_free` is true, free all the texture memory inside the pool.
   * Otherwise, only unused textures will be freed. */
  void reset(bool force_free = false);
};

}  // namespace blender::gpu
