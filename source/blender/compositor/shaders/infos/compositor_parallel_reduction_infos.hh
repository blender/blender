/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifdef GPU_SHADER
#  include "gpu_shader_compat.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_parallel_reduction_shared)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(bool, is_initial_reduction)
SAMPLER(0, sampler2D, input_tx)
COMPUTE_SOURCE("compositor_parallel_reduction.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_luminance_shared)
PUSH_CONSTANT(float3, luminance_coefficients)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_parallel_reduction_float_shared)
GROUP_SHARED(float, reduction_data[gl_WorkGroupSize.x * gl_WorkGroupSize.y])
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_parallel_reduction_float2_shared)
GROUP_SHARED(float2, reduction_data[gl_WorkGroupSize.x * gl_WorkGroupSize.y])
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_parallel_reduction_float4_shared)
GROUP_SHARED(float4, reduction_data[gl_WorkGroupSize.x * gl_WorkGroupSize.y])
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_parallel_reduction_half4_shared)
GROUP_SHARED(float4, reduction_data[gl_WorkGroupSize.x * gl_WorkGroupSize.y])
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_parallel_reduction_output_float)
IMAGE(0, SFLOAT_32, write, image2D, output_img)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_parallel_reduction_output_float2)
IMAGE(0, SFLOAT_32_32, write, image2D, output_img)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_parallel_reduction_output_float4)
IMAGE(0, SFLOAT_32_32_32_32, write, image2D, output_img)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_parallel_reduction_output_half4)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Sum Reductions.
 */

GPU_SHADER_CREATE_INFO(compositor_sum_float_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_float_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_output_float)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_sum_red)
COMPUTE_FUNCTION("reduce_sum_red")
ADDITIONAL_INFO(compositor_sum_float_shared)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_sum_green)
COMPUTE_FUNCTION("reduce_sum_green")
ADDITIONAL_INFO(compositor_sum_float_shared)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_sum_blue)
COMPUTE_FUNCTION("reduce_sum_blue")
ADDITIONAL_INFO(compositor_sum_float_shared)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_sum_luminance)
COMPUTE_FUNCTION("reduce_sum_luminance")
ADDITIONAL_INFO(compositor_sum_float_shared)
ADDITIONAL_INFO(compositor_luminance_shared)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_sum_log_luminance)
COMPUTE_FUNCTION("reduce_sum_log_luminance")
ADDITIONAL_INFO(compositor_sum_float_shared)
ADDITIONAL_INFO(compositor_luminance_shared)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_sum_color)
COMPUTE_FUNCTION("reduce_sum_color")
ADDITIONAL_INFO(compositor_parallel_reduction_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_output_float4)
GROUP_SHARED(float4, reduction_data[gl_WorkGroupSize.x * gl_WorkGroupSize.y])
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Sum Of Squared Difference Reductions.
 */

GPU_SHADER_CREATE_INFO(compositor_sum_squared_difference_float_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_float_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_output_float)
PUSH_CONSTANT(float, subtrahend)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_sum_red_squared_difference)
COMPUTE_FUNCTION("reduce_sum_red_squared_difference")
ADDITIONAL_INFO(compositor_sum_squared_difference_float_shared)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_sum_green_squared_difference)
COMPUTE_FUNCTION("reduce_sum_green_squared_difference")
ADDITIONAL_INFO(compositor_sum_squared_difference_float_shared)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_sum_blue_squared_difference)
COMPUTE_FUNCTION("reduce_sum_blue_squared_difference")
ADDITIONAL_INFO(compositor_sum_squared_difference_float_shared)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_sum_luminance_squared_difference)
COMPUTE_FUNCTION("reduce_sum_luminance_squared_difference")
ADDITIONAL_INFO(compositor_sum_squared_difference_float_shared)
ADDITIONAL_INFO(compositor_luminance_shared)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Maximum Reductions.
 */

GPU_SHADER_CREATE_INFO(compositor_maximum_luminance)
COMPUTE_FUNCTION("reduce_maximum_luminance")
ADDITIONAL_INFO(compositor_parallel_reduction_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_float_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_output_float)
ADDITIONAL_INFO(compositor_luminance_shared)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_maximum_brightness)
COMPUTE_FUNCTION("reduce_maximum_brightness")
ADDITIONAL_INFO(compositor_parallel_reduction_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_float_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_output_float)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_maximum_float)
COMPUTE_FUNCTION("reduce_maximum_float")
ADDITIONAL_INFO(compositor_parallel_reduction_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_float_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_output_float)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_maximum_float2)
COMPUTE_FUNCTION("reduce_maximum_float2")
ADDITIONAL_INFO(compositor_parallel_reduction_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_float2_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_output_float2)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_maximum_float_in_range)
COMPUTE_FUNCTION("reduce_maximum_float_in_range")
ADDITIONAL_INFO(compositor_parallel_reduction_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_float_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_output_float)
PUSH_CONSTANT(float, lower_bound)
PUSH_CONSTANT(float, upper_bound)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Minimum Reductions.
 */

GPU_SHADER_CREATE_INFO(compositor_minimum_luminance)
COMPUTE_FUNCTION("reduce_minimum_luminance")
ADDITIONAL_INFO(compositor_parallel_reduction_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_float_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_output_float)
ADDITIONAL_INFO(compositor_luminance_shared)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_minimum_float)
COMPUTE_FUNCTION("reduce_minimum_float")
ADDITIONAL_INFO(compositor_parallel_reduction_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_float_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_output_float)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_minimum_float_in_range)
COMPUTE_FUNCTION("reduce_minimum_float_in_range")
ADDITIONAL_INFO(compositor_parallel_reduction_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_float_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_output_float)
PUSH_CONSTANT(float, lower_bound)
PUSH_CONSTANT(float, upper_bound)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Velocity Reductions.
 */

GPU_SHADER_CREATE_INFO(compositor_max_velocity)
ADDITIONAL_INFO(compositor_parallel_reduction_float4_shared)
ADDITIONAL_INFO(compositor_parallel_reduction_output_half4)
COMPUTE_FUNCTION("reduce_max_velocity")
LOCAL_GROUP_SIZE(32, 32)
PUSH_CONSTANT(bool, is_initial_reduction)
SAMPLER(0, sampler2D, input_tx)
COMPUTE_SOURCE("compositor_parallel_reduction.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
