/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_index_range.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_task.hh"

#include "GPU_texture.h"

#include "BKE_texture.h"

#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "RE_texture.h"

#include "COM_cached_texture.hh"

namespace blender::realtime_compositor {

/* --------------------------------------------------------------------
 * Cached Texture Key.
 */

CachedTextureKey::CachedTextureKey(int2 size, float2 offset, float2 scale)
    : size(size), offset(offset), scale(scale)
{
}

uint64_t CachedTextureKey::hash() const
{
  return get_default_hash_3(size, offset, scale);
}

bool operator==(const CachedTextureKey &a, const CachedTextureKey &b)
{
  return a.size == b.size && a.offset == b.offset && a.scale == b.scale;
}

/* --------------------------------------------------------------------
 * Cached Texture.
 */

CachedTexture::CachedTexture(
    Tex *texture, const Scene *scene, int2 size, float2 offset, float2 scale)
{
  Array<float4> color_pixels(size.x * size.y);
  Array<float> value_pixels(size.x * size.y);
  threading::parallel_for(IndexRange(size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(size.x)) {
        /* Compute the coordinates in the [0, 1] range and add 0.5 to evaluate the texture at the
         * center of pixels in case it was interpolated. */
        float2 coordinates = ((float2(x, y) + 0.5f) / float2(size)) * 2.0f - 1.0f;
        /* Note that it is expected that the offset is scaled by the scale. */
        coordinates = (coordinates + offset) * scale;
        TexResult texture_result;
        BKE_texture_get_value(scene, texture, coordinates, &texture_result, true);
        color_pixels[y * size.x + x] = float4(texture_result.trgba);
        value_pixels[y * size.x + x] = texture_result.talpha ? texture_result.trgba[3] :
                                                               texture_result.tin;
      }
    }
  });

  color_texture_ = GPU_texture_create_2d("Cached Color Texture",
                                         size.x,
                                         size.y,
                                         1,
                                         GPU_RGBA16F,
                                         GPU_TEXTURE_USAGE_SHADER_READ,
                                         *color_pixels.data());

  value_texture_ = GPU_texture_create_2d("Cached Value Texture",
                                         size.x,
                                         size.y,
                                         1,
                                         GPU_R16F,
                                         GPU_TEXTURE_USAGE_SHADER_READ,
                                         value_pixels.data());
}

CachedTexture::~CachedTexture()
{
  GPU_texture_free(color_texture_);
  GPU_texture_free(value_texture_);
}

GPUTexture *CachedTexture::color_texture()
{
  return color_texture_;
}

GPUTexture *CachedTexture::value_texture()
{
  return value_texture_;
}

}  // namespace blender::realtime_compositor
