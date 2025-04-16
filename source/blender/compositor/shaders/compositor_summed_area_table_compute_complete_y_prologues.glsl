/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

/* See the compute_complete_y_prologues function for a description of this shader. */
void main()
{
  int x = int(gl_GlobalInvocationID.x);
  int num_rows = texture_size(incomplete_y_prologues_tx).y;

  float4 accumulated_color = float4(0.0f);
  for (int y = 0; y < num_rows; y++) {
    accumulated_color += texture_load(incomplete_y_prologues_tx, int2(x, y));
    accumulated_color += texture_load(complete_x_prologues_sum_tx, int2(gl_WorkGroupID.x, y));
    imageStore(complete_y_prologues_img, int2(x, y), accumulated_color);
  }
}
