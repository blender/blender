/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute_math.hh"

namespace blender::attribute_math {

ColorGeometry4fMixer::ColorGeometry4fMixer(MutableSpan<ColorGeometry4f> output_buffer,
                                           ColorGeometry4f default_color)
    : buffer_(output_buffer),
      default_color_(default_color),
      total_weights_(output_buffer.size(), 0.0f)
{
  buffer_.fill(ColorGeometry4f(0.0f, 0.0f, 0.0f, 0.0f));
}

void ColorGeometry4fMixer::mix_in(const int64_t index,
                                  const ColorGeometry4f &color,
                                  const float weight)
{
  BLI_assert(weight >= 0.0f);
  ColorGeometry4f &output_color = buffer_[index];
  output_color.r += color.r * weight;
  output_color.g += color.g * weight;
  output_color.b += color.b * weight;
  output_color.a += color.a * weight;
  total_weights_[index] += weight;
}

void ColorGeometry4fMixer::finalize()
{
  for (const int64_t i : buffer_.index_range()) {
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
  }
}

ColorGeometry4bMixer::ColorGeometry4bMixer(MutableSpan<ColorGeometry4b> buffer,
                                           ColorGeometry4b default_color)
    : buffer_(buffer),
      default_color_(default_color),
      total_weights_(buffer.size(), 0.0f),
      accumulation_buffer_(buffer.size(), float4(0, 0, 0, 0))
{
}

void ColorGeometry4bMixer::mix_in(int64_t index, const ColorGeometry4b &color, float weight)
{
  BLI_assert(weight >= 0.0f);
  float4 &accum_value = accumulation_buffer_[index];
  accum_value[0] += color.r * weight;
  accum_value[1] += color.g * weight;
  accum_value[2] += color.b * weight;
  accum_value[3] += color.a * weight;
  total_weights_[index] += weight;
}

void ColorGeometry4bMixer::finalize()
{
  for (const int64_t i : buffer_.index_range()) {
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
  }
}

}  // namespace blender::attribute_math
