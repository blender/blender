/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  /* The lower bound is inclusive and upper bound is exclusive. */
  bool is_inside = all(greaterThanEqual(texel, lower_bound)) && all(lessThan(texel, upper_bound));
  /* Write the pixel color if it is inside the cropping region, otherwise, write zero. */
  vec4 color = is_inside ? texture_load(input_tx, texel) : vec4(0.0);
  imageStore(output_img, texel, color);
}
