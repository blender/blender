/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  const float2 direction_to_line_point = position - float2(texel);
  const float projection = dot(normal, direction_to_line_point);

  bool is_below_line = projection <= 0;
  float4 color = is_below_line ? texture_load(first_image_tx, texel) :
                                 texture_load(second_image_tx, texel);
  imageStore(output_img, texel, color);
}
