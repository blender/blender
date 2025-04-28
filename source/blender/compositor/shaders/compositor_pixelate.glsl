/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  int2 start = (texel / int2(pixel_size)) * int2(pixel_size);
  int2 end = min(start + int2(pixel_size), texture_size(input_tx));

  float4 accumulated_color = float4(0.0f);
  for (int y = start.y; y < end.y; y++) {
    for (int x = start.x; x < end.x; x++) {
      accumulated_color += texture_load_unbound(input_tx, int2(x, y));
    }
  }

  int2 size = end - start;
  int count = size.x * size.y;
  imageStore(output_img, texel, accumulated_color / count);
}
