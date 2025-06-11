/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  if (zero_pad) {
    imageStore(output_img, texel, texture_load(input_tx, texel - size, float4(0.0f)));
  }
  else {
    imageStore(output_img, texel, texture_load(input_tx, texel - size));
  }
}
