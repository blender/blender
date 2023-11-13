/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* Get the normalized coordinates of the pixel centers. */
  vec2 normalized_texel = (vec2(texel) + vec2(0.5)) / vec2(texture_size(input_tx));

  /* Sample the red and blue channels shifted by the dispersion amount. */
  const float red = texture(input_tx, normalized_texel + vec2(dispersion, 0.0)).r;
  const float green = texture_load(input_tx, texel).g;
  const float blue = texture(input_tx, normalized_texel - vec2(dispersion, 0.0)).b;

  imageStore(output_img, texel, vec4(red, green, blue, 1.0));
}
