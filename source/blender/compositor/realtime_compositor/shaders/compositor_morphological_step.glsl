/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* Find the minimum/maximum value in the window of the given radius around the pixel. This is
   * essentially a morphological operator with a square structuring element. The LIMIT value should
   * be FLT_MAX if OPERATOR is min and FLT_MIN if OPERATOR is max. */
  float value = LIMIT;
  for (int i = -radius; i <= radius; i++) {
    value = OPERATOR(value, texture_load(input_tx, texel + ivec2(i, 0), vec4(LIMIT)).x);
  }

  /* Write the value using the transposed texel. See the execute_step_horizontal_pass method for
   * more information on the rational behind this. */
  imageStore(output_img, texel.yx, vec4(value));
}
