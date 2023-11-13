/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Computes the structure tensor of the image using a Dirac delta window function as described in
 * section "3.2 Local Structure Estimation" of the paper:
 *
 *   Kyprianidis, Jan Eric. "Image and video abstraction by multi-scale anisotropic Kuwahara
 *   filtering." 2011.
 *
 * The structure tensor should then be smoothed using a Gaussian function to eliminate high
 * frequency details. */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* The weight kernels of the filter optimized for rotational symmetry described in section "3.2.1
   * Gradient Calculation". */
  const float corner_weight = 0.182;
  const float center_weight = 1.0 - 2.0 * corner_weight;

  vec3 x_partial_derivative = texture_load(input_tx, texel + ivec2(-1, 1)).rgb * -corner_weight +
                              texture_load(input_tx, texel + ivec2(-1, 0)).rgb * -center_weight +
                              texture_load(input_tx, texel + ivec2(-1, -1)).rgb * -corner_weight +
                              texture_load(input_tx, texel + ivec2(1, 1)).rgb * corner_weight +
                              texture_load(input_tx, texel + ivec2(1, 0)).rgb * center_weight +
                              texture_load(input_tx, texel + ivec2(1, -1)).rgb * corner_weight;

  vec3 y_partial_derivative = texture_load(input_tx, texel + ivec2(-1, 1)).rgb * corner_weight +
                              texture_load(input_tx, texel + ivec2(0, 1)).rgb * center_weight +
                              texture_load(input_tx, texel + ivec2(1, 1)).rgb * corner_weight +
                              texture_load(input_tx, texel + ivec2(-1, -1)).rgb * -corner_weight +
                              texture_load(input_tx, texel + ivec2(0, -1)).rgb * -center_weight +
                              texture_load(input_tx, texel + ivec2(1, -1)).rgb * -corner_weight;

  float dxdx = dot(x_partial_derivative, x_partial_derivative);
  float dxdy = dot(x_partial_derivative, y_partial_derivative);
  float dydy = dot(y_partial_derivative, y_partial_derivative);

  /* We encode the structure tensor in a vec4 using a column major storage order. */
  vec4 structure_tensor = vec4(dxdx, dxdy, dxdy, dydy);

  imageStore(structure_tensor_img, texel, structure_tensor);
}
