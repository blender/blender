/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_parallel_reduction_shared)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(bool, is_initial_reduction)
SAMPLER(0, sampler2D, input_tx)
COMPUTE_SOURCE("compositor_parallel_reduction.glsl")
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Sum Reductions.
 */

GPU_SHADER_CREATE_INFO(compositor_sum_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_shared)
DEFINE_VALUE("REDUCE(lhs, rhs)", "lhs + rhs")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_sum_float_shared)
ADDITIONAL_INFO(compositor_sum_shared)
IMAGE(0, SFLOAT_32, write, image2D, output_img)
DEFINE_VALUE("TYPE", "float")
DEFINE_VALUE("IDENTITY", "0.0f")
DEFINE_VALUE("LOAD(value)", "value.x")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_sum_red)
ADDITIONAL_INFO(compositor_sum_float_shared)
DEFINE_VALUE("INITIALIZE(value)", "value.r")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_sum_green)
ADDITIONAL_INFO(compositor_sum_float_shared)
DEFINE_VALUE("INITIALIZE(value)", "value.g")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_sum_blue)
ADDITIONAL_INFO(compositor_sum_float_shared)
DEFINE_VALUE("INITIALIZE(value)", "value.b")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_sum_luminance)
ADDITIONAL_INFO(compositor_sum_float_shared)
PUSH_CONSTANT(float3, luminance_coefficients)
DEFINE_VALUE("INITIALIZE(value)", "dot(value.rgb, luminance_coefficients)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_sum_log_luminance)
ADDITIONAL_INFO(compositor_sum_float_shared)
PUSH_CONSTANT(float3, luminance_coefficients)
DEFINE_VALUE("INITIALIZE(value)", "log(max(dot(value.rgb, luminance_coefficients), 1e-5f))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_sum_color)
ADDITIONAL_INFO(compositor_sum_shared)
IMAGE(0, SFLOAT_32_32_32_32, write, image2D, output_img)
DEFINE_VALUE("TYPE", "vec4")
DEFINE_VALUE("IDENTITY", "vec4(0.0f)")
DEFINE_VALUE("INITIALIZE(value)", "value")
DEFINE_VALUE("LOAD(value)", "value")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Sum Of Squared Difference Reductions.
 */

GPU_SHADER_CREATE_INFO(compositor_sum_squared_difference_float_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_shared)
IMAGE(0, SFLOAT_32, write, image2D, output_img)
PUSH_CONSTANT(float, subtrahend)
DEFINE_VALUE("TYPE", "float")
DEFINE_VALUE("IDENTITY", "0.0f")
DEFINE_VALUE("LOAD(value)", "value.x")
DEFINE_VALUE("REDUCE(lhs, rhs)", "lhs + rhs")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_sum_red_squared_difference)
ADDITIONAL_INFO(compositor_sum_squared_difference_float_shared)
DEFINE_VALUE("INITIALIZE(value)", "pow(value.r - subtrahend, 2.0f)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_sum_green_squared_difference)
ADDITIONAL_INFO(compositor_sum_squared_difference_float_shared)
DEFINE_VALUE("INITIALIZE(value)", "pow(value.g - subtrahend, 2.0f)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_sum_blue_squared_difference)
ADDITIONAL_INFO(compositor_sum_squared_difference_float_shared)
DEFINE_VALUE("INITIALIZE(value)", "pow(value.b - subtrahend, 2.0f)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_sum_luminance_squared_difference)
ADDITIONAL_INFO(compositor_sum_squared_difference_float_shared)
PUSH_CONSTANT(float3, luminance_coefficients)
DEFINE_VALUE("INITIALIZE(value)", "pow(dot(value.rgb, luminance_coefficients) - subtrahend, 2.0f)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Maximum Reductions.
 */

GPU_SHADER_CREATE_INFO(compositor_maximum_luminance)
ADDITIONAL_INFO(compositor_parallel_reduction_shared)
IMAGE(0, SFLOAT_32, write, image2D, output_img)
PUSH_CONSTANT(float3, luminance_coefficients)
DEFINE_VALUE("TYPE", "float")
DEFINE_VALUE("IDENTITY", "FLT_MIN")
DEFINE_VALUE("INITIALIZE(value)", "dot(value.rgb, luminance_coefficients)")
DEFINE_VALUE("LOAD(value)", "value.x")
DEFINE_VALUE("REDUCE(lhs, rhs)", "max(lhs, rhs)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_maximum_brightness)
ADDITIONAL_INFO(compositor_parallel_reduction_shared)
IMAGE(0, SFLOAT_32, write, image2D, output_img)
DEFINE_VALUE("TYPE", "float")
DEFINE_VALUE("IDENTITY", "FLT_MIN")
DEFINE_VALUE("INITIALIZE(value)", "reduce_max(value.rgb)")
DEFINE_VALUE("LOAD(value)", "value.x")
DEFINE_VALUE("REDUCE(lhs, rhs)", "max(lhs, rhs)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_maximum_float)
ADDITIONAL_INFO(compositor_parallel_reduction_shared)
IMAGE(0, SFLOAT_32, write, image2D, output_img)
DEFINE_VALUE("TYPE", "float")
DEFINE_VALUE("IDENTITY", "FLT_MIN")
DEFINE_VALUE("INITIALIZE(value)", "value.x")
DEFINE_VALUE("LOAD(value)", "value.x")
DEFINE_VALUE("REDUCE(lhs, rhs)", "max(rhs, lhs)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_maximum_float2)
ADDITIONAL_INFO(compositor_parallel_reduction_shared)
IMAGE(0, SFLOAT_32_32, write, image2D, output_img)
DEFINE_VALUE("TYPE", "vec2")
DEFINE_VALUE("IDENTITY", "vec2(FLT_MIN)")
DEFINE_VALUE("INITIALIZE(value)", "value.xy")
DEFINE_VALUE("LOAD(value)", "value.xy")
DEFINE_VALUE("REDUCE(lhs, rhs)", "max(rhs, lhs)")
DEFINE_VALUE("WRITE(value)", "vec4(value, vec2(0.0f))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_maximum_float_in_range)
ADDITIONAL_INFO(compositor_parallel_reduction_shared)
IMAGE(0, SFLOAT_32, write, image2D, output_img)
PUSH_CONSTANT(float, lower_bound)
PUSH_CONSTANT(float, upper_bound)
DEFINE_VALUE("TYPE", "float")
DEFINE_VALUE("IDENTITY", "lower_bound")
DEFINE_VALUE("INITIALIZE(v)", "((v.x <= upper_bound) && (v.x >= lower_bound)) ? v.x : lower_bound")
DEFINE_VALUE("LOAD(value)", "value.x")
DEFINE_VALUE("REDUCE(lhs, rhs)", "((rhs > lhs) && (rhs <= upper_bound)) ? rhs : lhs")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Minimum Reductions.
 */

GPU_SHADER_CREATE_INFO(compositor_minimum_luminance)
ADDITIONAL_INFO(compositor_parallel_reduction_shared)
IMAGE(0, SFLOAT_32, write, image2D, output_img)
PUSH_CONSTANT(float3, luminance_coefficients)
DEFINE_VALUE("TYPE", "float")
DEFINE_VALUE("IDENTITY", "FLT_MAX")
DEFINE_VALUE("INITIALIZE(value)", "dot(value.rgb, luminance_coefficients)")
DEFINE_VALUE("LOAD(value)", "value.x")
DEFINE_VALUE("REDUCE(lhs, rhs)", "min(lhs, rhs)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_minimum_float)
ADDITIONAL_INFO(compositor_parallel_reduction_shared)
IMAGE(0, SFLOAT_32, write, image2D, output_img)
DEFINE_VALUE("TYPE", "float")
DEFINE_VALUE("IDENTITY", "FLT_MAX")
DEFINE_VALUE("INITIALIZE(value)", "value.x")
DEFINE_VALUE("LOAD(value)", "value.x")
DEFINE_VALUE("REDUCE(lhs, rhs)", "min(rhs, lhs)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_minimum_float_in_range)
ADDITIONAL_INFO(compositor_parallel_reduction_shared)
IMAGE(0, SFLOAT_32, write, image2D, output_img)
PUSH_CONSTANT(float, lower_bound)
PUSH_CONSTANT(float, upper_bound)
DEFINE_VALUE("TYPE", "float")
DEFINE_VALUE("IDENTITY", "upper_bound")
DEFINE_VALUE("INITIALIZE(v)", "((v.x <= upper_bound) && (v.x >= lower_bound)) ? v.x : upper_bound")
DEFINE_VALUE("LOAD(value)", "value.x")
DEFINE_VALUE("REDUCE(lhs, rhs)", "((rhs < lhs) && (rhs >= lower_bound)) ? rhs : lhs")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Velocity Reductions.
 */

GPU_SHADER_CREATE_INFO(compositor_max_velocity)
LOCAL_GROUP_SIZE(32, 32)
PUSH_CONSTANT(bool, is_initial_reduction)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DEFINE_VALUE("TYPE", "vec4")
DEFINE_VALUE("IDENTITY", "vec4(0.0f)")
DEFINE_VALUE("INITIALIZE(value)", "value")
DEFINE_VALUE("LOAD(value)", "value")
DEFINE_VALUE("REDUCE(lhs, rhs)",
             "vec4(dot(lhs.xy, lhs.xy) > dot(rhs.xy, rhs.xy) ? lhs.xy : rhs.xy,"
             "     dot(lhs.zw, lhs.zw) > dot(rhs.zw, rhs.zw) ? lhs.zw : rhs.zw)")
COMPUTE_SOURCE("compositor_parallel_reduction.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
