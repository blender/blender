/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  vec2 input_size = vec2(texture_size(input_tx));

  vec2 coordinates = vec2(texel) + vec2(0.5);

  float current_sin = 0.0;
  float current_cos = 1.0;
  float current_scale = 1.0;
  vec2 current_translation = vec2(0.0);

  /* For each iteration, accumulate the input at the transformed coordinates, then increment the
   * transformations for the next iteration. */
  vec4 accumulated_color = vec4(0.0);
  for (int i = 0; i < iterations; i++) {
    /* Transform the coordinates by first offsetting the origin, scaling, translating, rotating,
     * then finally restoring the origin. Notice that we do the inverse of each of the transforms,
     * since we are transforming the coordinates, not the image. */
    vec2 transformed_coordinates = coordinates;
    transformed_coordinates -= origin;
    transformed_coordinates /= current_scale;
    transformed_coordinates -= current_translation;
    transformed_coordinates *= mat2(current_cos, current_sin, -current_sin, current_cos);
    transformed_coordinates += origin;

    accumulated_color += texture(input_tx, transformed_coordinates / input_size);

    current_scale += scale;
    current_translation += translation;

    /* Those are the sine and cosine addition identities. Used to avoid computing sine and cosine
     * at each iteration. */
    float new_sin = current_sin * rotation_cos + current_cos * rotation_sin;
    current_cos = current_cos * rotation_cos - current_sin * rotation_sin;
    current_sin = new_sin;
  }

  imageStore(output_img, texel, accumulated_color / iterations);
}
