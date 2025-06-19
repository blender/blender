/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  float4 accumulated_color = float4(0.0f);

  /* First, compute the contribution of the center pixel. */
  float4 center_color = texture_load(input_tx, texel);
  accumulated_color += center_color * texture_load(weights_tx, int2(0)).x;

  /* Then, compute the contributions of the pixel to the right and left, noting that the
   * weights texture only stores the weights for the positive half, but since the filter is
   * symmetric, the same weight is used for the negative half and we add both of their
   * contributions. */
  for (int i = 1; i < texture_size(weights_tx).x; i++) {
    float weight = texture_load(weights_tx, int2(i, 0)).x;
    accumulated_color += texture_load(input_tx, texel + int2(i, 0)) * weight;
    accumulated_color += texture_load(input_tx, texel + int2(-i, 0)) * weight;
  }

  /* Write the color using the transposed texel. See the execute_separable_blur_horizontal_pass
   * method for more information on the rational behind this. */
  imageStore(output_img, texel.yx, accumulated_color);
}
