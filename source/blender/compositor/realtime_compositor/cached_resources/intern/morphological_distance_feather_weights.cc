/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <cmath>
#include <cstdint>

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_index_range.hh"

#include "RE_pipeline.h"

#include "DNA_scene_types.h"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "COM_morphological_distance_feather_weights.hh"

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
  return get_default_hash_2(type, radius);
}

bool operator==(const MorphologicalDistanceFeatherWeightsKey &a,
                const MorphologicalDistanceFeatherWeightsKey &b)
{
  return a.type == b.type && a.radius == b.radius;
}

/* --------------------------------------------------------------------
 * Morphological Distance Feather Weights.
 */

MorphologicalDistanceFeatherWeights::MorphologicalDistanceFeatherWeights(int type, int radius)
{
  compute_weights(radius);
  compute_distance_falloffs(type, radius);
}

MorphologicalDistanceFeatherWeights::~MorphologicalDistanceFeatherWeights()
{
  GPU_texture_free(weights_texture_);
  GPU_texture_free(distance_falloffs_texture_);
}

void MorphologicalDistanceFeatherWeights::compute_weights(int radius)
{
  /* The size of filter is double the radius plus 1, but since the filter is symmetric, we only
   * compute half of it and no doubling happens. We add 1 to make sure the filter size is always
   * odd and there is a center weight. */
  const int size = radius + 1;
  Array<float> weights(size);

  float sum = 0.0f;

  /* First, compute the center weight. */
  const float center_weight = RE_filter_value(R_FILTER_GAUSS, 0.0f);
  weights[0] = center_weight;
  sum += center_weight;

  /* Second, compute the other weights in the positive direction, making sure to add double the
   * weight to the sum of weights because the filter is symmetric and we only loop over half of
   * it. Skip the center weight already computed by dropping the front index. */
  const float scale = radius > 0.0f ? 1.0f / radius : 0.0f;
  for (const int i : weights.index_range().drop_front(1)) {
    const float weight = RE_filter_value(R_FILTER_GAUSS, i * scale);
    weights[i] = weight;
    sum += weight * 2.0f;
  }

  /* Finally, normalize the weights. */
  for (const int i : weights.index_range()) {
    weights[i] /= sum;
  }

  weights_texture_ = GPU_texture_create_1d("Weights", size, 1, GPU_R16F, weights.data());
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
  Array<float> falloffs(size);

  /* Compute the distance falloffs in the positive direction only, because the falloffs are
   * symmetric. */
  const float scale = radius > 0.0f ? 1.0f / radius : 0.0f;
  for (const int i : falloffs.index_range()) {
    falloffs[i] = compute_distance_falloff(type, i * scale);
  }

  distance_falloffs_texture_ = GPU_texture_create_1d(
      "Distance Factors", size, 1, GPU_R16F, falloffs.data());
}

void MorphologicalDistanceFeatherWeights::bind_weights_as_texture(GPUShader *shader,
                                                                  const char *texture_name) const
{
  const int texture_image_unit = GPU_shader_get_sampler_binding(shader, texture_name);
  GPU_texture_bind(weights_texture_, texture_image_unit);
}

void MorphologicalDistanceFeatherWeights::unbind_weights_as_texture() const
{
  GPU_texture_unbind(weights_texture_);
}

void MorphologicalDistanceFeatherWeights::bind_distance_falloffs_as_texture(
    GPUShader *shader, const char *texture_name) const
{
  const int texture_image_unit = GPU_shader_get_sampler_binding(shader, texture_name);
  GPU_texture_bind(distance_falloffs_texture_, texture_image_unit);
}

void MorphologicalDistanceFeatherWeights::unbind_distance_falloffs_as_texture() const
{
  GPU_texture_unbind(distance_falloffs_texture_);
}

}  // namespace blender::realtime_compositor
