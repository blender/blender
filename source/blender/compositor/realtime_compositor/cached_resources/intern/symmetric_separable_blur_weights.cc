/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <memory>

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_index_range.hh"
#include "BLI_math_base.hh"

#include "RE_pipeline.h"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "COM_symmetric_separable_blur_weights.hh"

namespace blender::realtime_compositor {

/* --------------------------------------------------------------------
 * Symmetric Separable Blur Weights Key.
 */

SymmetricSeparableBlurWeightsKey::SymmetricSeparableBlurWeightsKey(int type, float radius)
    : type(type), radius(radius)
{
}

uint64_t SymmetricSeparableBlurWeightsKey::hash() const
{
  return get_default_hash_2(type, radius);
}

bool operator==(const SymmetricSeparableBlurWeightsKey &a,
                const SymmetricSeparableBlurWeightsKey &b)
{
  return a.type == b.type && a.radius == b.radius;
}

/* --------------------------------------------------------------------
 * Symmetric Separable Blur Weights.
 */

SymmetricSeparableBlurWeights::SymmetricSeparableBlurWeights(int type, float radius)
{
  /* The size of filter is double the radius plus 1, but since the filter is symmetric, we only
   * compute half of it and no doubling happens. We add 1 to make sure the filter size is always
   * odd and there is a center weight. */
  const int size = math::ceil(radius) + 1;
  Array<float> weights(size);

  float sum = 0.0f;

  /* First, compute the center weight. */
  const float center_weight = RE_filter_value(type, 0.0f);
  weights[0] = center_weight;
  sum += center_weight;

  /* Second, compute the other weights in the positive direction, making sure to add double the
   * weight to the sum of weights because the filter is symmetric and we only loop over half of
   * it. Skip the center weight already computed by dropping the front index. */
  const float scale = radius > 0.0f ? 1.0f / radius : 0.0f;
  for (const int i : weights.index_range().drop_front(1)) {
    const float weight = RE_filter_value(type, i * scale);
    weights[i] = weight;
    sum += weight * 2.0f;
  }

  /* Finally, normalize the weights. */
  for (const int i : weights.index_range()) {
    weights[i] /= sum;
  }

  texture_ = GPU_texture_create_1d(
      "Weights", size, 1, GPU_R16F, GPU_TEXTURE_USAGE_GENERAL, weights.data());
}

SymmetricSeparableBlurWeights::~SymmetricSeparableBlurWeights()
{
  GPU_texture_free(texture_);
}

void SymmetricSeparableBlurWeights::bind_as_texture(GPUShader *shader,
                                                    const char *texture_name) const
{
  const int texture_image_unit = GPU_shader_get_sampler_binding(shader, texture_name);
  GPU_texture_bind(texture_, texture_image_unit);
}

void SymmetricSeparableBlurWeights::unbind_as_texture() const
{
  GPU_texture_unbind(texture_);
}

/* --------------------------------------------------------------------
 * Symmetric Separable Blur Weights Container.
 */

void SymmetricSeparableBlurWeightsContainer::reset()
{
  /* First, delete all resources that are no longer needed. */
  map_.remove_if([](auto item) { return !item.value->needed; });

  /* Second, reset the needed status of the remaining resources to false to ready them to track
   * their needed status for the next evaluation. */
  for (auto &value : map_.values()) {
    value->needed = false;
  }
}

SymmetricSeparableBlurWeights &SymmetricSeparableBlurWeightsContainer::get(int type, float radius)
{
  const SymmetricSeparableBlurWeightsKey key(type, radius);

  auto &weights = *map_.lookup_or_add_cb(
      key, [&]() { return std::make_unique<SymmetricSeparableBlurWeights>(type, radius); });

  weights.needed = true;
  return weights;
}

}  // namespace blender::realtime_compositor
