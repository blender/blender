/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  float accumulated_weight = 0.0f;
  float4 accumulated_color = float4(0.0f);

  /* First, compute the contribution of the center pixel. */
  float4 center_color = texture_load(input_tx, texel);
  float center_weight = texture_load(weights_tx, int2(0)).x;
  accumulated_color += center_color * center_weight;
  accumulated_weight += center_weight;

  /* The dispatch domain is transposed in the vertical pass, so make sure to reverse transpose the
   * texel coordinates when loading the radius. See the horizontal_pass function in the
   * symmetric_separable_blur_variable_size.cc file for more information. */
  int radius = int(texture_load(radius_tx, is_vertical_pass ? texel.yx : texel).x);

  /* Then, compute the contributions of the pixel to the right and left, noting that the
   * weights texture only stores the weights for the positive half, but since the filter is
   * symmetric, the same weight is used for the negative half and we add both of their
   * contributions. */
  for (int i = 1; i <= radius; i++) {
    /* Add 0.5 to evaluate at the center of the pixels. */
    float weight = texture(weights_tx, float2((float(i) + 0.5f) / float(radius + 1), 0.0f)).x;
    accumulated_color += texture_load(input_tx, texel + int2(i, 0)) * weight;
    accumulated_color += texture_load(input_tx, texel + int2(-i, 0)) * weight;
    accumulated_weight += weight * 2.0f;
  }

  /* Write the color using the transposed texel. See the horizontal_pass function mentioned above
   * for more information on the rational behind this. */
  imageStore(output_img, texel.yx, accumulated_color / accumulated_weight);
}
