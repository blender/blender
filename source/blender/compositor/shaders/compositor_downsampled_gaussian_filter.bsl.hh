/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

namespace compositor::downsampled_gaussian::filtering {

struct Resources {
  [[push_constant]] float2 uv_step;
  [[push_constant]] float center_weight;
  [[push_constant]] int tap_count;
  /* Bilinear taps for one side away from the center. Sizes must match max_tap_pairs. */
  [[push_constant]] float tap_weights[12];
  [[push_constant]] float tap_offsets[12];

  [[sampler(0)]] sampler2D input_tx;
  [[image(0, write, SFLOAT_16_16_16_16)]] image2D output_img;
};

[[compute, local_size(16, 16)]]
void compute_main([[resource_table]] Resources &srt,
                  [[global_invocation_id]] const uint3 global_id)
{
  const int2 texel = int2(global_id.xy);
  const int2 size = imageSize(srt.output_img);
  if (any(greaterThanEqual(texel, size))) {
    return;
  }
  const float2 uv = (float2(texel) + float2(0.5f)) / float2(size);
  float4 color = texelFetch(srt.input_tx, texel, 0) * srt.center_weight;
  for (int i = 0; i < srt.tap_count; i++) {
    const float2 offset = srt.uv_step * srt.tap_offsets[i];
    color += (textureLod(srt.input_tx, uv - offset, 0.0f) +
              textureLod(srt.input_tx, uv + offset, 0.0f)) *
             srt.tap_weights[i];
  }
  imageStore(srt.output_img, texel, color);
}

}  // namespace compositor::downsampled_gaussian::filtering

PipelineCompute compositor_downsampled_gaussian_filter(
    compositor::downsampled_gaussian::filtering::compute_main);
