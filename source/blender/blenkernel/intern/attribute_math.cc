/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"

#include "BKE_attribute_math.hh"

namespace blender::bke::attribute_math {

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

}  // namespace blender::bke::attribute_math
