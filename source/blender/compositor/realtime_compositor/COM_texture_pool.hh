/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>

#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_vector.hh"

#include "GPU_texture.h"

namespace blender::realtime_compositor {

/* ------------------------------------------------------------------------------------------------
 * Texture Pool Key
 *
 * A key used to identify a texture specification in a texture pool. Defines a hash and an equality
 * operator for use in a hash map. */
class TexturePoolKey {
 public:
  int2 size;
  eGPUTextureFormat format;

  /* Construct a key from the given texture size and format. */
  TexturePoolKey(int2 size, eGPUTextureFormat format);

  /* Construct a key from the size and format of the given texture. */
  TexturePoolKey(const GPUTexture *texture);

  uint64_t hash() const;
};

bool operator==(const TexturePoolKey &a, const TexturePoolKey &b);

/* ------------------------------------------------------------------------------------------------
 * Texture Pool
 *
 * A texture pool allows the allocation and reuse of textures throughout the execution of the
 * compositor to avoid memory fragmentation and texture allocation overheads. The texture pool
 * delegates the actual texture allocation to an allocate_texture method that should be implemented
 * by the caller of the compositor evaluator, allowing a more agnostic and flexible execution that
 * can be controlled by the caller. If the compositor is expected to execute frequently, like on
 * every redraw, then the allocation method should use a persistent texture pool to allow
 * cross-evaluation texture pooling, for instance, by using the DRWTexturePool. But if the
 * evaluator is expected to execute infrequently, the allocated textures can just be freed when the
 * evaluator is done, that is, when the pool is destructed. */
class TexturePool {
 private:
  /* The set of textures in the pool that are available to acquire for each distinct texture
   * specification. */
  Map<TexturePoolKey, Vector<GPUTexture *>> textures_;

 public:
  /* Check if there is an available texture with the given specification in the pool, if such
   * texture exists, return it, otherwise, return a newly allocated texture. Expect the texture to
   * be uncleared and possibly contains garbage data. */
  GPUTexture *acquire(int2 size, eGPUTextureFormat format);

  /* Shorthand for acquire with GPU_RGBA16F format. */
  GPUTexture *acquire_color(int2 size);

  /* Shorthand for acquire with GPU_RGBA16F format. Identical to acquire_color because vectors are
   * 4D, and are thus stored in RGBA textures. */
  GPUTexture *acquire_vector(int2 size);

  /* Shorthand for acquire with GPU_R16F format. */
  GPUTexture *acquire_float(int2 size);

  /* Put the texture back into the pool, potentially to be acquired later by another user. Expects
   * the texture to be one that was acquired using the same texture pool. */
  void release(GPUTexture *texture);

  /* Reset the texture pool by clearing all available textures without freeing the textures. If the
   * textures will no longer be needed, they should be freed in the destructor. This should be
   * called after the compositor is done evaluating. */
  void reset();

 private:
  /* Returns a newly allocated texture with the given specification. This method should be
   * implemented by the caller of the compositor evaluator. See the class description for more
   * information. */
  virtual GPUTexture *allocate_texture(int2 size, eGPUTextureFormat format) = 0;
};

}  // namespace blender::realtime_compositor
