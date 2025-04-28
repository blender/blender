/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_image_diagonals.glsl"
#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 size = imageSize(anti_diagonal_img);
  int index = int(gl_GlobalInvocationID.x);
  int anti_diagonal_length = compute_anti_diagonal_length(size, index);
  int2 start = compute_anti_diagonal_start(size, index);
  int2 direction = get_anti_diagonal_direction();
  int2 end = start + (anti_diagonal_length - 1) * direction;

  /* For each iteration, apply a causal filter followed by a non causal filters along the anti
   * diagonal mapped to the current thread invocation. */
  for (int i = 0; i < iterations; i++) {
    /* Causal Pass:
     * Sequentially apply a causal filter running from the start of the anti diagonal to its end by
     * mixing the value of the pixel in the anti diagonal with the average value of the previous
     * output and next input in the same anti diagonal. */
    for (int j = 0; j < anti_diagonal_length; j++) {
      int2 texel = start + j * direction;
      float4 previous_output = imageLoad(anti_diagonal_img, texel - i * direction);
      float4 current_input = imageLoad(anti_diagonal_img, texel);
      float4 next_input = imageLoad(anti_diagonal_img, texel + i * direction);

      float4 neighbor_average = (previous_output + next_input) / 2.0f;
      float4 causal_output = mix(current_input, neighbor_average, fade_factor);
      imageStore(anti_diagonal_img, texel, causal_output);
      imageFence(anti_diagonal_img);
    }

    /* Non Causal Pass:
     * Sequentially apply a non causal filter running from the end of the diagonal to its start by
     * mixing the value of the pixel in the diagonal with the average value of the previous output
     * and next input in the same diagonal. */
    for (int j = 0; j < anti_diagonal_length; j++) {
      int2 texel = end - j * direction;
      float4 previous_output = imageLoad(anti_diagonal_img, texel + i * direction);
      float4 current_input = imageLoad(anti_diagonal_img, texel);
      float4 next_input = imageLoad(anti_diagonal_img, texel - i * direction);

      float4 neighbor_average = (previous_output + next_input) / 2.0f;
      float4 non_causal_output = mix(current_input, neighbor_average, fade_factor);
      imageStore(anti_diagonal_img, texel, non_causal_output);
      imageFence(anti_diagonal_img);
    }
  }

  /* For each pixel in the anti diagonal mapped to the current invocation thread, add the result of
   * the diagonal pass to the vertical pass. */
  for (int j = 0; j < anti_diagonal_length; j++) {
    int2 texel = start + j * direction;
    float4 horizontal = texture_load(diagonal_tx, texel);
    float4 vertical = imageLoad(anti_diagonal_img, texel);
    float4 combined = horizontal + vertical;
    imageStore(anti_diagonal_img, texel, float4(combined.rgb, 1.0f));
  }
}
