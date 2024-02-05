/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  ivec2 start = (texel / ivec2(pixel_size)) * ivec2(pixel_size);
  ivec2 end = min(start + ivec2(pixel_size), texture_size(input_tx));

  vec4 accumulated_color = vec4(0.0);
  for (int y = start.y; y < end.y; y++) {
    for (int x = start.x; x < end.x; x++) {
      accumulated_color += texture_load_unbound(input_tx, ivec2(x, y));
    }
  }

  ivec2 size = end - start;
  int count = size.x * size.y;
  imageStore(output_img, texel, accumulated_color / count);
}
