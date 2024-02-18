/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* This shader reduces the given texture into a smaller texture of a size equal to the number of
 * work groups. In particular, each work group reduces its contents into a single value and writes
 * that value to a single pixel in the output image. The shader can be dispatched multiple times to
 * eventually reduce the image into a single pixel.
 *
 * The shader works by loading the whole data of each work group into a linear array, then it
 * reduces the second half of the array onto the first half of the array, then it reduces the
 * second quarter of the array onto the first quarter or the array, and so on until only one
 * element remains. The following figure illustrates the process for sum reduction on 8 elements.
 *
 *     .---. .---. .---. .---. .---. .---. .---. .---.
 *     | 0 | | 1 | | 2 | | 3 | | 4 | | 5 | | 6 | | 7 |  Original data.
 *     '---' '---' '---' '---' '---' '---' '---' '---'
 *       |.____|_____|_____|_____|     |     |     |
 *       ||    |.____|_____|___________|     |     |
 *       ||    ||    |.____|_________________|     |
 *       ||    ||    ||    |.______________________|  <--First reduction. Stride = 4.
 *       ||    ||    ||    ||
 *     .---. .---. .---. .----.
 *     | 4 | | 6 | | 8 | | 10 |                       <--Data after first reduction.
 *     '---' '---' '---' '----'
 *       |.____|_____|     |
 *       ||    |.__________|                          <--Second reduction. Stride = 2.
 *       ||    ||
 *     .----. .----.
 *     | 12 | | 16 |                                  <--Data after second reduction.
 *     '----' '----'
 *       |.____|
 *       ||                                           <--Third reduction. Stride = 1.
 *     .----.
 *     | 28 |
 *     '----'                                         <--Data after third reduction.
 *
 *
 * The shader is generic enough to implement many types of reductions. This is done by using macros
 * that the developer should define to implement a certain reduction operation. Those include,
 * TYPE, IDENTITY, INITIALIZE, LOAD, and REDUCE. See the implementation below for more information
 * as well as the compositor_parallel_reduction_info.hh for example reductions operations. */

/* Doing the reduction in shared memory is faster, so create a shared array where the whole data
 * of the work group will be loaded and reduced. The 2D structure of the work group is irrelevant
 * for reduction, so we just load the data in a 1D array to simplify reduction. The developer is
 * expected to define the TYPE macro to be a float or a vec4, depending on the type of data being
 * reduced. */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

#define reduction_size (gl_WorkGroupSize.x * gl_WorkGroupSize.y)
shared TYPE reduction_data[reduction_size];

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* Initialize the shared array for out of bound invocations using the IDENTITY value. The
   * developer is expected to define the IDENTITY macro to be a value of type TYPE that does not
   * affect the output of the reduction. For instance, sum reductions have an identity of 0.0,
   * while max value reductions have an identity of FLT_MIN */
  if (any(lessThan(texel, ivec2(0))) || any(greaterThanEqual(texel, texture_size(input_tx)))) {
    reduction_data[gl_LocalInvocationIndex] = IDENTITY;
  }
  else {
    vec4 value = texture_load_unbound(input_tx, texel);

    /* Initialize the shared array given the previously loaded value. This step can be different
     * depending on whether this is the initial reduction pass or a latter one. Indeed, the input
     * texture for the initial reduction is the source texture itself, while the input texture to a
     * latter reduction pass is an intermediate texture after one or more reductions have happened.
     * This is significant because the data being reduced might be computed from the original data
     * and different from it, for instance, when summing the luminance of an image, the original
     * data is a vec4 color, while the reduced data is a float luminance value. So for the initial
     * reduction pass, the luminance will be computed from the color, reduced, then stored into an
     * intermediate float texture. On the other hand, for latter reduction passes, the luminance
     * will be loaded directly and reduced without extra processing. So the developer is expected
     * to define the INITIALIZE and LOAD macros to be expressions that derive the needed value from
     * the loaded value for the initial reduction pass and latter ones respectively. */
    reduction_data[gl_LocalInvocationIndex] = is_initial_reduction ? INITIALIZE(value) :
                                                                     LOAD(value);
  }

  /* Reduce the reduction data by half on every iteration until only one element remains. See the
   * above figure for an intuitive understanding of the stride value. */
  for (uint stride = reduction_size / 2; stride > 0; stride /= 2) {
    barrier();

    /* Only the threads up to the current stride should be active as can be seen in the diagram
     * above. */
    if (gl_LocalInvocationIndex >= stride) {
      continue;
    }

    /* Reduce each two elements that are stride apart, writing the result to the element with the
     * lower index, as can be seen in the diagram above. The developer is expected to define the
     * REDUCE macro to be a commutative and associative binary operator suitable for parallel
     * reduction. */
    reduction_data[gl_LocalInvocationIndex] = REDUCE(
        reduction_data[gl_LocalInvocationIndex], reduction_data[gl_LocalInvocationIndex + stride]);
  }

  /* Finally, the result of the reduction is available as the first element in the reduction data,
   * write it to the pixel corresponding to the work group, making sure only the one thread writes
   * it. */
  barrier();
  if (gl_LocalInvocationIndex == 0) {
    imageStore(output_img, ivec2(gl_WorkGroupID.xy), vec4(reduction_data[0]));
  }
}
