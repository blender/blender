/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_color_utils.glsl"
#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* Make sure the input is not negative to avoid a subtractive effect when mixing the glare. */
  vec4 input_color = max(vec4(0.0), texture_load(input_tx, texel));

  vec2 normalized_coordinates = (vec2(texel) + vec2(0.5)) / vec2(texture_size(input_tx));
  vec4 glare_color = texture(glare_tx, normalized_coordinates);

  /* Adjust saturation of glare. */
  vec4 glare_hsva;
  rgb_to_hsv(glare_color, glare_hsva);
  glare_hsva.y = clamp(glare_hsva.y * saturation, 0.0, 1.0);
  vec4 glare_rgba;
  hsv_to_rgb(glare_hsva, glare_rgba);

  vec3 combined_color = input_color.rgb + glare_rgba.rgb * tint;

  imageStore(output_img, texel, vec4(combined_color, input_color.a));
}
