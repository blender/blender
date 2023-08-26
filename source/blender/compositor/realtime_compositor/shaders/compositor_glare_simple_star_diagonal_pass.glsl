/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_image_diagonals.glsl)

void main()
{
  ivec2 size = imageSize(diagonal_img);
  int index = int(gl_GlobalInvocationID.x);
  int diagonal_length = compute_diagonal_length(size, index);
  ivec2 start = compute_diagonal_start(size, index);
  ivec2 direction = get_diagonal_direction();
  ivec2 end = start + (diagonal_length - 1) * direction;

  /* For each iteration, apply a causal filter followed by a non causal filters along the diagonal
   * mapped to the current thread invocation. */
  for (int i = 0; i < iterations; i++) {
    /* Causal Pass:
     * Sequentially apply a causal filter running from the start of the diagonal to its end by
     * mixing the value of the pixel in the diagonal with the average value of the previous output
     * and next input in the same diagonal. */
    for (int j = 0; j < diagonal_length; j++) {
      ivec2 texel = start + j * direction;
      vec4 previous_output = imageLoad(diagonal_img, texel - i * direction);
      vec4 current_input = imageLoad(diagonal_img, texel);
      vec4 next_input = imageLoad(diagonal_img, texel + i * direction);

      vec4 neighbour_average = (previous_output + next_input) / 2.0;
      vec4 causal_output = mix(current_input, neighbour_average, fade_factor);
      imageStore(diagonal_img, texel, causal_output);
    }

    /* Non Causal Pass:
     * Sequentially apply a non causal filter running from the end of the diagonal to its start by
     * mixing the value of the pixel in the diagonal with the average value of the previous output
     * and next input in the same diagonal. */
    for (int j = 0; j < diagonal_length; j++) {
      ivec2 texel = end - j * direction;
      vec4 previous_output = imageLoad(diagonal_img, texel + i * direction);
      vec4 current_input = imageLoad(diagonal_img, texel);
      vec4 next_input = imageLoad(diagonal_img, texel - i * direction);

      vec4 neighbour_average = (previous_output + next_input) / 2.0;
      vec4 non_causal_output = mix(current_input, neighbour_average, fade_factor);
      imageStore(diagonal_img, texel, non_causal_output);
    }
  }
}
