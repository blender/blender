/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <memory>

#include "BLI_math_vector_types.hh"

#include "DNA_ID.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "COM_context.hh"
#include "COM_morphological_distance_feather_weights.hh"
#include "COM_smaa_precomputed_textures.hh"
#include "COM_symmetric_blur_weights.hh"
#include "COM_symmetric_separable_blur_weights.hh"

#include "COM_static_cache_manager.hh"

namespace blender::realtime_compositor {

/* --------------------------------------------------------------------
 * Static Cache Manager.
 */

void StaticCacheManager::reset()
{
  /* First, delete all resources that are no longer needed. */
  symmetric_blur_weights_.remove_if([](auto item) { return !item.value->needed; });
  symmetric_separable_blur_weights_.remove_if([](auto item) { return !item.value->needed; });
  morphological_distance_feather_weights_.remove_if([](auto item) { return !item.value->needed; });

  for (auto &cached_textures_for_id : cached_textures_.values()) {
    cached_textures_for_id.remove_if([](auto item) { return !item.value->needed; });
  }
  cached_textures_.remove_if([](auto item) { return item.value.is_empty(); });

  if (smaa_precomputed_textures_ && !smaa_precomputed_textures_->needed) {
    smaa_precomputed_textures_.reset();
  }

  /* Second, reset the needed status of the remaining resources to false to ready them to track
   * their needed status for the next evaluation. */
  for (auto &value : symmetric_blur_weights_.values()) {
    value->needed = false;
  }
  for (auto &value : symmetric_separable_blur_weights_.values()) {
    value->needed = false;
  }
  for (auto &value : morphological_distance_feather_weights_.values()) {
    value->needed = false;
  }
  for (auto &cached_textures_for_id : cached_textures_.values()) {
    for (auto &value : cached_textures_for_id.values()) {
      value->needed = false;
    }
  }
  if (smaa_precomputed_textures_) {
    smaa_precomputed_textures_->needed = false;
  }
}

SymmetricBlurWeights &StaticCacheManager::get_symmetric_blur_weights(int type, float2 radius)
{
  const SymmetricBlurWeightsKey key(type, radius);

  auto &weights = *symmetric_blur_weights_.lookup_or_add_cb(
      key, [&]() { return std::make_unique<SymmetricBlurWeights>(type, radius); });

  weights.needed = true;
  return weights;
}

SymmetricSeparableBlurWeights &StaticCacheManager::get_symmetric_separable_blur_weights(
    int type, float radius)
{
  const SymmetricSeparableBlurWeightsKey key(type, radius);

  auto &weights = *symmetric_separable_blur_weights_.lookup_or_add_cb(
      key, [&]() { return std::make_unique<SymmetricSeparableBlurWeights>(type, radius); });

  weights.needed = true;
  return weights;
}

MorphologicalDistanceFeatherWeights &StaticCacheManager::
    get_morphological_distance_feather_weights(int type, int radius)
{
  const MorphologicalDistanceFeatherWeightsKey key(type, radius);

  auto &weights = *morphological_distance_feather_weights_.lookup_or_add_cb(
      key, [&]() { return std::make_unique<MorphologicalDistanceFeatherWeights>(type, radius); });

  weights.needed = true;
  return weights;
}

CachedTexture &StaticCacheManager::get_cached_texture(
    Context &context, Tex *texture, const Scene *scene, int2 size, float2 offset, float2 scale)
{
  const CachedTextureKey key(size, offset, scale);

  auto &cached_textures_for_id = cached_textures_.lookup_or_add_default(texture->id.name);

  if (context.query_id_recalc_flag(reinterpret_cast<ID *>(texture)) & ID_RECALC_ALL) {
    cached_textures_for_id.clear();
  }

  auto &cached_texture = *cached_textures_for_id.lookup_or_add_cb(
      key, [&]() { return std::make_unique<CachedTexture>(texture, scene, size, offset, scale); });

  cached_texture.needed = true;
  return cached_texture;
}

SMAAPrecomputedTextures &StaticCacheManager::get_smaa_precomputed_textures()
{
  if (!smaa_precomputed_textures_) {
    smaa_precomputed_textures_ = std::make_unique<SMAAPrecomputedTextures>();
  }

  smaa_precomputed_textures_->needed = true;
  return *smaa_precomputed_textures_;
}

}  // namespace blender::realtime_compositor
