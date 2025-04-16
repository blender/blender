/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  /* Compute the dot product between the 3x3 window around the pixel and the filter kernel. */
  float4 color = float4(0);
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 3; i++) {
      color += texture_load(input_tx, texel + int2(i - 1, j - 1)) * ukernel[j][i];
    }
  }

  /* Mix with the original color at the center of the kernel using the input factor. */
  color = mix(texture_load(input_tx, texel), color, texture_load(factor_tx, texel).x);

  /* Store the color making sure it is not negative. */
  imageStore(output_img, texel, max(color, 0.0f));
}
