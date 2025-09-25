/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"

/* Given the texel in the range [-radius, radius] in both axis, load the appropriate weight from
 * the weights texture, where the given texel (0, 0) corresponds the center of weights texture.
 * Note that we load the weights texture inverted along both directions to maintain the shape of
 * the weights if it was not symmetrical. To understand why inversion makes sense, consider a 1D
 * weights texture whose right half is all ones and whose left half is all zeros. Further, consider
 * that we are blurring a single white pixel on a black background. When computing the value of a
 * pixel that is to the right of the white pixel, the white pixel will be in the left region of the
 * search window, and consequently, without inversion, a zero will be sampled from the left side of
 * the weights texture and result will be zero. However, what we expect is that pixels to the right
 * of the white pixel will be white, that is, they should sample a weight of 1 from the right side
 * of the weights texture, hence the need for inversion. */
float4 load_weight(int2 texel)
{
  /* Add the radius to transform the texel into the range [0, radius * 2], with an additional 0.5f
   * to sample at the center of the pixels, then divide by the upper bound plus one to transform
   * the texel into the normalized range [0, 1] needed to sample the weights sampler. Finally,
   * invert the textures coordinates by subtracting from 1 to maintain the shape of the weights as
   * mentioned in the function description. */
  return texture(weights_tx,
                 1.0f - ((float2(texel) + float2(radius + 0.5f)) / (radius * 2.0f + 1.0f)));
}

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  /* The mask input is treated as a boolean. If it is zero, then no blurring happens for this
   * pixel. Otherwise, the pixel is blurred normally and the mask value is irrelevant. */
  float mask = texture_load(mask_tx, texel).x;
  if (mask == 0.0f) {
    imageStore(output_img, texel, texture_load(input_tx, texel));
    return;
  }

  /* Go over the window of the given radius and accumulate the colors multiplied by their
   * respective weights as well as the weights themselves. */
  float4 accumulated_color = float4(0.0f);
  float4 accumulated_weight = float4(0.0f);
  for (int y = -radius; y <= radius; y++) {
    for (int x = -radius; x <= radius; x++) {
      float4 weight = load_weight(int2(x, y));
      accumulated_color += texture_load(input_tx, texel + int2(x, y)) * weight;
      accumulated_weight += weight;
    }
  }

  imageStore(output_img, texel, safe_divide(accumulated_color, accumulated_weight));
}
