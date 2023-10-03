/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* This shader implements a single pass of the Jump Flooding algorithm described in sections 3.1
 * and 3.2 of the paper:
 *
 *   Rong, Guodong, and Tiow-Seng Tan. "Jump flooding in GPU with applications to Voronoi diagram
 *   and distance transform." Proceedings of the 2006 symposium on Interactive 3D graphics and
 *   games. 2006.
 *
 * The shader is a straightforward implementation of the aforementioned sections of the paper,
 * noting that the nil special value in the paper is equivalent to JUMP_FLOODING_NON_FLOODED_VALUE.
 *
 * The `gpu_shader_compositor_jump_flooding_lib.glsl` library contains the necessary utility
 * functions to initialize, encode, and extract the information in the jump flooding values. */

#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_jump_flooding_lib.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* For each of the previously flooded pixels in the 3x3 window of the given step size around the
   * center pixel, find the position of the closest seed pixel that is closest to the current
   * center pixel. */
  vec2 closest_seed_position = vec2(0.0);
  float minimum_squared_distance = FLT_MAX;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      ivec2 offset = ivec2(i, j) * step_size;

      /* Use #JUMP_FLOODING_NON_FLOODED_VALUE as a fallback value to exempt out of bound pixels
       * from the loop as can be seen in the following continue condition. */
      vec4 value = texture_load(input_tx, texel + offset, JUMP_FLOODING_NON_FLOODED_VALUE);

      /* The pixel is either not flooded yet or is out of bound, so skip it. */
      if (!is_jump_flooded(value)) {
        continue;
      }

      /* Extract the position of the closest seed pixel to this neighboring pixel and compute the
       * squared distance from that position to the center pixel. */
      ivec2 position = extract_jump_flooding_closest_seed_texel(value);
      float squared_distance = distance_squared(vec2(position), vec2(texel));

      if (squared_distance < minimum_squared_distance) {
        minimum_squared_distance = squared_distance;
        closest_seed_position = vec2(position);
      }
    }
  }

  /* If the minimum squared distance is still #FLT_MAX, that means the loop never got past the
   * continue condition and thus no flooding happened. If flooding happened, we write the closest
   * seed position as well as the distance to it. */
  bool flooding_happened = minimum_squared_distance != FLT_MAX;
  float minimum_distance = sqrt(minimum_squared_distance);
  vec4 jump_flooding_value = encode_jump_flooding_value(
      closest_seed_position, minimum_distance, flooding_happened);

  imageStore(output_img, texel, jump_flooding_value);
}
