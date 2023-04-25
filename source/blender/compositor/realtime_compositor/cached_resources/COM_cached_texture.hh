/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>

#include "BLI_math_vector_types.hh"

#include "GPU_texture.h"

#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "COM_cached_resource.hh"

namespace blender::realtime_compositor {

/* ------------------------------------------------------------------------------------------------
 * Cached Texture Key.
 */
class CachedTextureKey {
 public:
  int2 size;
  float2 offset;
  float2 scale;

  CachedTextureKey(int2 size, float2 offset, float2 scale);

  uint64_t hash() const;
};

bool operator==(const CachedTextureKey &a, const CachedTextureKey &b);

/* -------------------------------------------------------------------------------------------------
 * Cached Texture.
 *
 * A cached resource that computes and caches a GPU texture containing the the result of evaluating
 * the given texture ID on a space that spans the given size, modified by the given offset and
 * scale. */
class CachedTexture : public CachedResource {
 private:
  GPUTexture *color_texture_ = nullptr;
  GPUTexture *value_texture_ = nullptr;

 public:
  CachedTexture(Tex *texture, const Scene *scene, int2 size, float2 offset, float2 scale);

  ~CachedTexture();

  GPUTexture *color_texture();

  GPUTexture *value_texture();
};

}  // namespace blender::realtime_compositor
