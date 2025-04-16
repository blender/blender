/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_bicubic_sampler_lib.glsl"
#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int2 input_size = texture_size(input_tx);

  float2 coordinates = (float2(texel) + float2(0.5f)) / float2(input_size);
  float2 center = float2(0.5f);

  float2 scale = float2(texture_load(x_scale_tx, texel).x, texture_load(y_scale_tx, texel).x);
  float2 scaled_coordinates = center + (coordinates - center) / max(scale, 0.0001f);

  imageStore(output_img, texel, SAMPLER_FUNCTION(input_tx, scaled_coordinates));
}
