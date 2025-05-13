/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

/* A shared table that stores the UV coordinates of all pixels in the work group. This is necessary
 * to avoid recomputing UV coordinates when computing the gradients necessary for anisotropic
 * filtering, see the implementation for more information. */
shared float2 uv_coordinates_table[gl_WorkGroupSize.x][gl_WorkGroupSize.y];

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  float2 uv_coordinates = texture_load(uv_tx, texel).xy;
  float2 uv_size = float2(texture_size(uv_tx));

  /* Store the UV coordinates into the shared table and issue a barrier to later compute the
   * gradients from the table. */
  int2 table_index = int2(gl_LocalInvocationID.xy);
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
   * subtraction.
   *
   * Divide by the input size since textureGrad assumes derivatives with respect to texel
   * coordinates. */
  int x_step = (table_index.x % 2) * -2 + 1;
  float2 x_neighbor = uv_coordinates_table[table_index.x + x_step][table_index.y];
  float2 x_gradient = (x_neighbor - uv_coordinates) * x_step / uv_size.x;

  /* Compute the partial derivative of the UV coordinates along the y direction using a
   * finite difference approximation. See the previous code section for more information. */
  int y_step = (table_index.y % 2) * -2 + 1;
  float2 y_neighbor = uv_coordinates_table[table_index.x][table_index.y + y_step];
  float2 y_gradient = (y_neighbor - uv_coordinates) * y_step / uv_size.y;

  /* Sample the input using the UV coordinates passing in the computed gradients in order to
   * utilize the anisotropic filtering capabilities of the sampler. */
  float4 sampled_color = textureGrad(input_tx, uv_coordinates, x_gradient, y_gradient);

  /* The UV texture is assumed to contain an alpha channel as its third channel, since the UV
   * coordinates might be defined in only a subset area of the UV texture as mentioned. In that
   * case, the alpha is typically opaque at the subset area and transparent everywhere else, and
   * alpha pre-multiplication is then performed. This format of having an alpha channel in the UV
   * coordinates is the format used by UV passes in render engines, hence the mentioned logic. */
  float alpha = texture_load(uv_tx, texel).z;

  float4 result = sampled_color * alpha;

  imageStore(output_img, texel, result);
}
