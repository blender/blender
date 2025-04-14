/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

/* An intermediate shared memory where the result of X accumulation will be stored. */
shared float4 block[gl_WorkGroupSize.x][gl_WorkGroupSize.y];

void main()
{
  /* Accumulate the block along the horizontal direction starting from the X prologue value,
   * writing each accumulation step to the intermediate shared memory. */
  if (gl_LocalInvocationID.x == 0) {
    int2 x_prologue_texel = int2(gl_GlobalInvocationID.y, gl_WorkGroupID.x);
    float4 x_accumulated_color = texture_load(
        complete_x_prologues_tx, x_prologue_texel, float4(0.0f));
    for (uint i = 0; i < gl_WorkGroupSize.x; i++) {
      int2 texel = int2(gl_WorkGroupID.x * gl_WorkGroupSize.x + i, gl_GlobalInvocationID.y);
      x_accumulated_color += OPERATION(texture_load(input_tx, texel, float4(0.0f)));
      block[i][gl_LocalInvocationID.y] = x_accumulated_color;
    }
  }

  /* Make sure the result of X accumulation is completely done. */
  barrier();

  /* Accumulate the block along the vertical direction starting from the Y prologue value,
   * writing each accumulation step to the output image. */
  if (gl_LocalInvocationID.y == 0) {
    int2 y_prologue_texel = int2(gl_GlobalInvocationID.x, gl_WorkGroupID.y);
    float4 y_accumulated_color = texture_load(
        complete_y_prologues_tx, y_prologue_texel, float4(0.0f));
    for (uint i = 0; i < gl_WorkGroupSize.y; i++) {
      y_accumulated_color += block[gl_LocalInvocationID.x][i];
      int2 texel = int2(gl_GlobalInvocationID.x, gl_WorkGroupID.y * gl_WorkGroupSize.y + i);
      imageStore(output_img, texel, y_accumulated_color);
    }
  }
}
