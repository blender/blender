/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_quaternion.hh"

#include "BKE_attribute_math.hh"

namespace blender::bke::attribute_math {

template<>
math::Quaternion mix2(const float factor, const math::Quaternion &a, const math::Quaternion &b)
{
  return math::interpolate(a, b, factor);
}

template<>
math::Quaternion mix3(const float3 &weights,
                      const math::Quaternion &v0,
                      const math::Quaternion &v1,
                      const math::Quaternion &v2)
{
  const float3 expmap_mixed = mix3(weights, v0.expmap(), v1.expmap(), v2.expmap());
  return math::Quaternion::expmap(expmap_mixed);
}

template<>
math::Quaternion mix4(const float4 &weights,
                      const math::Quaternion &v0,
                      const math::Quaternion &v1,
                      const math::Quaternion &v2,
                      const math::Quaternion &v3)
{
  const float3 expmap_mixed = mix4(weights, v0.expmap(), v1.expmap(), v2.expmap(), v3.expmap());
  return math::Quaternion::expmap(expmap_mixed);
}

template<> float4x4 mix2(const float factor, const float4x4 &a, const float4x4 &b)
{
  return math::interpolate(a, b, factor);
}

template<>
float4x4 mix3(const float3 &weights, const float4x4 &v0, const float4x4 &v1, const float4x4 &v2)
{
  const float3 location = mix3(weights, v0.location(), v1.location(), v2.location());
  const math::Quaternion rotation = mix3(
      weights, math::to_quaternion(v0), math::to_quaternion(v1), math::to_quaternion(v2));
  const float3 scale = mix3(weights, math::to_scale(v0), math::to_scale(v1), math::to_scale(v2));
  return math::from_loc_rot_scale<float4x4>(location, rotation, scale);
}

template<>
float4x4 mix4(const float4 &weights,
              const float4x4 &v0,
              const float4x4 &v1,
              const float4x4 &v2,
              const float4x4 &v3)
{
  const float3 location = mix4(
      weights, v0.location(), v1.location(), v2.location(), v3.location());
  const math::Quaternion rotation = mix4(weights,
                                         math::to_quaternion(v0),
                                         math::to_quaternion(v1),
                                         math::to_quaternion(v2),
                                         math::to_quaternion(v3));
  const float3 scale = mix4(
      weights, math::to_scale(v0), math::to_scale(v1), math::to_scale(v2), math::to_scale(v3));
  return math::from_loc_rot_scale<float4x4>(location, rotation, scale);
}

ColorGeometry4fMixer::ColorGeometry4fMixer(MutableSpan<ColorGeometry4f> buffer,
                                           ColorGeometry4f default_color)
    : ColorGeometry4fMixer(buffer, buffer.index_range(), default_color)
{
}

ColorGeometry4fMixer::ColorGeometry4fMixer(MutableSpan<ColorGeometry4f> buffer,
                                           const IndexMask &mask,
                                           const ColorGeometry4f default_color)
    : buffer_(buffer), default_color_(default_color), total_weights_(buffer.size(), 0.0f)
{
  const ColorGeometry4f zero{0.0f, 0.0f, 0.0f, 0.0f};
  mask.foreach_index([&](const int64_t i) { buffer_[i] = zero; });
}

void ColorGeometry4fMixer::set(const int64_t index,
                               const ColorGeometry4f &color,
                               const float weight)
{
  buffer_[index].r = color.r * weight;
  buffer_[index].g = color.g * weight;
  buffer_[index].b = color.b * weight;
  buffer_[index].a = color.a * weight;
  total_weights_[index] = weight;
}

void ColorGeometry4fMixer::mix_in(const int64_t index,
                                  const ColorGeometry4f &color,
                                  const float weight)
{
  ColorGeometry4f &output_color = buffer_[index];
  output_color.r += color.r * weight;
  output_color.g += color.g * weight;
  output_color.b += color.b * weight;
  output_color.a += color.a * weight;
  total_weights_[index] += weight;
}

void ColorGeometry4fMixer::finalize()
{
  this->finalize(buffer_.index_range());
}

void ColorGeometry4fMixer::finalize(const IndexMask &mask)
{
  mask.foreach_index([&](const int64_t i) {
    const float weight = total_weights_[i];
    ColorGeometry4f &output_color = buffer_[i];
    if (weight > 0.0f) {
      const float weight_inv = 1.0f / weight;
      output_color.r *= weight_inv;
      output_color.g *= weight_inv;
      output_color.b *= weight_inv;
      output_color.a *= weight_inv;
    }
    else {
      output_color = default_color_;
    }
  });
}

ColorGeometry4bMixer::ColorGeometry4bMixer(MutableSpan<ColorGeometry4b> buffer,
                                           const ColorGeometry4b default_color)
    : ColorGeometry4bMixer(buffer, buffer.index_range(), default_color)
{
}

ColorGeometry4bMixer::ColorGeometry4bMixer(MutableSpan<ColorGeometry4b> buffer,
                                           const IndexMask &mask,
                                           const ColorGeometry4b default_color)
    : buffer_(buffer),
      default_color_(default_color),
      total_weights_(buffer.size(), 0.0f),
      accumulation_buffer_(buffer.size(), float4(0, 0, 0, 0))
{
  const ColorGeometry4b zero{0, 0, 0, 0};
  mask.foreach_index([&](const int64_t i) { buffer_[i] = zero; });
}

void ColorGeometry4bMixer::ColorGeometry4bMixer::set(int64_t index,
                                                     const ColorGeometry4b &color,
                                                     const float weight)
{
  accumulation_buffer_[index][0] = color.r * weight;
  accumulation_buffer_[index][1] = color.g * weight;
  accumulation_buffer_[index][2] = color.b * weight;
  accumulation_buffer_[index][3] = color.a * weight;
  total_weights_[index] = weight;
}

void ColorGeometry4bMixer::mix_in(int64_t index, const ColorGeometry4b &color, float weight)
{
  float4 &accum_value = accumulation_buffer_[index];
  accum_value[0] += color.r * weight;
  accum_value[1] += color.g * weight;
  accum_value[2] += color.b * weight;
  accum_value[3] += color.a * weight;
  total_weights_[index] += weight;
}

void ColorGeometry4bMixer::finalize()
{
  this->finalize(buffer_.index_range());
}

void ColorGeometry4bMixer::finalize(const IndexMask &mask)
{
  mask.foreach_index([&](const int64_t i) {
    const float weight = total_weights_[i];
    const float4 &accum_value = accumulation_buffer_[i];
    ColorGeometry4b &output_color = buffer_[i];
    if (weight > 0.0f) {
      const float weight_inv = 1.0f / weight;
      output_color.r = accum_value[0] * weight_inv;
      output_color.g = accum_value[1] * weight_inv;
      output_color.b = accum_value[2] * weight_inv;
      output_color.a = accum_value[3] * weight_inv;
    }
    else {
      output_color = default_color_;
    }
  });
}

float4x4Mixer::float4x4Mixer(MutableSpan<float4x4> buffer)
    : float4x4Mixer(buffer, buffer.index_range())
{
}

float4x4Mixer::float4x4Mixer(MutableSpan<float4x4> buffer, const IndexMask & /*mask*/)
    : buffer_(buffer),
      total_weights_(buffer.size(), 0.0f),
      location_buffer_(buffer.size(), float3(0)),
      expmap_buffer_(buffer.size(), float3(0)),
      scale_buffer_(buffer.size(), float3(0))
{
}

void float4x4Mixer::float4x4Mixer::set(int64_t index, const float4x4 &value, const float weight)
{
  location_buffer_[index] = value.location() * weight;
  expmap_buffer_[index] = math::to_quaternion(value).expmap() * weight;
  scale_buffer_[index] = math::to_scale(value) * weight;
  total_weights_[index] = weight;
}

void float4x4Mixer::mix_in(int64_t index, const float4x4 &value, float weight)
{
  location_buffer_[index] += value.location() * weight;
  expmap_buffer_[index] += math::to_quaternion(value).expmap() * weight;
  scale_buffer_[index] += math::to_scale(value) * weight;
  total_weights_[index] += weight;
}

void float4x4Mixer::finalize()
{
  this->finalize(buffer_.index_range());
}

void float4x4Mixer::finalize(const IndexMask &mask)
{
  mask.foreach_index([&](const int64_t i) {
    const float weight = total_weights_[i];
    if (weight > 0.0f) {
      const float weight_inv = math::rcp(weight);
      buffer_[i] = math::from_loc_rot_scale<float4x4>(
          location_buffer_[i] * weight_inv,
          math::Quaternion::expmap(expmap_buffer_[i] * weight_inv),
          scale_buffer_[i] * weight_inv);
    }
    else {
      buffer_[i] = float4x4::identity();
    }
  });
}

void gather(const GSpan src, const Span<int> map, GMutableSpan dst)
{
  attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    array_utils::gather(src.typed<T>(), map, dst.typed<T>());
  });
}

void gather(const GVArray &src, const Span<int> map, GMutableSpan dst)
{
  attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    array_utils::gather(src.typed<T>(), map, dst.typed<T>());
  });
}

void gather_group_to_group(const OffsetIndices<int> src_offsets,
                           const OffsetIndices<int> dst_offsets,
                           const IndexMask &selection,
                           const GSpan src,
                           GMutableSpan dst)
{
  attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    array_utils::gather_group_to_group(
        src_offsets, dst_offsets, selection, src.typed<T>(), dst.typed<T>());
  });
}

void gather_to_groups(const OffsetIndices<int> dst_offsets,
                      const IndexMask &src_selection,
                      const GSpan src,
                      GMutableSpan dst)
{
  attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    array_utils::gather_to_groups(dst_offsets, src_selection, src.typed<T>(), dst.typed<T>());
  });
}

}  // namespace blender::bke::attribute_math
