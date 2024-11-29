/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cmath>
#include <cstdint>
#include <memory>

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_index_range.hh"

#include "RE_pipeline.h"

#include "DNA_scene_types.h"

#include "GPU_texture.hh"

#include "COM_context.hh"
#include "COM_morphological_distance_feather_weights.hh"
#include "COM_result.hh"

namespace blender::realtime_compositor {

/* --------------------------------------------------------------------
 * Morphological Distance Feather Weights Key.
 */

MorphologicalDistanceFeatherWeightsKey::MorphologicalDistanceFeatherWeightsKey(int type,
                                                                               float radius)
    : type(type), radius(radius)
{
}

uint64_t MorphologicalDistanceFeatherWeightsKey::hash() const
{
  return get_default_hash(type, radius);
}

bool operator==(const MorphologicalDistanceFeatherWeightsKey &a,
                const MorphologicalDistanceFeatherWeightsKey &b)
{
  return a.type == b.type && a.radius == b.radius;
}

/* --------------------------------------------------------------------
 * Morphological Distance Feather Weights.
 */

MorphologicalDistanceFeatherWeights::MorphologicalDistanceFeatherWeights(Context &context,
                                                                         int type,
                                                                         int radius)
    : weights_result(context.create_result(ResultType::Float)),
      falloffs_result(context.create_result(ResultType::Float))
{
  this->compute_weights(radius);
  this->compute_distance_falloffs(type, radius);

  if (context.use_gpu()) {
    this->weights_result.allocate_texture(Domain(int2(weights_.size(), 1)), false);
    this->falloffs_result.allocate_texture(Domain(int2(falloffs_.size(), 1)), false);
    GPU_texture_update(this->weights_result, GPU_DATA_FLOAT, weights_.data());
    GPU_texture_update(this->falloffs_result, GPU_DATA_FLOAT, falloffs_.data());

    /* CPU-side data no longer needed, so free it. */
    weights_ = Array<float>();
    falloffs_ = Array<float>();
  }
  else {
    this->weights_result.wrap_external(weights_.data(), int2(weights_.size(), 1));
    this->falloffs_result.wrap_external(falloffs_.data(), int2(falloffs_.size(), 1));
  }
}

MorphologicalDistanceFeatherWeights::~MorphologicalDistanceFeatherWeights()
{
  weights_result.release();
  falloffs_result.release();
}

void MorphologicalDistanceFeatherWeights::compute_weights(int radius)
{
  /* The size of filter is double the radius plus 1, but since the filter is symmetric, we only
   * compute half of it and no doubling happens. We add 1 to make sure the filter size is always
   * odd and there is a center weight. */
  const int size = radius + 1;
  weights_ = Array<float>(size);

  float sum = 0.0f;

  /* First, compute the center weight. */
  const float center_weight = RE_filter_value(R_FILTER_GAUSS, 0.0f);
  weights_[0] = center_weight;
  sum += center_weight;

  /* Second, compute the other weights in the positive direction, making sure to add double the
   * weight to the sum of weights because the filter is symmetric and we only loop over half of
   * it. Skip the center weight already computed by dropping the front index. */
  const float scale = radius > 0.0f ? 1.0f / radius : 0.0f;
  for (const int i : weights_.index_range().drop_front(1)) {
    const float weight = RE_filter_value(R_FILTER_GAUSS, i * scale);
    weights_[i] = weight;
    sum += weight * 2.0f;
  }

  /* Finally, normalize the weights. */
  for (const int i : weights_.index_range()) {
    weights_[i] /= sum;
  }
}

/* Computes a falloff that is equal to 1 at an input of zero and decrease to zero at an input of 1,
 * with the rate of decrease depending on the falloff type. */
static float compute_distance_falloff(int type, float x)
{
  x = 1.0f - x;

  switch (type) {
    case PROP_SMOOTH:
      return 3.0f * x * x - 2.0f * x * x * x;
    case PROP_SPHERE:
      return std::sqrt(2.0f * x - x * x);
    case PROP_ROOT:
      return std::sqrt(x);
    case PROP_SHARP:
      return x * x;
    case PROP_INVSQUARE:
      return x * (2.0f - x);
    case PROP_LIN:
      return x;
    default:
      BLI_assert_unreachable();
      return x;
  }
}

void MorphologicalDistanceFeatherWeights::compute_distance_falloffs(int type, int radius)
{
  /* The size of the distance falloffs is double the radius plus 1, but since the falloffs are
   * symmetric, we only compute half of them and no doubling happens. We add 1 to make sure the
   * falloffs size is always odd and there is a center falloff. */
  const int size = radius + 1;
  falloffs_ = Array<float>(size);

  /* Compute the distance falloffs in the positive direction only, because the falloffs are
   * symmetric. */
  const float scale = radius > 0.0f ? 1.0f / radius : 0.0f;
  for (const int i : falloffs_.index_range()) {
    falloffs_[i] = compute_distance_falloff(type, i * scale);
  }
}

/* --------------------------------------------------------------------
 * Morphological Distance Feather Weights Container.
 */

void MorphologicalDistanceFeatherWeightsContainer::reset()
{
  /* First, delete all resources that are no longer needed. */
  map_.remove_if([](auto item) { return !item.value->needed; });

  /* Second, reset the needed status of the remaining resources to false to ready them to track
   * their needed status for the next evaluation. */
  for (auto &value : map_.values()) {
    value->needed = false;
  }
}

MorphologicalDistanceFeatherWeights &MorphologicalDistanceFeatherWeightsContainer::get(
    Context &context, int type, int radius)
{
  const MorphologicalDistanceFeatherWeightsKey key(type, radius);

  auto &weights = *map_.lookup_or_add_cb(key, [&]() {
    return std::make_unique<MorphologicalDistanceFeatherWeights>(context, type, radius);
  });

  weights.needed = true;
  return weights;
}

}  // namespace blender::realtime_compositor
