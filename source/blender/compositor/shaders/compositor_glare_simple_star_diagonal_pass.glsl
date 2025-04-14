/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_image_diagonals.glsl"

void main()
{
  int2 size = imageSize(diagonal_img);
  int index = int(gl_GlobalInvocationID.x);
  int diagonal_length = compute_diagonal_length(size, index);
  int2 start = compute_diagonal_start(size, index);
  int2 direction = get_diagonal_direction();
  int2 end = start + (diagonal_length - 1) * direction;

  /* For each iteration, apply a causal filter followed by a non causal filters along the diagonal
   * mapped to the current thread invocation. */
  for (int i = 0; i < iterations; i++) {
    /* Causal Pass:
     * Sequentially apply a causal filter running from the start of the diagonal to its end by
     * mixing the value of the pixel in the diagonal with the average value of the previous output
     * and next input in the same diagonal. */
    for (int j = 0; j < diagonal_length; j++) {
      int2 texel = start + j * direction;
      float4 previous_output = imageLoad(diagonal_img, texel - i * direction);
      float4 current_input = imageLoad(diagonal_img, texel);
      float4 next_input = imageLoad(diagonal_img, texel + i * direction);

      float4 neighbor_average = (previous_output + next_input) / 2.0f;
      float4 causal_output = mix(current_input, neighbor_average, fade_factor);
      imageStore(diagonal_img, texel, causal_output);
      imageFence(diagonal_img);
    }

    /* Non Causal Pass:
     * Sequentially apply a non causal filter running from the end of the diagonal to its start by
     * mixing the value of the pixel in the diagonal with the average value of the previous output
     * and next input in the same diagonal. */
    for (int j = 0; j < diagonal_length; j++) {
      int2 texel = end - j * direction;
      float4 previous_output = imageLoad(diagonal_img, texel + i * direction);
      float4 current_input = imageLoad(diagonal_img, texel);
      float4 next_input = imageLoad(diagonal_img, texel - i * direction);

      float4 neighbor_average = (previous_output + next_input) / 2.0f;
      float4 non_causal_output = mix(current_input, neighbor_average, fade_factor);
      imageStore(diagonal_img, texel, non_causal_output);
      imageFence(diagonal_img);
    }
  }
}
