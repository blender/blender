/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

namespace compositor::downsampled_gaussian::reconstruction {

struct Resources {
  [[push_constant]] int2 input_size;
  [[push_constant]] int2 input_padding;

  [[sampler(0)]] sampler2D input_tx;
  [[image(0, write, SFLOAT_16_16_16_16)]] image2D output_img;
};

/* Use bilinear up to 2x enlargement, otherwise a positive cubic B-spline. The four B-spline
 * weights are bilinearly paired: placed at `w1 / (w0 + w1)` between two texel centers, scaled by
 * `w0 + w1`. Return both sample coordinates in the padded texture, and the second pair's weight.
 * Bilinear axes return the same coordinate twice and a zero weight. */
float3 cubic_axis(float uv, int size, int output_size, int padding)
{
  const float inverse_physical_size = 1.0f / float(size + 2 * padding);
  if (2 * size >= output_size) {
    const float p = (uv * float(size) + float(padding)) * inverse_physical_size;
    return float3(p, p, 0.0f);
  }
  const float p = uv * float(size) - 0.5f;
  const float base = floor(p);
  const float f = p - base;
  const float w0 = (1.0f - f) * (1.0f - f) * (1.0f - f) / 6.0f;
  const float w1 = (3.0f * f * f * f - 6.0f * f * f + 4.0f) / 6.0f;
  const float w2 = (-3.0f * f * f * f + 3.0f * f * f + 3.0f * f + 1.0f) / 6.0f;
  const float w3 = f * f * f / 6.0f;
  return float3((base - 0.5f + w1 / (w0 + w1) + float(padding)) * inverse_physical_size,
                (base + 1.5f + w3 / (w2 + w3) + float(padding)) * inverse_physical_size,
                w2 + w3);
}

[[compute, local_size(16, 16)]]
void compute_main([[resource_table]] Resources &srt,
                  [[global_invocation_id]] const uint3 global_id)
{
  const int2 texel = int2(global_id.xy);
  const int2 output_size = imageSize(srt.output_img);
  if (any(greaterThanEqual(texel, output_size))) {
    return;
  }
  const float2 uv = (float2(texel) + float2(0.5f)) / float2(output_size);
  const int2 size = srt.input_size;
  const float3 x_axis = cubic_axis(uv.x, size.x, output_size.x, srt.input_padding.x);
  const float3 y_axis = cubic_axis(uv.y, size.y, output_size.y, srt.input_padding.y);
  float4 color = textureLod(srt.input_tx, float2(x_axis.x, y_axis.x), 0.0f);
  if (2 * size.x < output_size.x) {
    color = mix(color, textureLod(srt.input_tx, float2(x_axis.y, y_axis.x), 0.0f), x_axis.z);
  }
  if (2 * size.y < output_size.y) {
    float4 row = textureLod(srt.input_tx, float2(x_axis.x, y_axis.y), 0.0f);
    if (2 * size.x < output_size.x) {
      row = mix(row, textureLod(srt.input_tx, float2(x_axis.y, y_axis.y), 0.0f), x_axis.z);
    }
    color = mix(color, row, y_axis.z);
  }
  imageStore(srt.output_img, texel, color);
}

}  // namespace compositor::downsampled_gaussian::reconstruction

PipelineCompute compositor_downsampled_gaussian_reconstruct(
    compositor::downsampled_gaussian::reconstruction::compute_main);
