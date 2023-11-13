/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 input_size = texture_size(input_tx);

  /* Add 0.5 to evaluate the input sampler at the center of the pixel. */
  vec2 coordinates = vec2(texel) + vec2(0.5);

  /* For each iteration, accumulate the input at the normalize coordinates, hence the divide by
   * input size, then transform the coordinates for the next iteration. */
  vec4 accumulated_color = vec4(0.0);
  for (int i = 0; i < iterations; i++) {
    accumulated_color += texture(input_tx, coordinates / vec2(input_size));
    coordinates = (mat3(inverse_transformation) * vec3(coordinates, 1.0)).xy;
  }

  /* Write the accumulated color divided by the number of iterations. */
  imageStore(output_img, texel, accumulated_color / iterations);
}
