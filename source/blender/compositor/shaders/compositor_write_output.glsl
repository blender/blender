/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int2 output_texel = texel + lower_bound;
  if (any(greaterThan(output_texel, upper_bound))) {
    return;
  }

  float4 input_color = texture_load(input_tx, texel);

#if defined(DIRECT_OUTPUT)
  float4 output_color = input_color;
#elif defined(OPAQUE_OUTPUT)
  float4 output_color = float4(input_color.rgb, 1.0f);
#elif defined(ALPHA_OUTPUT)
  float alpha = texture_load(alpha_tx, texel).x;
  float4 output_color = float4(input_color.rgb, alpha);
#endif

  imageStore(output_img, texel + lower_bound, output_color);
}
