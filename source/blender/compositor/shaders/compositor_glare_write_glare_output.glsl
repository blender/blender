/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_color_utils.glsl"

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  vec2 normalized_coordinates = (vec2(texel) + vec2(0.5)) / vec2(imageSize(output_img));
  vec4 glare_color = texture(input_tx, normalized_coordinates);

  /* Adjust saturation of glare. */
  vec4 glare_hsva;
  rgb_to_hsv(glare_color, glare_hsva);
  glare_hsva.y = clamp(glare_hsva.y * saturation, 0.0, 1.0);
  vec4 glare_rgba;
  hsv_to_rgb(glare_hsva, glare_rgba);

  vec3 adjusted_glare_value = glare_rgba.rgb * tint;
  imageStore(output_img, texel, vec4(adjusted_glare_value, 1.0));
}
