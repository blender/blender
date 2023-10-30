/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  vec4 pass_color = texture_load(input_tx, texel + compositing_region_lower_bound);
  imageStore(output_img, texel, READ_EXPRESSION(pass_color));
}
