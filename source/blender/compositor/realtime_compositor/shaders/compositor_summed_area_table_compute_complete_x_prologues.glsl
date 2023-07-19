#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

/* A shared memory to sum the prologues using parallel reduction. See the parallel reduction shader
 * "compositor_parallel_reduction.glsl" for more information. */
shared vec4 complete_prologue[gl_WorkGroupSize.x];

/* See the compute_complete_x_prologues function for a description of this shader. */
void main()
{
  /* Note that the X prologues are stored transposed, hence the horizontal dispatch domain, even
   * though, conceptually, the dispatch domain covers the vertical axis of the image. */
  int x = int(gl_GlobalInvocationID.x);

  vec4 accumulated_color = vec4(0.0);
  for (int y = 0; y < texture_size(incomplete_x_prologues_tx).y; y++) {
    accumulated_color += texture_load(incomplete_x_prologues_tx, ivec2(x, y), vec4(0.0));
    imageStore(complete_x_prologues_img, ivec2(x, y), accumulated_color);

    if (gl_WorkGroupID.x == 0) {
      /* Note that the first row of sums is the result of summing the prologues of a virtual block
       * that is before the first row of blocks and we assume that those prologues are all zeros,
       * so we set the sum to zero in that case. This is implemented by setting the sums of the
       * first vertical workgroup to zero, white latter workgroups are summed as as usual and
       * stored starting from the second row. */
      imageStore(complete_x_prologues_sum_img, ivec2(y, 0), vec4(0.0));
    }

    /* A parallel reduction loop to sum the prologues. This is exactly the same as the parallel
     * reduction loop in the shader "compositor_parallel_reduction.glsl", see that shader for
     * more information. */
    complete_prologue[gl_LocalInvocationIndex] = accumulated_color;
    for (uint stride = gl_WorkGroupSize.x / 2; stride > 0; stride /= 2) {
      barrier();

      if (gl_LocalInvocationIndex >= stride) {
        continue;
      }

      complete_prologue[gl_LocalInvocationIndex] =
          complete_prologue[gl_LocalInvocationIndex] +
          complete_prologue[gl_LocalInvocationIndex + stride];
    }

    barrier();
    if (gl_LocalInvocationIndex == 0) {
      /*  Note that we store using a transposed texel, but that is only to undo the transposition
       * mentioned above. Also note that we start from the second row because the first row is
       * set to zero as mentioned above. */
      vec4 sum = complete_prologue[0];
      imageStore(complete_x_prologues_sum_img, ivec2(y, gl_WorkGroupID.x + 1), sum);
    }
  }
}
