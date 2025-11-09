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
 * TYPE, IDENTITY, INITIALIZE, LOAD, REDUCE, and WRITE. See the implementation below for more
 * information as well as the compositor_parallel_reduction_infos.hh for example reductions
 * operations. */

/* Doing the reduction in shared memory is faster, so create a shared array where the whole data
 * of the work group will be loaded and reduced. The 2D structure of the work group is irrelevant
 * for reduction, so we just load the data in a 1D array to simplify reduction. The developer is
 * expected to define the TYPE macro to be a float or a vec4, depending on the type of data being
 * reduced. */

#include "infos/compositor_parallel_reduction_infos.hh"

COMPUTE_SHADER_CREATE_INFO(compositor_parallel_reduction_shared)
COMPUTE_SHADER_CREATE_INFO(compositor_parallel_reduction_output_float4)

#include "gpu_shader_compositor_texture_utilities.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_math_vector_reduce_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

#define reduction_size (gl_WorkGroupSize.x * gl_WorkGroupSize.y)

/* Operations */

template<typename T> struct Min {
  static T identity()
  {
    return T(0);
  }

  static T initialize(T value)
  {
    return value;
  }

  static T reduce(T lhs, T rhs)
  {
    return min(lhs, rhs);
  }
};
template struct Min<float>;

template<typename T> struct Max {
  static T identity()
  {
    return T(0);
  }

  static T initialize(T value)
  {
    return value;
  }

  static T reduce(T lhs, T rhs)
  {
    return max(lhs, rhs);
  }
};
template struct Max<float>;
template struct Max<float2>;
template struct Max<float4>;

template<typename T> struct Sum {
  static T identity()
  {
    return T(0);
  }

  static T initialize(T value)
  {
    return value;
  }

  static T reduce(T lhs, T rhs)
  {
    return lhs + rhs;
  }
};
template struct Sum<float>;
template struct Sum<float4>;

template<typename T> struct MaxVelocity {
  static T identity()
  {
    return T(0);
  }

  static T initialize(T value)
  {
    return value;
  }

  static T reduce(T lhs, T rhs)
  {
    return float4(dot(lhs.xy, lhs.xy) > dot(rhs.xy, rhs.xy) ? lhs.xy : rhs.xy,
                  dot(lhs.zw, lhs.zw) > dot(rhs.zw, rhs.zw) ? lhs.zw : rhs.zw);
  }
};
template struct MaxVelocity<float4>;

template<typename T> struct MaxInRange {
  static T identity()
  {
    return T(0);
  }

  static T initialize(T value)
  {
    float max = push_constant_get(compositor_maximum_float_in_range, upper_bound);
    float min = push_constant_get(compositor_maximum_float_in_range, lower_bound);
    return ((value <= max) && (value >= min)) ? value : min;
  }

  static T reduce(T lhs, T rhs)
  {
    float max = push_constant_get(compositor_maximum_float_in_range, upper_bound);
    return ((rhs > lhs) && (rhs <= max)) ? rhs : lhs;
  }
};
template struct MaxInRange<float>;

template<typename T> struct MinInRange {
  static T identity()
  {
    return T(0);
  }

  static T initialize(T value)
  {
    float max = push_constant_get(compositor_minimum_float_in_range, upper_bound);
    float min = push_constant_get(compositor_minimum_float_in_range, lower_bound);
    return ((value <= max) && (value >= min)) ? value : max;
  }

  static T reduce(T lhs, T rhs)
  {
    float min = push_constant_get(compositor_minimum_float_in_range, lower_bound);
    return ((rhs < lhs) && (rhs >= min)) ? rhs : lhs;
  }
};
template struct MinInRange<float>;

template<typename T> struct SumSquareDifference {
  static T identity()
  {
    return T(0);
  }

  static T initialize(T value)
  {
    float sub = push_constant_get(compositor_sum_squared_difference_float_shared, subtrahend);
    return square(value - sub);
  }

  static T reduce(T lhs, T rhs)
  {
    return lhs + rhs;
  }
};
template struct SumSquareDifference<float>;

/* ChannelMix */
struct ChannelR {};
struct ChannelG {};
struct ChannelB {};
struct ChannelRG {};
struct ChannelRGBA {};
struct ChannelLuma {};
struct ChannelLogLuma {};
struct ChannelMax {};

template<typename T, typename ChannelMix> T channel_mix(float4 value)
{
  return value;
}
template float4 channel_mix<float4, ChannelRGBA>(float4);
/* clang-format off */
template<> float channel_mix<float, ChannelR>(float4 value) { return value.r; }
template<> float channel_mix<float, ChannelG>(float4 value) { return value.g; }
template<> float channel_mix<float, ChannelB>(float4 value) { return value.b; }
template<> float channel_mix<float, ChannelMax>(float4 value) { return reduce_max(value.rgb); }
template<> float2 channel_mix<float2, ChannelRG>(float4 value) { return value.rg; }
/* clang-format on */
template<> float channel_mix<float, ChannelLuma>(float4 value)
{
  float3 coefficients = push_constant_get(compositor_luminance_shared, luminance_coefficients);
  return dot(value.rgb, coefficients);
}
template<> float channel_mix<float, ChannelLogLuma>(float4 value)
{
  float3 coefficients = push_constant_get(compositor_luminance_shared, luminance_coefficients);
  return log(max(dot(value.rgb, coefficients), 1e-5f));
}

/* clang-format off */
template<typename T> T load(float4 value) { return value; }
template float4 load<float4>(float4);
template<> float load<float>(float4 value) { return value.x; }
template<> float2 load<float2>(float4 value) { return value.xy; }

float4 to_float4(float value) { return float4(value); }
float4 to_float4(float2 value) { return value.xyyy; }
float4 to_float4(float4 value) { return value; }
/* clang-format on */

void load_shared_data(uint index, float &r_data)
{
  r_data = shared_variable_get(compositor_parallel_reduction_float_shared, reduction_data)[index];
}
void load_shared_data(uint index, float2 &r_data)
{
  r_data = shared_variable_get(compositor_parallel_reduction_float2_shared, reduction_data)[index];
}
void load_shared_data(uint index, float4 &r_data)
{
  r_data = shared_variable_get(compositor_parallel_reduction_float4_shared, reduction_data)[index];
}

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

template<typename T, typename Operation, typename ChannelMix> void reduction()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  /* Initialize the shared array for out of bound invocations using the `IDENTITY` value. The
   * developer is expected to define the `IDENTITY` macro to be a value of type `TYPE` that does
   * not affect the output of the reduction. For instance, sum reductions have an identity of 0.0,
   * while max value reductions have an identity of FLT_MIN */
  if (any(lessThan(texel, int2(0))) || any(greaterThanEqual(texel, texture_size(input_tx)))) {
    store_shared_data(gl_LocalInvocationIndex, Operation::identity());
  }
  else {
    float4 value = texture_load_unbound(input_tx, texel);

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
    T data = is_initial_reduction ? Operation::initialize(channel_mix<T, ChannelMix>(value)) :
                                    load<T>(value);
    store_shared_data(gl_LocalInvocationIndex, data);
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
    T lhs, rhs;
    load_shared_data(gl_LocalInvocationIndex, lhs);
    load_shared_data(gl_LocalInvocationIndex + stride, rhs);
    T result = Operation::reduce(lhs, rhs);
    store_shared_data(gl_LocalInvocationIndex, result);
  }

  /* Finally, the result of the reduction is available as the first element in the reduction data,
   * write it to the pixel corresponding to the work group, making sure only the one thread writes
   * it. */
  barrier();
  if (gl_LocalInvocationIndex == 0) {
    T data;
    load_shared_data(0, data);
    imageStore(output_img, int2(gl_WorkGroupID.xy), to_float4(data));
  }
}

template void reduction<float, Min<float>, ChannelLuma>();
template void reduction<float, Min<float>, ChannelR>();

template void reduction<float, Max<float>, ChannelLuma>();
template void reduction<float, Max<float>, ChannelMax>();
template void reduction<float, Max<float>, ChannelR>();
template void reduction<float2, Max<float2>, ChannelRG>();

template void reduction<float, Sum<float>, ChannelR>();
template void reduction<float, Sum<float>, ChannelG>();
template void reduction<float, Sum<float>, ChannelB>();
template void reduction<float, Sum<float>, ChannelLuma>();
template void reduction<float, Sum<float>, ChannelLogLuma>();
template void reduction<float4, Sum<float4>, ChannelRGBA>();

template void reduction<float, SumSquareDifference<float>, ChannelR>();
template void reduction<float, SumSquareDifference<float>, ChannelG>();
template void reduction<float, SumSquareDifference<float>, ChannelB>();
template void reduction<float, SumSquareDifference<float>, ChannelLuma>();

template void reduction<float, MaxInRange<float>, ChannelR>();

template void reduction<float, MinInRange<float>, ChannelR>();

template void reduction<float4, MaxVelocity<float4>, ChannelRGBA>();

void reduce_sum_red()
{
  reduction<float, Sum<float>, ChannelR>();
}
void reduce_sum_green()
{
  reduction<float, Sum<float>, ChannelG>();
}
void reduce_sum_blue()
{
  reduction<float, Sum<float>, ChannelB>();
}
void reduce_sum_luminance()
{
  reduction<float, Sum<float>, ChannelLuma>();
}
void reduce_sum_log_luminance()
{
  reduction<float, Sum<float>, ChannelLogLuma>();
}
void reduce_sum_color()
{
  reduction<float4, Sum<float4>, ChannelRGBA>();
}

void reduce_sum_red_squared_difference()
{
  reduction<float, SumSquareDifference<float>, ChannelR>();
}
void reduce_sum_green_squared_difference()
{
  reduction<float, SumSquareDifference<float>, ChannelG>();
}
void reduce_sum_blue_squared_difference()
{
  reduction<float, SumSquareDifference<float>, ChannelB>();
}
void reduce_sum_luminance_squared_difference()
{
  reduction<float, SumSquareDifference<float>, ChannelLuma>();
}

void reduce_maximum_luminance()
{
  reduction<float, Max<float>, ChannelLuma>();
}
void reduce_maximum_brightness()
{
  reduction<float, Max<float>, ChannelMax>();
}
void reduce_maximum_float()
{
  reduction<float, Max<float>, ChannelR>();
}
void reduce_maximum_float2()
{
  reduction<float2, Max<float2>, ChannelRG>();
}

void reduce_maximum_float_in_range()
{
  reduction<float, MaxInRange<float>, ChannelR>();
}

void reduce_minimum_luminance()
{
  reduction<float, Min<float>, ChannelLuma>();
}
void reduce_minimum_float()
{
  reduction<float, Min<float>, ChannelR>();
}

void reduce_minimum_float_in_range()
{
  reduction<float, MinInRange<float>, ChannelR>();
}

void reduce_max_velocity()
{
  reduction<float4, MaxVelocity<float4>, ChannelRGBA>();
}
