/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <memory>

#include "BLI_hash.hh"
#include "BLI_index_range.hh"
#include "BLI_math_base.hh"

#include "RE_pipeline.h"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_symmetric_separable_blur_weights.hh"

namespace blender::compositor {

/* --------------------------------------------------------------------
 * Symmetric Separable Blur Weights Key.
 */

SymmetricSeparableBlurWeightsKey::SymmetricSeparableBlurWeightsKey(int type, float radius)
    : type(type), radius(radius)
{
}

uint64_t SymmetricSeparableBlurWeightsKey::hash() const
{
  return get_default_hash(type, radius);
}

bool operator==(const SymmetricSeparableBlurWeightsKey &a,
                const SymmetricSeparableBlurWeightsKey &b)
{
  return a.type == b.type && a.radius == b.radius;
}

/* --------------------------------------------------------------------
 * Symmetric Separable Blur Weights.
 */

SymmetricSeparableBlurWeights::SymmetricSeparableBlurWeights(Context &context,
                                                             int type,
                                                             float radius)
    : result(context.create_result(ResultType::Float))
{
  /* The size of filter is double the radius plus 1, but since the filter is symmetric, we only
   * compute half of it and no doubling happens. We add 1 to make sure the filter size is always
   * odd and there is a center weight. */
  const int size = math::ceil(radius) + 1;
  this->result.allocate_texture(Domain(int2(size, 1)), false, ResultStorageType::CPU);

  float sum = 0.0f;

  /* First, compute the center weight. */
  const float center_weight = RE_filter_value(type, 0.0f);
  this->result.store_pixel(int2(0, 0), center_weight);
  sum += center_weight;

  /* Second, compute the other weights in the positive direction, making sure to add double the
   * weight to the sum of weights because the filter is symmetric and we only loop over half of
   * it. Skip the center weight already computed by dropping the front index. */
  const float scale = radius > 0.0f ? 1.0f / radius : 0.0f;
  for (const int i : IndexRange(size).drop_front(1)) {
    const float weight = RE_filter_value(type, i * scale);
    this->result.store_pixel(int2(i, 0), weight);
    sum += weight * 2.0f;
  }

  /* Finally, normalize the weights. */
  for (const int i : IndexRange(size)) {
    const int2 texel = int2(i, 0);
    this->result.store_pixel(texel, this->result.load_pixel<float>(texel) / sum);
  }

  if (context.use_gpu()) {
    const Result gpu_result = this->result.upload_to_gpu(false);
    this->result.release();
    this->result = gpu_result;
  }
}

SymmetricSeparableBlurWeights::~SymmetricSeparableBlurWeights()
{
  this->result.release();
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

Result &SymmetricSeparableBlurWeightsContainer::get(Context &context, int type, float radius)
{
  const SymmetricSeparableBlurWeightsKey key(type, radius);

  auto &weights = *map_.lookup_or_add_cb(key, [&]() {
    return std::make_unique<SymmetricSeparableBlurWeights>(context, type, radius);
  });

  weights.needed = true;
  return weights.result;
}

}  // namespace blender::compositor
