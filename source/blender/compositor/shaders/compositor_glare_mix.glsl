/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* Add 0.5 to evaluate the input sampler at the center of the pixel and divide by the input image
   * size to get the relevant coordinates into the sampler's expected [0, 1] range. Make sure the
   * input color is not negative to avoid a subtractive effect when mixing the glare. */
  vec2 normalized_coordinates = (vec2(texel) + vec2(0.5)) / vec2(texture_size(input_tx));
  vec4 glare_color = texture(glare_tx, normalized_coordinates);
  vec4 input_color = max(vec4(0.0), texture_load(input_tx, texel));

  vec3 highlights = input_color.rgb + glare_color.rgb * strength;

  imageStore(output_img, texel, vec4(highlights, input_color.a));
}
