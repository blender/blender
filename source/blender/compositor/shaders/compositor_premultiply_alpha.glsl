/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 input_color = texture_load(input_tx, texel);
  imageStore(output_img, texel, input_color * float4(float3(input_color.a), 1.0f));
}
