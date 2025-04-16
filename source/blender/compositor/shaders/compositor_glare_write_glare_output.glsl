/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_color_utils.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  float2 normalized_coordinates = (float2(texel) + float2(0.5f)) / float2(imageSize(output_img));
  float4 glare_color = texture(input_tx, normalized_coordinates);

  /* Adjust saturation of glare. */
  float4 glare_hsva;
  rgb_to_hsv(glare_color, glare_hsva);
  glare_hsva.y = clamp(glare_hsva.y * saturation, 0.0f, 1.0f);
  float4 glare_rgba;
  hsv_to_rgb(glare_hsva, glare_rgba);

  float3 adjusted_glare_value = glare_rgba.rgb * tint;
  imageStore(output_img, texel, float4(adjusted_glare_value, 1.0f));
}
