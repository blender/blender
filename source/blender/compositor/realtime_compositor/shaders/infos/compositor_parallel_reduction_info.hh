/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_parallel_reduction_shared)
    .local_group_size(16, 16)
    .push_constant(Type::BOOL, "is_initial_reduction")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .compute_source("compositor_parallel_reduction.glsl");

/* --------------------------------------------------------------------
 * Sum Reductions.
 */

GPU_SHADER_CREATE_INFO(compositor_sum_shared)
    .additional_info("compositor_parallel_reduction_shared")
    .define("REDUCE(lhs, rhs)", "lhs + rhs");

GPU_SHADER_CREATE_INFO(compositor_sum_float_shared)
    .additional_info("compositor_sum_shared")
    .image(0, GPU_R32F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .define("TYPE", "float")
    .define("IDENTITY", "0.0")
    .define("LOAD(value)", "value.x");

GPU_SHADER_CREATE_INFO(compositor_sum_red)
    .additional_info("compositor_sum_float_shared")
    .define("INITIALIZE(value)", "value.r")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_sum_green)
    .additional_info("compositor_sum_float_shared")
    .define("INITIALIZE(value)", "value.g")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_sum_blue)
    .additional_info("compositor_sum_float_shared")
    .define("INITIALIZE(value)", "value.b")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_sum_luminance)
    .additional_info("compositor_sum_float_shared")
    .push_constant(Type::VEC3, "luminance_coefficients")
    .define("INITIALIZE(value)", "dot(value.rgb, luminance_coefficients)")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_sum_log_luminance)
    .additional_info("compositor_sum_float_shared")
    .push_constant(Type::VEC3, "luminance_coefficients")
    .define("INITIALIZE(value)", "log(max(dot(value.rgb, luminance_coefficients), 1e-5))")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_sum_color)
    .additional_info("compositor_sum_shared")
    .image(0, GPU_RGBA32F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .define("TYPE", "vec4")
    .define("IDENTITY", "vec4(0.0)")
    .define("INITIALIZE(value)", "value")
    .define("LOAD(value)", "value")
    .do_static_compilation(true);

/* --------------------------------------------------------------------
 * Sum Of Squared Difference Reductions.
 */

GPU_SHADER_CREATE_INFO(compositor_sum_squared_difference_float_shared)
    .additional_info("compositor_parallel_reduction_shared")
    .image(0, GPU_R32F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .push_constant(Type::FLOAT, "subtrahend")
    .define("TYPE", "float")
    .define("IDENTITY", "0.0")
    .define("LOAD(value)", "value.x")
    .define("REDUCE(lhs, rhs)", "lhs + rhs");

GPU_SHADER_CREATE_INFO(compositor_sum_red_squared_difference)
    .additional_info("compositor_sum_squared_difference_float_shared")
    .define("INITIALIZE(value)", "pow(value.r - subtrahend, 2.0)")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_sum_green_squared_difference)
    .additional_info("compositor_sum_squared_difference_float_shared")
    .define("INITIALIZE(value)", "pow(value.g - subtrahend, 2.0)")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_sum_blue_squared_difference)
    .additional_info("compositor_sum_squared_difference_float_shared")
    .define("INITIALIZE(value)", "pow(value.b - subtrahend, 2.0)")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_sum_luminance_squared_difference)
    .additional_info("compositor_sum_squared_difference_float_shared")
    .push_constant(Type::VEC3, "luminance_coefficients")
    .define("INITIALIZE(value)", "pow(dot(value.rgb, luminance_coefficients) - subtrahend, 2.0)")
    .do_static_compilation(true);

/* --------------------------------------------------------------------
 * Maximum Reductions.
 */

GPU_SHADER_CREATE_INFO(compositor_maximum_luminance)
    .additional_info("compositor_parallel_reduction_shared")
    .typedef_source("common_math_lib.glsl")
    .image(0, GPU_R32F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .push_constant(Type::VEC3, "luminance_coefficients")
    .define("TYPE", "float")
    .define("IDENTITY", "FLT_MIN")
    .define("INITIALIZE(value)", "dot(value.rgb, luminance_coefficients)")
    .define("LOAD(value)", "value.x")
    .define("REDUCE(lhs, rhs)", "max(lhs, rhs)")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_maximum_float_in_range)
    .additional_info("compositor_parallel_reduction_shared")
    .image(0, GPU_R32F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .push_constant(Type::FLOAT, "lower_bound")
    .push_constant(Type::FLOAT, "upper_bound")
    .define("TYPE", "float")
    .define("IDENTITY", "lower_bound")
    .define("INITIALIZE(v)", "((v.x <= upper_bound) && (v.x >= lower_bound)) ? v.x : lower_bound")
    .define("LOAD(value)", "value.x")
    .define("REDUCE(lhs, rhs)", "((rhs > lhs) && (rhs <= upper_bound)) ? rhs : lhs")
    .do_static_compilation(true);

/* --------------------------------------------------------------------
 * Minimum Reductions.
 */

GPU_SHADER_CREATE_INFO(compositor_minimum_luminance)
    .additional_info("compositor_parallel_reduction_shared")
    .typedef_source("common_math_lib.glsl")
    .image(0, GPU_R32F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .push_constant(Type::VEC3, "luminance_coefficients")
    .define("TYPE", "float")
    .define("IDENTITY", "FLT_MAX")
    .define("INITIALIZE(value)", "dot(value.rgb, luminance_coefficients)")
    .define("LOAD(value)", "value.x")
    .define("REDUCE(lhs, rhs)", "min(lhs, rhs)")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_minimum_float_in_range)
    .additional_info("compositor_parallel_reduction_shared")
    .image(0, GPU_R32F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .push_constant(Type::FLOAT, "lower_bound")
    .push_constant(Type::FLOAT, "upper_bound")
    .define("TYPE", "float")
    .define("IDENTITY", "upper_bound")
    .define("INITIALIZE(v)", "((v.x <= upper_bound) && (v.x >= lower_bound)) ? v.x : upper_bound")
    .define("LOAD(value)", "value.x")
    .define("REDUCE(lhs, rhs)", "((rhs < lhs) && (rhs >= lower_bound)) ? rhs : lhs")
    .do_static_compilation(true);

/* --------------------------------------------------------------------
 * Velocity Reductions.
 */

GPU_SHADER_CREATE_INFO(compositor_max_velocity)
    .local_group_size(32, 32)
    .push_constant(Type::BOOL, "is_initial_reduction")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .define("TYPE", "vec4")
    .define("IDENTITY", "vec4(0.0)")
    .define("INITIALIZE(value)", "value")
    .define("LOAD(value)", "value")
    .define("REDUCE(lhs, rhs)",
            "vec4(dot(lhs.xy, lhs.xy) > dot(rhs.xy, rhs.xy) ? lhs.xy : rhs.xy,"
            "     dot(lhs.zw, lhs.zw) > dot(rhs.zw, rhs.zw) ? lhs.zw : rhs.zw)")
    .compute_source("compositor_parallel_reduction.glsl")
    .do_static_compilation(true);
