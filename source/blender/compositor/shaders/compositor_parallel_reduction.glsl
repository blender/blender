/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/compositor_parallel_reduction_infos.hh"

COMPUTE_SHADER_CREATE_INFO(compositor_parallel_reduction_shared)
COMPUTE_SHADER_CREATE_INFO(compositor_parallel_reduction_output_float4)

#include "gpu_shader_compositor_texture_utilities.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

/* Texture Loading Utilities.
 *
 * Utility functions to load the appropriate type from the float4 returned by the generic texture
 * loading function. This is just a swizzle in most cases. */
template<typename T> T load(float4 value)
{
  return value;
}

template<> float load<float>(float4 value)
{
  return value.x;
}

template<> float2 load<float2>(float4 value)
{
  return value.xy;
}

template<> float4 load<float4>(float4 value)
{
  return value;
}

/* Image Storing Utilities.
 *
 * Utility functions to convert the input value into a float4 value since this is what the image
 * store functions expect. */
float4 to_storage_value(float value)
{
  return float4(value);
}

float4 to_storage_value(float2 value)
{
  return value.xyyy;
}

float4 to_storage_value(float4 value)
{
  return value;
}

/* Group Shard Data Loading Utilities.
 *
 * Utility functions to load the element with the given index from the group shared data. */
template<typename T> T load_shared_data(uint index)
{
  return T(0);
}

template<> float load_shared_data<float>(uint index)
{
  return shared_variable_get(compositor_parallel_reduction_float_shared, reduction_data)[index];
}

template<> float2 load_shared_data<float2>(uint index)
{
  return shared_variable_get(compositor_parallel_reduction_float2_shared, reduction_data)[index];
}

template<> float4 load_shared_data<float4>(uint index)
{
  return shared_variable_get(compositor_parallel_reduction_float4_shared, reduction_data)[index];
}

/* Group Shard Data Storing Utilities.
 *
 * Utility functions to store an element in the group shared data at the given index. */
void store_shared_data(uint index, float data)
{
  shared_variable_get(compositor_parallel_reduction_float_shared, reduction_data)[index] = data;
}

void store_shared_data(uint index, float2 data)
{
  shared_variable_get(compositor_parallel_reduction_float2_shared, reduction_data)[index] = data;
}

void store_shared_data(uint index, float4 data)
{
  shared_variable_get(compositor_parallel_reduction_float4_shared, reduction_data)[index] = data;
}

/* Identity Functions.
 *
 * This should return the value of the type that does not affect the output of the reduction. For
 * instance, sum reductions have an identity of 0, while max value reductions have an identity of
 * the minimum possible float value. */
struct IdentityZero {};
struct IdentityMinimumFloat {};
struct IdentityMaximumFloat {};
struct IdentityLowerBound {};
struct IdentityUpperBound {};

template<typename T, typename IdentityValue> T identity()
{
  return T(0);
}

template<> float identity<float, IdentityZero>()
{
  return 0.0f;
}

template<> float4 identity<float4, IdentityZero>()
{
  return float4(0.0f);
}

template<> float identity<float, IdentityMinimumFloat>()
{
  return -FLT_MAX;
}

template<> float2 identity<float2, IdentityMinimumFloat>()
{
  return float2(-FLT_MAX);
}

template<> float identity<float, IdentityMaximumFloat>()
{
  return FLT_MAX;
}

template<> float identity<float, IdentityLowerBound>()
{
  return push_constant_get(compositor_maximum_float_in_range, lower_bound);
}

template<> float identity<float, IdentityUpperBound>()
{
  return push_constant_get(compositor_minimum_float_in_range, upper_bound);
}

/* Initialize Functions.
 *
 * This should compute the value that should be reduced from the loaded value. For instance, a sum
 * reduction would simply return the value while a sum squared difference reduction would compute
 * square the difference from a subtrahend. */
struct InitializeDefault {};
struct InitializeSquaredDifference {};
struct InitializeMinimumInRange {};
struct InitializeMaximumInRange {};
struct InitializeLuminance {};
struct InitializeLogLuminance {};

template<typename T, typename InitializeFunction> T initialize(float4 value)
{
  return value;
}

template<> float initialize<float, InitializeDefault>(float4 value)
{
  return value.x;
}

template<> float2 initialize<float2, InitializeDefault>(float4 value)
{
  return value.xy;
}

template<> float4 initialize<float4, InitializeDefault>(float4 value)
{
  return value;
}

template<> float4 initialize<float4, InitializeSquaredDifference>(float4 value)
{
  float4 sub = push_constant_get(compositor_sum_squared_difference_color, subtrahend);
  return square(value - sub);
}

template<> float initialize<float, InitializeMinimumInRange>(float4 value)
{
  const float max = push_constant_get(compositor_minimum_float_in_range, upper_bound);
  const float min = push_constant_get(compositor_minimum_float_in_range, lower_bound);
  return ((value.x <= max) && (value.x >= min)) ? value.x : max;
}

template<> float initialize<float, InitializeMaximumInRange>(float4 value)
{
  float max = push_constant_get(compositor_maximum_float_in_range, upper_bound);
  float min = push_constant_get(compositor_maximum_float_in_range, lower_bound);
  return ((value.x <= max) && (value.x >= min)) ? value.x : min;
}

template<> float initialize<float, InitializeLuminance>(float4 value)
{
  float3 coefficients = push_constant_get(compositor_parallel_reduction_luminance_shared,
                                          luminance_coefficients);
  return dot(value.rgb, coefficients);
}

template<> float initialize<float, InitializeLogLuminance>(float4 value)
{
  float3 coefficients = push_constant_get(compositor_parallel_reduction_luminance_shared,
                                          luminance_coefficients);
  return log(max(dot(value.rgb, coefficients), 1e-5f));
}

/* Reduce Functions.
 *
 * This should be a commutative and associative binary operator suitable for parallel reduction. */
struct ReduceSum {};
struct ReduceMinimum {};
struct ReduceMaximum {};
struct ReduceMinimumInRange {};
struct ReduceMaximumInRange {};
struct ReduceMaximumVelocity {};

template<typename T, typename ReduceFunction> T reduce(T lhs, T rhs)
{
  return lhs + rhs;
}

template<> float4 reduce<float4, ReduceSum>(float4 lhs, float4 rhs)
{
  return lhs + rhs;
}

template<> float reduce<float, ReduceSum>(float lhs, float rhs)
{
  return lhs + rhs;
}

template<> float reduce<float, ReduceMinimum>(float lhs, float rhs)
{
  return min(lhs, rhs);
}

template<> float reduce<float, ReduceMaximum>(float lhs, float rhs)
{
  return max(lhs, rhs);
}

template<> float2 reduce<float2, ReduceMaximum>(float2 lhs, float2 rhs)
{
  return max(lhs, rhs);
}

template<> float reduce<float, ReduceMinimumInRange>(float lhs, float rhs)
{
  float min = push_constant_get(compositor_minimum_float_in_range, lower_bound);
  return ((rhs < lhs) && (rhs >= min)) ? rhs : lhs;
}

template<> float reduce<float, ReduceMaximumInRange>(float lhs, float rhs)
{
  float max = push_constant_get(compositor_maximum_float_in_range, upper_bound);
  return ((rhs > lhs) && (rhs <= max)) ? rhs : lhs;
}

template<> float4 reduce<float4, ReduceMaximumVelocity>(float4 lhs, float4 rhs)
{
  return float4(dot(lhs.xy, lhs.xy) > dot(rhs.xy, rhs.xy) ? lhs.xy : rhs.xy,
                dot(lhs.zw, lhs.zw) > dot(rhs.zw, rhs.zw) ? lhs.zw : rhs.zw);
}

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
 * The shader is generic enough to implement many types of reductions. This is done by using
 * templating as can be seen in the implementation and the above templates.
 *
 * Doing the reduction in shared memory is faster, so a shared array is used where the whole data
 * of the work group will be loaded and reduced. The 2D structure of the work group is irrelevant
 * for reduction, so we just load the data in a 1D array to simplify reduction. */
template<typename T, typename IdentityValue, typename InitializeFunction, typename ReduceFunction>
void reduction()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  /* Initialize the shared array for out of bound invocations using the identity value of the
   * operation. */
  if (any(lessThan(texel, int2(0))) || any(greaterThanEqual(texel, texture_size(input_tx)))) {
    store_shared_data(gl_LocalInvocationIndex, identity<T, IdentityValue>());
  }
  else {
    float4 value = texture_load_unbound(input_tx, texel);

    /* Initialize the shared array given the previously loaded value. This step can be different
     * depending on whether this is the initial reduction pass or a latter one. Indeed, the input
     * texture for the initial reduction is the source texture itself, while the input texture to a
     * latter reduction pass is an intermediate texture after one or more reductions have happened.
     * This is significant because the data being reduced might be computed from the original data
     * and different from it, for instance, when summing the luminance of an image, the original
     * data is a float4 color, while the reduced data is a float luminance value. So for the
     * initial reduction pass, the luminance will be computed from the color, reduced, then stored
     * into an intermediate float texture. On the other hand, for latter reduction passes, the
     * luminance will be loaded directly and reduced without extra processing. */
    T data = is_initial_reduction ? initialize<T, InitializeFunction>(value) : load<T>(value);
    store_shared_data(gl_LocalInvocationIndex, data);
  }

  /* Reduce the reduction data by half on every iteration until only one element remains. See the
   * above figure for an intuitive understanding of the stride value. */
  constexpr uint reduction_size = gl_WorkGroupSize.x * gl_WorkGroupSize.y;
  for (uint stride = reduction_size / 2; stride > 0; stride /= 2) {
    barrier();

    /* Only the threads up to the current stride should be active as can be seen in the diagram
     * above. */
    if (gl_LocalInvocationIndex >= stride) {
      continue;
    }

    /* Reduce each two elements that are stride apart, writing the result to the element with the
     * lower index, as can be seen in the diagram above.  */
    T result = reduce<T, ReduceFunction>(load_shared_data<T>(gl_LocalInvocationIndex),
                                         load_shared_data<T>(gl_LocalInvocationIndex + stride));
    store_shared_data(gl_LocalInvocationIndex, result);
  }

  /* Finally, the result of the reduction is available as the first element in the reduction data,
   * write it to the pixel corresponding to the work group, making sure only the one thread writes
   * it. */
  barrier();
  if (gl_LocalInvocationIndex == 0) {
    imageStore(output_img, int2(gl_WorkGroupID.xy), to_storage_value(load_shared_data<T>(0)));
  }
}

template void reduction<float4, IdentityZero, InitializeDefault, ReduceSum>();
void reduce_sum_color()
{
  reduction<float4, IdentityZero, InitializeDefault, ReduceSum>();
}

template void reduction<float, IdentityZero, InitializeLogLuminance, ReduceSum>();
void reduce_sum_log_luminance()
{
  reduction<float, IdentityZero, InitializeLogLuminance, ReduceSum>();
}

template void reduction<float4, IdentityZero, InitializeSquaredDifference, ReduceSum>();
void reduce_sum_squared_difference_color()
{
  reduction<float4, IdentityZero, InitializeSquaredDifference, ReduceSum>();
}

template void reduction<float, IdentityMaximumFloat, InitializeDefault, ReduceMinimum>();
void reduce_minimum_float()
{
  reduction<float, IdentityMaximumFloat, InitializeDefault, ReduceMinimum>();
}

template void reduction<float, IdentityMaximumFloat, InitializeLuminance, ReduceMinimum>();
void reduce_minimum_luminance()
{
  reduction<float, IdentityMaximumFloat, InitializeLuminance, ReduceMinimum>();
}

template void reduction<float,
                        IdentityUpperBound,
                        InitializeMinimumInRange,
                        ReduceMinimumInRange>();
void reduce_minimum_float_in_range()
{
  reduction<float, IdentityUpperBound, InitializeMinimumInRange, ReduceMinimumInRange>();
}

template void reduction<float, IdentityMinimumFloat, InitializeDefault, ReduceMaximum>();
void reduce_maximum_float()
{
  reduction<float, IdentityMinimumFloat, InitializeDefault, ReduceMaximum>();
}

template void reduction<float2, IdentityMinimumFloat, InitializeDefault, ReduceMaximum>();
void reduce_maximum_float2()
{
  reduction<float2, IdentityMinimumFloat, InitializeDefault, ReduceMaximum>();
}

template void reduction<float, IdentityMinimumFloat, InitializeLuminance, ReduceMaximum>();
void reduce_maximum_luminance()
{
  reduction<float, IdentityMinimumFloat, InitializeLuminance, ReduceMaximum>();
}

template void reduction<float,
                        IdentityLowerBound,
                        InitializeMaximumInRange,
                        ReduceMaximumInRange>();
void reduce_maximum_float_in_range()
{
  reduction<float, IdentityLowerBound, InitializeMaximumInRange, ReduceMaximumInRange>();
}

template void reduction<float4, IdentityZero, InitializeDefault, ReduceMaximumVelocity>();
void reduce_max_velocity()
{
  reduction<float4, IdentityZero, InitializeDefault, ReduceMaximumVelocity>();
}
