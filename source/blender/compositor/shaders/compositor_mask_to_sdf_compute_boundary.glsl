/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The mask to SDF operation uses a jump flood algorithm to flood the region to be distance
 * transformed with the pixels at its boundary. The algorithms expects an input image whose values
 * are those returned by the initialize_jump_flooding_value function, given the texel location and
 * a boolean specifying if the pixel is a boundary one.
 *
 * Technically, we needn't restrict the output to just the boundary pixels, since the algorithm can
 * still operate if the interior of the region was also included. However, the algorithm operates
 * more accurately when the number of pixels to be flooded is minimum. */

#include "infos/compositor_mask_to_sdf_infos.hh"

COMPUTE_SHADER_CREATE_INFO(compositor_mask_to_sdf_compute_boundary)

#include "gpu_shader_compositor_jump_flooding_lib.glsl"
#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  const int2 texel = int2(gl_GlobalInvocationID.xy);

  /* Identify if any of the 8 neighbors around the center pixel are unmasked. */
  bool has_unmasked_neighbors = false;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      const int2 offset = int2(i, j);

      /* Exempt the center pixel. */
      if (all(equal(offset, int2(0)))) {
        continue;
      }

      if (!bool(texture_load(mask_tx, texel + offset).x)) {
        has_unmasked_neighbors = true;
        break;
      }
    }
  }

  /* The pixels at the boundary are those that are masked and have unmasked neighbors. */
  const bool is_masked = bool(texture_load(mask_tx, texel).x);
  const bool is_boundary_pixel = is_masked && has_unmasked_neighbors;

  /* Encode the boundary information in the format expected by the jump flooding algorithm. */
  const int2 jump_flooding_value = initialize_jump_flooding_value(texel, is_boundary_pixel);

  imageStore(boundary_img, texel, int4(jump_flooding_value, int2(0)));
}
