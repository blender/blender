/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_color_utils.glsl"
#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  float4 color_ycca;
  rgba_to_ycca_itu_709(texture_load(input_tx, texel), color_ycca);

  imageStore(output_img, texel, color_ycca);
}
