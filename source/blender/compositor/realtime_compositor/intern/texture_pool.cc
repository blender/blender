/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>

#include "BLI_hash.hh"
#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_vector.hh"

#include "GPU_texture.h"

#include "COM_texture_pool.hh"

namespace blender::realtime_compositor {

/* -------------------------------------------------------------------- */
/** \name Texture Pool Key
 * \{ */

TexturePoolKey::TexturePoolKey(int2 size, eGPUTextureFormat format) : size(size), format(format) {}

TexturePoolKey::TexturePoolKey(const GPUTexture *texture)
{
  size = int2(GPU_texture_width(texture), GPU_texture_height(texture));
  format = GPU_texture_format(texture);
}

uint64_t TexturePoolKey::hash() const
{
  return get_default_hash_3(size.x, size.y, format);
}

bool operator==(const TexturePoolKey &a, const TexturePoolKey &b)
{
  return a.size == b.size && a.format == b.format;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture Pool
 * \{ */

GPUTexture *TexturePool::acquire(int2 size, eGPUTextureFormat format)
{
  /* Check if there is an available texture with the required specification, and if one exists,
   * return it. */
  const TexturePoolKey key = TexturePoolKey(size, format);
  Vector<GPUTexture *> &available_textures = textures_.lookup_or_add_default(key);
  if (!available_textures.is_empty()) {
    return available_textures.pop_last();
  }

  /* Otherwise, allocate a new texture. */
  return allocate_texture(size, format);
}

void TexturePool::release(GPUTexture *texture)
{
  textures_.lookup(TexturePoolKey(texture)).append(texture);
}

void TexturePool::reset()
{
  textures_.clear();
}

/** \} */

}  // namespace blender::realtime_compositor
