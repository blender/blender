/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  float input_mask_value = texture_load(input_mask_tx, texel).x;
  float mask = int(round(input_mask_value)) == index ? 1.0f : 0.0f;

  imageStore(output_mask_img, texel, float4(mask));
}
