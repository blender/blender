/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Computes a linear gradient from the outer mask boundary to the inner mask boundary, starting
 * from 0 and ending at 1. This is computed using the equation:
 *
 *   Gradient = O / (O + I)
 *
 * Where O is the distance to the outer boundary and I is the distance to the inner boundary.
 * This can be viewed as computing the ratio between the distance to the outer boundary to the
 * distance between the outer and inner boundaries as can be seen in the following illustration
 * where the $ sign designates a pixel between both boundaries.
 *
 *                   |    O         I    |
 *   Outer Boundary  |---------$---------|  Inner Boundary
 *                   |                   |
 */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_jump_flooding_lib.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* Pixels inside the inner mask are always 1.0. */
  float inner_mask = texture_load(inner_mask_tx, texel).x;
  if (inner_mask != 0.0) {
    imageStore(output_img, texel, vec4(1.0));
    return;
  }

  /* Pixels outside the outer mask are always 0.0. */
  float outer_mask = texture_load(outer_mask_tx, texel).x;
  if (outer_mask == 0.0) {
    imageStore(output_img, texel, vec4(0.0));
    return;
  }

  /* Extract the distances to the inner and outer boundaries from the jump flooding tables. */
  vec4 inner_flooding_value = texture_load(flooded_inner_boundary_tx, texel);
  vec4 outer_flooding_value = texture_load(flooded_outer_boundary_tx, texel);
  float distance_to_inner = extract_jump_flooding_distance_to_closest_seed(inner_flooding_value);
  float distance_to_outer = extract_jump_flooding_distance_to_closest_seed(outer_flooding_value);

  float gradient = distance_to_outer / (distance_to_outer + distance_to_inner);

  imageStore(output_img, texel, vec4(gradient));
}
