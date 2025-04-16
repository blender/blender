/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  int width = imageSize(horizontal_img).x;

  /* For each iteration, apply a causal filter followed by a non causal filters along the row
   * mapped to the current thread invocation. */
  for (int i = 0; i < iterations; i++) {
    /* Causal Pass:
     * Sequentially apply a causal filter running from left to right by mixing the value of the
     * pixel in the row with the average value of the previous output and next input in the same
     * row. */
    for (int x = 0; x < width; x++) {
      int2 texel = int2(x, gl_GlobalInvocationID.x);
      float4 previous_output = imageLoad(horizontal_img, texel - int2(i, 0));
      float4 current_input = imageLoad(horizontal_img, texel);
      float4 next_input = imageLoad(horizontal_img, texel + int2(i, 0));

      float4 neighbor_average = (previous_output + next_input) / 2.0f;
      float4 causal_output = mix(current_input, neighbor_average, fade_factor);
      imageStore(horizontal_img, texel, causal_output);
      imageFence(horizontal_img);
    }

    /* Non Causal Pass:
     * Sequentially apply a non causal filter running from right to left by mixing the value of the
     * pixel in the row with the average value of the previous output and next input in the same
     * row. */
    for (int x = width - 1; x >= 0; x--) {
      int2 texel = int2(x, gl_GlobalInvocationID.x);
      float4 previous_output = imageLoad(horizontal_img, texel + int2(i, 0));
      float4 current_input = imageLoad(horizontal_img, texel);
      float4 next_input = imageLoad(horizontal_img, texel - int2(i, 0));

      float4 neighbor_average = (previous_output + next_input) / 2.0f;
      float4 non_causal_output = mix(current_input, neighbor_average, fade_factor);
      imageStore(horizontal_img, texel, non_causal_output);
      imageFence(horizontal_img);
    }
  }
}
