/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_image_diagonals.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 size = imageSize(anti_diagonal_img);
  int index = int(gl_GlobalInvocationID.x);
  int anti_diagonal_length = compute_anti_diagonal_length(size, index);
  ivec2 start = compute_anti_diagonal_start(size, index);
  ivec2 direction = get_anti_diagonal_direction();
  ivec2 end = start + (anti_diagonal_length - 1) * direction;

  /* For each iteration, apply a causal filter followed by a non causal filters along the anti
   * diagonal mapped to the current thread invocation. */
  for (int i = 0; i < iterations; i++) {
    /* Causal Pass:
     * Sequentially apply a causal filter running from the start of the anti diagonal to its end by
     * mixing the value of the pixel in the anti diagonal with the average value of the previous
     * output and next input in the same anti diagonal. */
    for (int j = 0; j < anti_diagonal_length; j++) {
      ivec2 texel = start + j * direction;
      vec4 previous_output = imageLoad(anti_diagonal_img, texel - i * direction);
      vec4 current_input = imageLoad(anti_diagonal_img, texel);
      vec4 next_input = imageLoad(anti_diagonal_img, texel + i * direction);

      vec4 neighbour_average = (previous_output + next_input) / 2.0;
      vec4 causal_output = mix(current_input, neighbour_average, fade_factor);
      imageStore(anti_diagonal_img, texel, causal_output);
    }

    /* Non Causal Pass:
     * Sequentially apply a non causal filter running from the end of the diagonal to its start by
     * mixing the value of the pixel in the diagonal with the average value of the previous output
     * and next input in the same diagonal. */
    for (int j = 0; j < anti_diagonal_length; j++) {
      ivec2 texel = end - j * direction;
      vec4 previous_output = imageLoad(anti_diagonal_img, texel + i * direction);
      vec4 current_input = imageLoad(anti_diagonal_img, texel);
      vec4 next_input = imageLoad(anti_diagonal_img, texel - i * direction);

      vec4 neighbour_average = (previous_output + next_input) / 2.0;
      vec4 non_causal_output = mix(current_input, neighbour_average, fade_factor);
      imageStore(anti_diagonal_img, texel, non_causal_output);
    }
  }

  /* For each pixel in the anti diagonal mapped to the current invocation thread, add the result of
   * the diagonal pass to the vertical pass. */
  for (int j = 0; j < anti_diagonal_length; j++) {
    ivec2 texel = start + j * direction;
    vec4 horizontal = texture_load(diagonal_tx, texel);
    vec4 vertical = imageLoad(anti_diagonal_img, texel);
    imageStore(anti_diagonal_img, texel, horizontal + vertical);
  }
}
