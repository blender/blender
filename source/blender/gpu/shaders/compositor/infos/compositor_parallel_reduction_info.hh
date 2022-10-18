/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_parallel_reduction_shared)
    .local_group_size(16, 16)
    .push_constant(Type::BOOL, "is_initial_reduction")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .compute_source("compositor_parallel_reduction.glsl");

/* --------------------------------------------------------------------
 * Sum Reductions.
 */

GPU_SHADER_CREATE_INFO(compositor_sum_float_shared)
    .additional_info("compositor_parallel_reduction_shared")
    .image(0, GPU_R32F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .define("TYPE", "float")
    .define("IDENTITY", "vec4(0.0)")
    .define("LOAD(value)", "value.x")
    .define("REDUCE(lhs, rhs)", "lhs + rhs");

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

/* --------------------------------------------------------------------
 * Sum Of Squared Difference Reductions.
 */

GPU_SHADER_CREATE_INFO(compositor_sum_squared_difference_float_shared)
    .additional_info("compositor_parallel_reduction_shared")
    .image(0, GPU_R32F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .push_constant(Type::FLOAT, "subtrahend")
    .define("TYPE", "float")
    .define("IDENTITY", "vec4(subtrahend)")
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
