/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 color = texture_load(input_tx, texel);
  float alpha = color.a > 0.0f ? color.a : 1.0f;
  float3 corrected_color = FUNCTION(max(color.rgb / alpha, float3(0.0f))) * alpha;
  imageStore(output_img, texel, float4(corrected_color, color.a));
}
