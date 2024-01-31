/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The Double Edge Mask operation uses a jump flood algorithm to compute a distance transform to
 * the boundary of the inner and outer masks. The algorithm expects an input image whose values are
 * those returned by the initialize_jump_flooding_value function, given the texel location and a
 * boolean specifying if the pixel is a boundary one.
 *
 * Technically, we needn't restrict the output to just the boundary pixels, since the algorithm can
 * still operate if the interior of the masks was also included. However, the algorithm operates
 * more accurately when the number of pixels to be flooded is minimum. */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_jump_flooding_lib.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* Identify if any of the 8 neighbors around the center pixel are not masked. */
  bool has_inner_non_masked_neighbors = false;
  bool has_outer_non_masked_neighbors = false;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      ivec2 offset = ivec2(i, j);

      /* Exempt the center pixel. */
      if (all(equal(offset, ivec2(0)))) {
        continue;
      }

      if (texture_load(inner_mask_tx, texel + offset).x == 0.0) {
        has_inner_non_masked_neighbors = true;
      }

      /* If the user specified include_edges_of_image to be true, then we assume the outer mask is
       * bounded by the image boundary, otherwise, we assume the outer mask is open-ended. This is
       * practically implemented by falling back to 0.0 or 1.0 for out of bound pixels. */
      vec4 boundary_fallback = include_edges_of_image ? vec4(0.0) : vec4(1.0);
      if (texture_load(outer_mask_tx, texel + offset, boundary_fallback).x == 0.0) {
        has_outer_non_masked_neighbors = true;
      }

      /* Both are true, no need to continue. */
      if (has_inner_non_masked_neighbors && has_outer_non_masked_neighbors) {
        break;
      }
    }
  }

  bool is_inner_masked = texture_load(inner_mask_tx, texel).x > 0.0;
  bool is_outer_masked = texture_load(outer_mask_tx, texel).x > 0.0;

  /* The pixels at the boundary are those that are masked and have non masked neighbors. The inner
   * boundary has a specialization, if include_all_inner_edges is false, only inner boundaries that
   * lie inside the outer mask will be considered a boundary. The outer boundary is only considered
   * if it is not inside the inner mask. */
  bool is_inner_boundary = is_inner_masked && has_inner_non_masked_neighbors &&
                           (is_outer_masked || include_all_inner_edges);
  bool is_outer_boundary = is_outer_masked && !is_inner_masked && has_outer_non_masked_neighbors;

  /* Encode the boundary information in the format expected by the jump flooding algorithm. */
  ivec2 inner_jump_flooding_value = initialize_jump_flooding_value(texel, is_inner_boundary);
  ivec2 outer_jump_flooding_value = initialize_jump_flooding_value(texel, is_outer_boundary);

  imageStore(inner_boundary_img, texel, ivec4(inner_jump_flooding_value, ivec2(0)));
  imageStore(outer_boundary_img, texel, ivec4(outer_jump_flooding_value, ivec2(0)));
}
