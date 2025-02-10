/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <memory>

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_index_range.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_task.hh"

#include "GPU_texture.hh"

#include "BKE_image.hh"
#include "BKE_texture.h"

#include "DNA_ID.h"
#include "DNA_texture_types.h"

#include "RE_texture.h"

#include "COM_cached_texture.hh"
#include "COM_context.hh"
#include "COM_result.hh"

namespace blender::compositor {

/* --------------------------------------------------------------------
 * Cached Texture Key.
 */

CachedTextureKey::CachedTextureKey(int2 size, float3 offset, float3 scale)
    : size(size), offset(offset), scale(scale)
{
}

uint64_t CachedTextureKey::hash() const
{
  return get_default_hash(size, offset, scale);
}

bool operator==(const CachedTextureKey &a, const CachedTextureKey &b)
{
  return a.size == b.size && a.offset == b.offset && a.scale == b.scale;
}

/* --------------------------------------------------------------------
 * Cached Texture.
 */

CachedTexture::CachedTexture(Context &context,
                             Tex *texture,
                             bool use_color_management,
                             int2 size,
                             float3 offset,
                             float3 scale)
    : color_result(context.create_result(ResultType::Color)),
      value_result(context.create_result(ResultType::Float))
{
  ImagePool *image_pool = BKE_image_pool_new();
  BKE_texture_fetch_images_for_pool(texture, image_pool);

  color_pixels_ = Array<float4>(size.x * size.y);
  value_pixels_ = Array<float>(size.x * size.y);
  threading::parallel_for(IndexRange(size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(size.x)) {
        /* Compute the coordinates in the [-1, 1] range and add 0.5 to evaluate the texture at the
         * center of pixels in case it was interpolated. */
        const float2 pixel_coordinates = ((float2(x, y) + 0.5f) / float2(size)) * 2.0f - 1.0f;
        /* Note that it is expected that the offset is scaled by the scale. */
        const float3 coordinates = (float3(pixel_coordinates, 0.0f) + offset) * scale;

        TexResult texture_result;
        const int result_type = multitex_ext_safe(
            texture, coordinates, &texture_result, image_pool, use_color_management, false);

        float4 color = float4(texture_result.trgba);
        color.w = texture_result.talpha ? color.w : texture_result.tin;
        if (!(result_type & TEX_RGB)) {
          copy_v3_fl(color, color.w);
        }

        color_pixels_[y * size.x + x] = color;
        value_pixels_[y * size.x + x] = color.w;
      }
    }
  });

  BKE_image_pool_free(image_pool);

  if (context.use_gpu()) {
    this->color_result.allocate_texture(Domain(size), false);
    this->value_result.allocate_texture(Domain(size), false);
    GPU_texture_update(this->color_result, GPU_DATA_FLOAT, color_pixels_.data());
    GPU_texture_update(this->value_result, GPU_DATA_FLOAT, value_pixels_.data());

    /* CPU-side data no longer needed, so free it. */
    color_pixels_ = Array<float4>();
    value_pixels_ = Array<float>();
  }
  else {
    this->color_result.wrap_external(&color_pixels_.data()[0].x, size);
    this->value_result.wrap_external(value_pixels_.data(), size);
  }
}

CachedTexture::~CachedTexture()
{
  this->color_result.release();
  this->value_result.release();
}

/* --------------------------------------------------------------------
 * Cached Texture Container.
 */

void CachedTextureContainer::reset()
{
  /* First, delete all cached textures that are no longer needed. */
  for (auto &cached_textures_for_id : map_.values()) {
    cached_textures_for_id.remove_if([](auto item) { return !item.value->needed; });
  }
  map_.remove_if([](auto item) { return item.value.is_empty(); });

  /* Second, reset the needed status of the remaining cached textures to false to ready them to
   * track their needed status for the next evaluation. */
  for (auto &cached_textures_for_id : map_.values()) {
    for (auto &value : cached_textures_for_id.values()) {
      value->needed = false;
    }
  }
}

CachedTexture &CachedTextureContainer::get(Context &context,
                                           Tex *texture,
                                           bool use_color_management,
                                           int2 size,
                                           float3 offset,
                                           float3 scale)
{
  const CachedTextureKey key(size, offset, scale);

  const std::string library_key = texture->id.lib ? texture->id.lib->id.name : "";
  const std::string id_key = std::string(texture->id.name) + library_key;
  auto &cached_textures_for_id = map_.lookup_or_add_default(id_key);

  /* Invalidate the cache for that texture ID if it was changed and reset the recalculate flag. */
  if (context.query_id_recalc_flag(reinterpret_cast<ID *>(texture)) & ID_RECALC_ALL) {
    cached_textures_for_id.clear();
  }

  auto &cached_texture = *cached_textures_for_id.lookup_or_add_cb(key, [&]() {
    return std::make_unique<CachedTexture>(
        context, texture, use_color_management, size, offset, scale);
  });

  cached_texture.needed = true;
  return cached_texture;
}

}  // namespace blender::compositor
