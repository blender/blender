/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 size = texture_size(input_tx);
  ivec2 flipped_texel = texel;
  if (flip_x) {
    flipped_texel.x = size.x - texel.x - 1;
  }
  if (flip_y) {
    flipped_texel.y = size.y - texel.y - 1;
  }
  imageStore(output_img, texel, texture_load(input_tx, flipped_texel));
}
