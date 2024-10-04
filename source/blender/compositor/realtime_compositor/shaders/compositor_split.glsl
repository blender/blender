/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 output_size = imageSize(output_img);
#if defined(SPLIT_HORIZONTAL)
  bool condition = (output_size.x * split_ratio) <= texel.x;
#elif defined(SPLIT_VERTICAL)
  bool condition = (output_size.y * split_ratio) <= texel.y;
#endif
  vec4 color = condition ? texture_load(first_image_tx, texel) :
                           texture_load(second_image_tx, texel);
  imageStore(output_img, texel, color);
}
