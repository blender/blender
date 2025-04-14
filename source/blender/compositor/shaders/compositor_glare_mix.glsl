/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_color_utils.glsl"
#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  /* Make sure the input is not negative to avoid a subtractive effect when mixing the glare. */
  float4 input_color = max(float4(0.0f), texture_load(input_tx, texel));

  float2 normalized_coordinates = (float2(texel) + float2(0.5f)) / float2(texture_size(input_tx));
  float4 glare_color = texture(glare_tx, normalized_coordinates);

  /* Adjust saturation of glare. */
  float4 glare_hsva;
  rgb_to_hsv(glare_color, glare_hsva);
  glare_hsva.y = clamp(glare_hsva.y * saturation, 0.0f, 1.0f);
  float4 glare_rgba;
  hsv_to_rgb(glare_hsva, glare_rgba);

  float3 combined_color = input_color.rgb + glare_rgba.rgb * tint;

  imageStore(output_img, texel, float4(combined_color, input_color.a));
}
