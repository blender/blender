/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* The Deriche filter is a parallel interconnection filter, meaning its output is the sum of its
   * causal and non causal filters. */
  vec4 filter_output = texture_load(causal_input_tx, texel) +
                       texture_load(non_causal_input_tx, texel);

  /* Write the color using the transposed texel. See the sum_causal_and_non_causal_results method
   * in the deriche_gaussian_blur.cc file for more information on the rational behind this. */
  imageStore(output_img, texel.yx, filter_output);
}
