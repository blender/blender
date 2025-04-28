/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The in-paint operation uses a jump flood algorithm to flood the region to be in-painted with the
 * pixels at its boundary. The algorithms expects an input image whose values are those returned by
 * the initialize_jump_flooding_value function, given the texel location and a boolean specifying
 * if the pixel is a boundary one.
 *
 * Technically, we needn't restrict the output to just the boundary pixels, since the algorithm can
 * still operate if the interior of the region was also included. However, the algorithm operates
 * more accurately when the number of pixels to be flooded is minimum. */

#include "gpu_shader_compositor_jump_flooding_lib.glsl"
#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  /* Identify if any of the 8 neighbors around the center pixel are transparent. */
  bool has_transparent_neighbors = false;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      int2 offset = int2(i, j);

      /* Exempt the center pixel. */
      if (all(notEqual(offset, int2(0)))) {
        if (texture_load(input_tx, texel + offset).a < 1.0f) {
          has_transparent_neighbors = true;
          break;
        }
      }
    }
  }

  /* The pixels at the boundary are those that are opaque and have transparent neighbors. */
  bool is_opaque = texture_load(input_tx, texel).a == 1.0f;
  bool is_boundary_pixel = is_opaque && has_transparent_neighbors;

  /* Encode the boundary information in the format expected by the jump flooding algorithm. */
  int2 jump_flooding_value = initialize_jump_flooding_value(texel, is_boundary_pixel);

  imageStore(boundary_img, texel, int4(jump_flooding_value, int2(0)));
}
