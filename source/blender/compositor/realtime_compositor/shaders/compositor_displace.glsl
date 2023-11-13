/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

/* A shared table that stores the displaced coordinates of all pixels in the work group. This is
 * necessary to avoid recomputing displaced coordinates when computing the gradients necessary for
 * anisotropic filtering, see the implementation for more information. */
shared vec2 displaced_coordinates_table[gl_WorkGroupSize.x][gl_WorkGroupSize.y];

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 input_size = texture_size(input_tx);

  /* Add 0.5 to evaluate the input sampler at the center of the pixel and divide by the image size
   * to get the coordinates into the sampler's expected [0, 1] range. */
  vec2 coordinates = (vec2(texel) + vec2(0.5)) / vec2(input_size);

  /* Note that the input displacement is in pixel space, so divide by the input size to transform
   * it into the normalized sampler space. */
  vec2 scale = vec2(texture_load(x_scale_tx, texel).x, texture_load(y_scale_tx, texel).x);
  vec2 displacement = texture_load(displacement_tx, texel).xy * scale / vec2(input_size);
  vec2 displaced_coordinates = coordinates - displacement;

  /* Store the displaced coordinates into the shared table and issue a barrier to later compute the
   * gradients from the table. */
  ivec2 table_index = ivec2(gl_LocalInvocationID.xy);
  displaced_coordinates_table[table_index.x][table_index.y] = displaced_coordinates;
  barrier();

  /* Compute the partial derivative of the displaced coordinates along the x direction using a
   * finite difference approximation. Odd invocations use a forward finite difference equation
   * while even invocations use a backward finite difference equation. This is done such that
   * invocations at the edges of the work group wouldn't need access to pixels that are outside of
   * the work group.
   *
   * The x_step value is 1 for even invocations and when added to the x table index and multiplied
   * by the result yields a standard forward finite difference equation. The x_step value is -1 for
   * odd invocations and when added to the x table index and multiplied by the result yields a
   * standard backward finite difference equation, because multiplication by -1 flips the order of
   * subtraction. */
  int x_step = (table_index.x % 2) * -2 + 1;
  vec2 x_neighbour = displaced_coordinates_table[table_index.x + x_step][table_index.y];
  vec2 x_gradient = (x_neighbour - displaced_coordinates) * x_step;

  /* Compute the partial derivative of the displaced coordinates along the y direction using a
   * finite difference approximation. See the previous code section for more information. */
  int y_step = (table_index.y % 2) * -2 + 1;
  vec2 y_neighbour = displaced_coordinates_table[table_index.x][table_index.y + y_step];
  vec2 y_gradient = (y_neighbour - displaced_coordinates) * y_step;

  /* Sample the input using the displaced coordinates passing in the computed gradients in order to
   * utilize the anisotropic filtering capabilities of the sampler. */
  vec4 displaced_color = textureGrad(input_tx, displaced_coordinates, x_gradient, y_gradient);

  imageStore(output_img, texel, displaced_color);
}
