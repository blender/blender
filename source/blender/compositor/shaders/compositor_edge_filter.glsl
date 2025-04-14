/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  /* Compute the dot product between the 3x3 window around the pixel and the edge detection kernel
   * in the X direction and Y direction. The Y direction kernel is computed by transposing the
   * given X direction kernel. */
  float3 color_x = float3(0);
  float3 color_y = float3(0);
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 3; i++) {
      float3 color = texture_load(input_tx, texel + int2(i - 1, j - 1)).rgb;
      color_x += color * ukernel[j][i];
      color_y += color * ukernel[i][j];
    }
  }

  /* Compute the channel-wise magnitude of the 2D vector composed from the X and Y edge detection
   * filter results. */
  float3 magnitude = sqrt(color_x * color_x + color_y * color_y);

  /* Mix the channel-wise magnitude with the original color at the center of the kernel using the
   * input factor. */
  float4 color = texture_load(input_tx, texel);
  magnitude = mix(color.rgb, magnitude, texture_load(factor_tx, texel).x);

  /* Store the channel-wise magnitude with the original alpha of the input. */
  imageStore(output_img, texel, float4(magnitude, color.a));
}
