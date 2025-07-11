/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <memory>

#include "BLI_hash.hh"
#include "BLI_index_range.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "RE_pipeline.h"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_symmetric_blur_weights.hh"

namespace blender::compositor {

/* --------------------------------------------------------------------
 * Symmetric Blur Weights Key.
 */

SymmetricBlurWeightsKey::SymmetricBlurWeightsKey(int type, float2 radius)
    : type(type), radius(radius)
{
}

uint64_t SymmetricBlurWeightsKey::hash() const
{
  return get_default_hash(type, radius.x, radius.y);
}

bool operator==(const SymmetricBlurWeightsKey &a, const SymmetricBlurWeightsKey &b)
{
  return a.type == b.type && a.radius == b.radius;
}

/* --------------------------------------------------------------------
 * Symmetric Blur Weights.
 */

SymmetricBlurWeights::SymmetricBlurWeights(Context &context, int type, float2 radius)
    : result(context.create_result(ResultType::Float))
{
  /* The full size of filter is double the radius plus 1, but since the filter is symmetric, we
   * only compute a single quadrant of it and so no doubling happens. We add 1 to make sure the
   * filter size is always odd and there is a center weight. */
  const float2 scale = math::safe_divide(float2(1.0f), radius);
  const int2 size = int2(math::ceil(radius)) + int2(1);
  this->result.allocate_texture(size, false, ResultStorageType::CPU);

  float sum = 0.0f;

  /* First, compute the center weight. */
  const float center_weight = RE_filter_value(type, 0.0f);
  this->result.store_pixel(int2(0, 0), center_weight);
  sum += center_weight;

  /* Then, compute the weights along the positive x axis, making sure to add double the weight to
   * the sum of weights because the filter is symmetric and we only loop over the positive half
   * of the x axis. Skip the center weight already computed by dropping the front index. */
  for (const int x : IndexRange(size.x).drop_front(1)) {
    const float weight = RE_filter_value(type, x * scale.x);
    this->result.store_pixel(int2(x, 0), weight);
    sum += weight * 2.0f;
  }

  /* Then, compute the weights along the positive y axis, making sure to add double the weight to
   * the sum of weights because the filter is symmetric and we only loop over the positive half
   * of the y axis. Skip the center weight already computed by dropping the front index. */
  for (const int y : IndexRange(size.y).drop_front(1)) {
    const float weight = RE_filter_value(type, y * scale.y);
    this->result.store_pixel(int2(0, y), weight);
    sum += weight * 2.0f;
  }

  /* Then, compute the other weights in the upper right quadrant, making sure to add quadruple
   * the weight to the sum of weights because the filter is symmetric and we only loop over one
   * quadrant of it. Skip the weights along the y and x axis already computed by dropping the
   * front index. */
  for (const int y : IndexRange(size.y).drop_front(1)) {
    for (const int x : IndexRange(size.x).drop_front(1)) {
      const float weight = RE_filter_value(type, math::length(float2(x, y) * scale));
      this->result.store_pixel(int2(x, y), weight);
      sum += weight * 4.0f;
    }
  }

  /* Finally, normalize the weights. */
  for (const int y : IndexRange(size.y)) {
    for (const int x : IndexRange(size.x)) {
      const int2 texel = int2(x, y);
      this->result.store_pixel(texel, this->result.load_pixel<float>(texel) / sum);
    }
  }

  if (context.use_gpu()) {
    const Result gpu_result = this->result.upload_to_gpu(false);
    this->result.release();
    this->result = gpu_result;
  }
}

SymmetricBlurWeights::~SymmetricBlurWeights()
{
  this->result.release();
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

Result &SymmetricBlurWeightsContainer::get(Context &context, int type, float2 radius)
{
  const SymmetricBlurWeightsKey key(type, radius);

  auto &weights = *map_.lookup_or_add_cb(
      key, [&]() { return std::make_unique<SymmetricBlurWeights>(context, type, radius); });

  weights.needed = true;
  return weights.result;
}

}  // namespace blender::compositor
