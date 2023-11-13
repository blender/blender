/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  float input_mask_value = texture_load(input_mask_tx, texel).x;
  float mask = int(round(input_mask_value)) == index ? 1.0 : 0.0;

  imageStore(output_mask_img, texel, vec4(mask));
}
