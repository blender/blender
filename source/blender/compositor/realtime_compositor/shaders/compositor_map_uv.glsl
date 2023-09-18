/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

/* A shared table that stores the UV coordinates of all pixels in the work group. This is necessary
 * to avoid recomputing UV coordinates when computing the gradients necessary for anisotropic
 * filtering, see the implementation for more information. */
shared vec2 uv_coordinates_table[gl_WorkGroupSize.x][gl_WorkGroupSize.y];

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  vec2 uv_coordinates = texture_load(uv_tx, texel).xy;

  /* Store the UV coordinates into the shared table and issue a barrier to later compute the
   * gradients from the table. */
  ivec2 table_index = ivec2(gl_LocalInvocationID.xy);
  uv_coordinates_table[table_index.x][table_index.y] = uv_coordinates;
  barrier();

  /* Compute the partial derivative of the UV coordinates along the x direction using a finite
   * difference approximation. Odd invocations use a forward finite difference equation while even
   * invocations use a backward finite difference equation. This is done such that invocations at
   * the edges of the work group wouldn't need access to pixels that are outside of the work group.
   *
   * The x_step value is 1 for even invocations and when added to the x table index and multiplied
   * by the result yields a standard forward finite difference equation. The x_step value is -1 for
   * odd invocations and when added to the x table index and multiplied by the result yields a
   * standard backward finite difference equation, because multiplication by -1 flips the order of
   * subtraction. */
  int x_step = (table_index.x % 2) * -2 + 1;
  vec2 x_neighbour = uv_coordinates_table[table_index.x + x_step][table_index.y];
  vec2 x_gradient = (x_neighbour - uv_coordinates) * x_step;

  /* Compute the partial derivative of the UV coordinates along the y direction using a
   * finite difference approximation. See the previous code section for more information. */
  int y_step = (table_index.y % 2) * -2 + 1;
  vec2 y_neighbour = uv_coordinates_table[table_index.x][table_index.y + y_step];
  vec2 y_gradient = (y_neighbour - uv_coordinates) * y_step;

  /* Sample the input using the UV coordinates passing in the computed gradients in order to
   * utilize the anisotropic filtering capabilities of the sampler. */
  vec4 sampled_color = textureGrad(input_tx, uv_coordinates, x_gradient, y_gradient);

  /* The UV coordinates might be defined in only a subset area of the UV textures, in which case,
   * the gradients would be infinite at the boundary of that area, which would produce erroneous
   * results due to anisotropic filtering. To workaround this, we attenuate the result if its
   * computed gradients are too high such that the result tends to zero when the magnitude of the
   * gradients tends to one, that is when their sum tends to 2. One is chosen as the threshold
   * because that's the maximum gradient magnitude when the boundary is the maximum sampler value
   * of one and the out of bound values are zero. Additionally, the user supplied gradient
   * attenuation factor can be used to control this attenuation or even disable it when it is zero,
   * ranging between zero and one. */
  float gradient_magnitude = (length(x_gradient) + length(y_gradient)) / 2.0;
  float gradient_attenuation = max(0.0, 1.0 - gradient_attenuation_factor * gradient_magnitude);

  /* The UV texture is assumed to contain an alpha channel as its third channel, since the UV
   * coordinates might be defined in only a subset area of the UV texture as mentioned. In that
   * case, the alpha is typically opaque at the subset area and transparent everywhere else, and
   * alpha pre-multiplication is then performed. This format of having an alpha channel in the UV
   * coordinates is the format used by UV passes in render engines, hence the mentioned logic. */
  float alpha = texture_load(uv_tx, texel).z;

  vec4 result = sampled_color * gradient_attenuation * alpha;

  imageStore(output_img, texel, result);
}
