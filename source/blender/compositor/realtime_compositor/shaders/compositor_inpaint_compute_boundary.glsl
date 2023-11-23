/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The in-paint operation uses a jump flood algorithm to flood the region to be inpainted with the
 * pixels at its boundary. The algorithms expects an input image whose values are those returned by
 * the initialize_jump_flooding_value function, given the texel location and a boolean specifying
 * if the pixel is a boundary one.
 *
 * Technically, we needn't restrict the output to just the boundary pixels, since the algorithm can
 * still operate if the interior of the region was also included. However, the algorithm operates
 * more accurately when the number of pixels to be flooded is minimum. */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_jump_flooding_lib.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* Identify if any of the 8 neighbours around the center pixel are transparent. */
  bool has_transparent_neighbours = false;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      ivec2 offset = ivec2(i, j);

      /* Exempt the center pixel. */
      if (all(notEqual(offset, ivec2(0)))) {
        if (texture_load(input_tx, texel + offset).a < 1.0) {
          has_transparent_neighbours = true;
          break;
        }
      }
    }
  }

  /* The pixels at the boundary are those that are opaque and have transparent neighbours. */
  bool is_opaque = texture_load(input_tx, texel).a == 1.0;
  bool is_boundary_pixel = is_opaque && has_transparent_neighbours;

  /* Encode the boundary information in the format expected by the jump flooding algorithm. */
  ivec2 jump_flooding_value = initialize_jump_flooding_value(texel, is_boundary_pixel);

  imageStore(boundary_img, texel, ivec4(jump_flooding_value, ivec2(0)));
}
