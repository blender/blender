/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <memory>

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_index_range.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "RE_pipeline.h"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "COM_symmetric_blur_weights.hh"

namespace blender::realtime_compositor {

/* --------------------------------------------------------------------
 * Symmetric Blur Weights Key.
 */

SymmetricBlurWeightsKey::SymmetricBlurWeightsKey(int type, float2 radius)
    : type(type), radius(radius)
{
}

uint64_t SymmetricBlurWeightsKey::hash() const
{
  return get_default_hash_3(type, radius.x, radius.y);
}

bool operator==(const SymmetricBlurWeightsKey &a, const SymmetricBlurWeightsKey &b)
{
  return a.type == b.type && a.radius == b.radius;
}

/* --------------------------------------------------------------------
 * Symmetric Blur Weights.
 */

SymmetricBlurWeights::SymmetricBlurWeights(int type, float2 radius)
{
  /* The full size of filter is double the radius plus 1, but since the filter is symmetric, we
   * only compute a single quadrant of it and so no doubling happens. We add 1 to make sure the
   * filter size is always odd and there is a center weight. */
  const float2 scale = math::safe_divide(float2(1.0f), radius);
  const int2 size = int2(math::ceil(radius)) + int2(1);
  Array<float> weights(size.x * size.y);

  float sum = 0.0f;

  /* First, compute the center weight. */
  const float center_weight = RE_filter_value(type, 0.0f);
  weights[0] = center_weight;
  sum += center_weight;

  /* Then, compute the weights along the positive x axis, making sure to add double the weight to
   * the sum of weights because the filter is symmetric and we only loop over the positive half
   * of the x axis. Skip the center weight already computed by dropping the front index. */
  for (const int x : IndexRange(size.x).drop_front(1)) {
    const float weight = RE_filter_value(type, x * scale.x);
    weights[x] = weight;
    sum += weight * 2.0f;
  }

  /* Then, compute the weights along the positive y axis, making sure to add double the weight to
   * the sum of weights because the filter is symmetric and we only loop over the positive half
   * of the y axis. Skip the center weight already computed by dropping the front index. */
  for (const int y : IndexRange(size.y).drop_front(1)) {
    const float weight = RE_filter_value(type, y * scale.y);
    weights[size.x * y] = weight;
    sum += weight * 2.0f;
  }

  /* Then, compute the other weights in the upper right quadrant, making sure to add quadruple
   * the weight to the sum of weights because the filter is symmetric and we only loop over one
   * quadrant of it. Skip the weights along the y and x axis already computed by dropping the
   * front index. */
  for (const int y : IndexRange(size.y).drop_front(1)) {
    for (const int x : IndexRange(size.x).drop_front(1)) {
      const float weight = RE_filter_value(type, math::length(float2(x, y) * scale));
      weights[size.x * y + x] = weight;
      sum += weight * 4.0f;
    }
  }

  /* Finally, normalize the weights. */
  for (const int y : IndexRange(size.y)) {
    for (const int x : IndexRange(size.x)) {
      weights[size.x * y + x] /= sum;
    }
  }

  texture_ = GPU_texture_create_2d(
      "Weights", size.x, size.y, 1, GPU_R16F, GPU_TEXTURE_USAGE_GENERAL, weights.data());
}

SymmetricBlurWeights::~SymmetricBlurWeights()
{
  GPU_texture_free(texture_);
}

void SymmetricBlurWeights::bind_as_texture(GPUShader *shader, const char *texture_name) const
{
  const int texture_image_unit = GPU_shader_get_sampler_binding(shader, texture_name);
  GPU_texture_bind(texture_, texture_image_unit);
}

void SymmetricBlurWeights::unbind_as_texture() const
{
  GPU_texture_unbind(texture_);
}

/* --------------------------------------------------------------------
 * Symmetric Blur Weights Container.
 */

void SymmetricBlurWeightsContainer::reset()
{
  /* First, delete all resources that are no longer needed. */
  map_.remove_if([](auto item) { return !item.value->needed; });

  /* Second, reset the needed status of the remaining resources to false to ready them to track
   * their needed status for the next evaluation. */
  for (auto &value : map_.values()) {
    value->needed = false;
  }
}

SymmetricBlurWeights &SymmetricBlurWeightsContainer::get(int type, float2 radius)
{
  const SymmetricBlurWeightsKey key(type, radius);

  auto &weights = *map_.lookup_or_add_cb(
      key, [&]() { return std::make_unique<SymmetricBlurWeights>(type, radius); });

  weights.needed = true;
  return weights;
}

}  // namespace blender::realtime_compositor
