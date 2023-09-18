/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

/* An intermediate shared memory where the result of X accumulation will be stored. */
shared vec4 block[gl_WorkGroupSize.x][gl_WorkGroupSize.y];

/* See the compute_incomplete_prologues function for a description of this shader. */
void main()
{
  /* Accumulate the block along the horizontal direction writing each accumulation step to the
   * intermediate shared memory block, and writing the final accumulated value to the suitable
   * prologue. */
  if (gl_LocalInvocationID.x == 0) {
    vec4 x_accumulated_color = vec4(0.0);
    for (int i = 0; i < gl_WorkGroupSize.x; i++) {
      ivec2 texel = ivec2(gl_WorkGroupID.x * gl_WorkGroupSize.x + i, gl_GlobalInvocationID.y);
      x_accumulated_color += OPERATION(texture_load(input_tx, texel, vec4(0.0)));
      block[i][gl_LocalInvocationID.y] = x_accumulated_color;
    }

    /* Note that the first column of prologues is the result of accumulating a virtual block that
     * is before the first column of blocks and we assume that this block is all zeros, so we set
     * the prologue to zero as well. This is implemented by writing starting from the second column
     * and writing zero to the first column, hence the plus one in the write_texel. */
    ivec2 write_texel = ivec2(gl_GlobalInvocationID.y, gl_WorkGroupID.x + 1);
    imageStore(incomplete_x_prologues_img, write_texel, x_accumulated_color);
    if (gl_WorkGroupID.x == 0) {
      imageStore(incomplete_x_prologues_img, ivec2(write_texel.x, 0), vec4(0.0));
    }
  }

  /* Make sure the result of X accumulation is completely done. */
  barrier();

  /* Accumulate the block along the vertical direction writing the final accumulated value to the
   * suitable prologue. */
  if (gl_LocalInvocationID.y == 0) {
    vec4 y_accumulated_color = vec4(0.0);
    for (int i = 0; i < gl_WorkGroupSize.y; i++) {
      y_accumulated_color += block[gl_LocalInvocationID.x][i];
    }

    /* Note that the first row of prologues is the result of accumulating a virtual block that is
     * before the first row of blocks and we assume that this block is all zeros, so we set the
     * prologue to zero as well. This is implemented by writing starting from the second row and
     * writing zero to the first row, hence the plus one in the write_texel. */
    ivec2 write_texel = ivec2(gl_GlobalInvocationID.x, gl_WorkGroupID.y + 1);
    imageStore(incomplete_y_prologues_img, write_texel, y_accumulated_color);
    if (gl_WorkGroupID.y == 0) {
      imageStore(incomplete_y_prologues_img, ivec2(write_texel.x, 0), vec4(0.0));
    }
  }
}
